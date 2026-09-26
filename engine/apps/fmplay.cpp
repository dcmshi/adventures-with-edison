// fmplay: play or render FM sounds straight from the game's driver DLLs.
//
//   fmplay ADLIB.DLL                          list sounds
//   fmplay ADLIB.DLL CONDUCTOR SUNROCK1 ...   play sounds together (names or ids)
//   fmplay ADLIB.DLL ... --wav out.wav        render to a WAV file instead
//   options: --seconds N (limit), --tempo T (global tempo, 0-255, default 128)
//
// The global tempo drives Rock and Bach's band parts (SETAUTOTEMPO). The game
// sets it itself; the value it uses isn't decoded yet, so 128 is a stand-in.

// A plain main(): this is a console tool and only needs SDL's audio, so we
// skip SDL_main's replacement entry point.
#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "audio/artech_fm_driver.h"
#include "audio/fm_renderer.h"
#include "formats/codeview.h"
#include "formats/ne_file.h"

namespace {

constexpr uint32_t kSampleRate = 48000;

std::string upper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return s;
}

void writeLe(std::ofstream& out, uint32_t v, int bytes) {
    for (int i = 0; i < bytes; ++i) out.put(static_cast<char>((v >> (8 * i)) & 0xFF));
}

bool writeWav(const std::string& path, const std::vector<int16_t>& stereo) {
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    const uint32_t dataBytes = static_cast<uint32_t>(stereo.size() * sizeof(int16_t));
    out.write("RIFF", 4); writeLe(out, 36 + dataBytes, 4); out.write("WAVE", 4);
    out.write("fmt ", 4); writeLe(out, 16, 4); writeLe(out, 1, 2); writeLe(out, 2, 2);
    writeLe(out, kSampleRate, 4); writeLe(out, kSampleRate * 4, 4); writeLe(out, 4, 2); writeLe(out, 16, 2);
    out.write("data", 4); writeLe(out, dataBytes, 4);
    out.write(reinterpret_cast<const char*>(stereo.data()), dataBytes);
    return static_cast<bool>(out);
}

// Keeps rendering until the sounds have ended plus a short release tail.
struct Session {
    edison::ArtechFmDriver& driver;
    edison::FmRenderer& renderer;
    uint64_t maxFrames;
    uint64_t frames = 0;
    uint64_t tailFrames = kSampleRate / 2;  // let the last notes ring out

    bool done() const { return frames >= maxFrames || tailFrames == 0; }

    void render(int16_t* stereo, uint32_t count) {
        renderer.render(stereo, count);
        frames += count;
        if (driver.idle()) tailFrames = count >= tailFrames ? 0 : tailFrames - count;
    }
};

}  // namespace

int main(int argc, char** argv) {
    std::string dllPath, wavPath;
    std::vector<std::string> soundArgs;
    double seconds = 600;
    int tempo = 128;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--wav" && i + 1 < argc) wavPath = argv[++i];
        else if (a == "--seconds" && i + 1 < argc) seconds = std::atof(argv[++i]);
        else if (a == "--tempo" && i + 1 < argc) tempo = std::atoi(argv[++i]);
        else if (dllPath.empty()) dllPath = a;
        else soundArgs.push_back(a);
    }
    if (dllPath.empty()) {
        std::fprintf(stderr, "usage: fmplay <DLL> [sound ...] [--wav out.wav] [--seconds N] [--tempo T]\n");
        return 2;
    }

    // Heap-allocated: the driver holds two 64 KB memory images.
    auto driverPtr = std::make_unique<edison::ArtechFmDriver>();
    edison::ArtechFmDriver& driver = *driverPtr;
    std::string error;
    if (!driver.loadDll(dllPath, &error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }
    edison::NeFile ne;
    ne.load(dllPath);
    const auto symbols = edison::codeViewNameMap(ne.bytes());

    driver.init();
    std::map<std::string, uint16_t> byName;
    const auto list = driver.listSounds();
    for (uint16_t id : list.sounds) {
        auto it = symbols.find({2, driver.soundAddress(id)});
        if (it != symbols.end()) byName.emplace(upper(it->second), id);
    }

    if (soundArgs.empty()) {
        std::printf("%zu sounds in %s:\n", list.sounds.size(), dllPath.c_str());
        for (uint16_t id : list.sounds) {
            auto it = symbols.find({2, driver.soundAddress(id)});
            std::printf("  %3d  %-16s ch%d\n", id, it != symbols.end() ? it->second.c_str() : "",
                        driver.peek(driver.soundAddress(id)));
        }
        return 0;
    }

    std::vector<uint16_t> ids;
    for (const auto& s : soundArgs) {
        const bool numeric = std::all_of(s.begin(), s.end(), [](unsigned char c) { return std::isdigit(c); });
        if (numeric) {
            ids.push_back(static_cast<uint16_t>(std::atoi(s.c_str())));
        } else if (auto it = byName.find(upper(s)); it != byName.end()) {
            ids.push_back(it->second);
        } else {
            std::fprintf(stderr, "unknown sound '%s' (run without sounds to list them)\n", s.c_str());
            return 1;
        }
    }

    edison::FmRenderer renderer(driver, kSampleRate);
    driver.init();
    driver.setGlobalTempo(static_cast<uint8_t>(tempo));
    for (uint16_t id : ids) driver.sendSound(id);
    Session session{driver, renderer, static_cast<uint64_t>(seconds * kSampleRate)};

    if (!wavPath.empty()) {
        std::vector<int16_t> pcm;
        std::vector<int16_t> block(2 * 1024);
        while (!session.done()) {
            session.render(block.data(), 1024);
            pcm.insert(pcm.end(), block.begin(), block.end());
        }
        if (!writeWav(wavPath, pcm)) {
            std::fprintf(stderr, "cannot write %s\n", wavPath.c_str());
            return 1;
        }
        std::printf("wrote %s (%.1f s, %llu ticks)\n", wavPath.c_str(),
                    static_cast<double>(pcm.size() / 2) / kSampleRate,
                    static_cast<unsigned long long>(renderer.ticks()));
        return 0;
    }

    SDL_SetMainReady();
    if (!SDL_Init(SDL_INIT_AUDIO)) {
        std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    const SDL_AudioSpec spec{SDL_AUDIO_S16, 2, static_cast<int>(kSampleRate)};
    SDL_AudioStream* stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
    if (!stream) {
        std::fprintf(stderr, "SDL_OpenAudioDeviceStream: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_ResumeAudioStreamDevice(stream);
    std::printf("playing... (Ctrl+C to stop)\n");

    std::vector<int16_t> block(2 * 1024);
    const int queueTarget = static_cast<int>(kSampleRate / 10) * 4;  // ~100 ms buffered
    while (!session.done()) {
        while (SDL_GetAudioStreamQueued(stream) < queueTarget && !session.done()) {
            session.render(block.data(), 1024);
            SDL_PutAudioStreamData(stream, block.data(), static_cast<int>(block.size() * sizeof(int16_t)));
        }
        SDL_Delay(10);
    }
    while (SDL_GetAudioStreamQueued(stream) > 0) SDL_Delay(10);
    SDL_DestroyAudioStream(stream);
    SDL_Quit();
    return 0;
}
