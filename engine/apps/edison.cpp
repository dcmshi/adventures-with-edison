// edison: the native Adventures with Edison launcher.
//
//   edison [CD DSK3 folder] [-O] [-A]
//     -O  skip the opening (as the original's -O)
//     -A  no FM music (as the original's -A)
//     --game mystery   start Mystery at the Museums directly
//     --game rockbach  start Rock and Bach Studio directly (--level N: straight to hallway spot N)
//     --game science   start the Wild Science Arcade directly
//     --level N        (with --game mystery) skip setup and play level N (0-7)
//     --puzzle K       (with --game mystery) play only puzzle K (0-15), at difficulty --level;
//                      16 is the bonus maze, 17 the winning end of a game, 18 the losing one,
//                      19 the winning end after the final quiz, 20 the custom level editor
//     --save DIR       where the games keep high scores and players (default: save)
//   For testing without a person at the keyboard:
//     --capture DIR MS    save the display to DIR/NNNNN.bmp every MS milliseconds
//     --click T X Y       click at game coordinates X, Y at T milliseconds (repeatable)
//     --rclick T X Y      the same with the right button
//     --drag T X0 Y0 X1 Y1 MS   press at X0, Y0 at T, move to X1, Y1 over MS, release
//     --type T TEXT       type TEXT at T milliseconds ('|' is Enter; repeatable)
//     --quit-after MS     close after MS milliseconds
//     --hidden            no window shown and the sound muted (test runs in the background)
//
// The folder defaults to original/cd/DSK3 (needs EDISON.EXE, SHELL.D01 and
// CADLIB.DLL from the CD). Mystery at the Museums is ported; picking one of
// the other games returns to the menu.

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "audio/artech_fm_driver.h"
#include "audio/fm_renderer.h"
#include "launcher/launcher.h"
#include "mystery/mystery.h"
#include "rockbach/rockbach.h"
#include "science/science.h"

namespace {

constexpr int kSampleRate = 48000;

struct Automation {
    std::string captureDir;
    uint64_t captureEvery = 0;
    struct Click { uint64_t at; int x, y; };
    std::vector<Click> clicks, rightClicks;
    // Press at (x0, y0), move to (x1, y1) over `ms`, release.
    struct Drag { uint64_t at; int x0, y0, x1, y1; uint64_t ms; bool started = false, done = false; };
    std::vector<Drag> drags;
    struct Typed { uint64_t at; std::string text; };  // '|' types Enter
    std::vector<Typed> typed;
    uint64_t quitAfter = 0;
    bool hidden = false;  // --hidden: no window shown, sound muted (for test runs in the background)
};

class SdlPlatform : public edison::Platform {
public:
    Automation automation;
    std::string startGame;

    bool open(bool music, std::string* error) {
        music_ = music;
        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) return fail(error, "SDL_Init");
        const SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE | (automation.hidden ? SDL_WINDOW_HIDDEN : 0);
        if (!SDL_CreateWindowAndRenderer("Adventures with Edison", 1280, 800, flags,
                                         &window_, &renderer_))
            return fail(error, "SDL_CreateWindowAndRenderer");
        SDL_SetRenderLogicalPresentation(renderer_, edison::Screen::kWidth, edison::Screen::kHeight,
                                         SDL_LOGICAL_PRESENTATION_LETTERBOX);
        SDL_SetRenderVSync(renderer_, automation.hidden ? 0 : 1);
        SDL_StartTextInput(window_);
        texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING,
                                     edison::Screen::kWidth, edison::Screen::kHeight);
        if (!texture_) return fail(error, "SDL_CreateTexture");
        SDL_SetTextureScaleMode(texture_, SDL_SCALEMODE_NEAREST);

        device_ = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
        if (!device_) {
            std::fprintf(stderr, "no audio: %s\n", SDL_GetError());
            return true;
        }
        start_ = SDL_GetTicks();
        return true;
    }

    ~SdlPlatform() override {
        if (fmStream_) SDL_DestroyAudioStream(fmStream_);
        if (wavStream_) SDL_DestroyAudioStream(wavStream_);
        if (device_) SDL_CloseAudioDevice(device_);
        if (texture_) SDL_DestroyTexture(texture_);
        if (renderer_) SDL_DestroyRenderer(renderer_);
        if (window_) SDL_DestroyWindow(window_);
        SDL_Quit();
    }

    bool pumpEvents() override {
        const uint64_t now = milliseconds();
        if (automation.quitAfter && now >= automation.quitAfter) return false;
        for (auto& t : automation.typed)
            if (t.at && now >= t.at) {
                for (char c : t.text) keys_.push_back(c == '|' ? static_cast<int>(kEnter) : c);
                t.at = 0;
            }
        for (auto& c : automation.clicks)
            if (c.at && now >= c.at) {
                clicked_ = true;
                clickX_ = c.x;
                clickY_ = c.y;
                c.at = 0;
                autoMouse_ = true;
                dragging_ = false;
            }
        for (auto& c : automation.rightClicks)
            if (c.at && now >= c.at) {
                rightClicked_ = true;
                rightX_ = c.x;
                rightY_ = c.y;
                c.at = 0;
            }
        for (auto& d : automation.drags) {
            if (d.done || now < d.at) continue;
            if (!d.started) {
                d.started = true;
                clicked_ = true;
                clickX_ = d.x0;
                clickY_ = d.y0;
            }
            autoMouse_ = true;
            const uint64_t t = std::min<uint64_t>(now - d.at, d.ms);
            autoX_ = d.x0 + static_cast<int>((d.x1 - d.x0) * static_cast<int64_t>(t) / std::max<int64_t>(d.ms, 1));
            autoY_ = d.y0 + static_cast<int>((d.y1 - d.y0) * static_cast<int64_t>(t) / std::max<int64_t>(d.ms, 1));
            autoDown_ = now - d.at < d.ms;
            if (!autoDown_) d.done = true;
            dragging_ = true;
        }
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) return false;
            if (e.type == SDL_EVENT_TEXT_INPUT) {
                for (const char* c = e.text.text; *c; ++c)
                    if (*c >= 32 && *c < 127) keys_.push_back(*c);
            }
            if (e.type == SDL_EVENT_KEY_DOWN) {
                switch (e.key.key) {
                case SDLK_BACKSPACE: keys_.push_back(kBackspace); break;
                case SDLK_TAB: keys_.push_back(kTab); break;
                case SDLK_RETURN: case SDLK_KP_ENTER: keys_.push_back(kEnter); break;
                case SDLK_ESCAPE: keys_.push_back(kEscape); break;
                case SDLK_LEFT: keys_.push_back(kLeft); break;
                case SDLK_RIGHT: keys_.push_back(kRight); break;
                case SDLK_UP: keys_.push_back(kUp); break;
                case SDLK_DOWN: keys_.push_back(kDown); break;
                default: break;
                }
            }
            if (e.type == SDL_EVENT_MOUSE_MOTION) autoMouse_ = false;
            if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT) {
                SDL_ConvertEventToRenderCoordinates(renderer_, &e);
                clicked_ = true;
                clickX_ = static_cast<int>(e.button.x);
                clickY_ = static_cast<int>(e.button.y);
            }
            if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_RIGHT) {
                SDL_ConvertEventToRenderCoordinates(renderer_, &e);
                rightClicked_ = true;
                rightX_ = static_cast<int>(e.button.x);
                rightY_ = static_cast<int>(e.button.y);
            }
        }
        return true;
    }

    uint64_t milliseconds() override { return SDL_GetTicks() - start_; }

    void present(const edison::Screen& screen, const edison::Palette& palette) override {
        void* pixels;
        int pitch;
        if (SDL_LockTexture(texture_, nullptr, &pixels, &pitch)) {
            uint32_t lut[256];
            for (int i = 0; i < 256; ++i)
                lut[i] = 0xFF000000u | palette[i].r << 16 | palette[i].g << 8 | palette[i].b;
            for (int y = 0; y < edison::Screen::kHeight; ++y) {
                auto* row = reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(pixels) + y * pitch);
                const uint8_t* src = &screen.pixels[static_cast<size_t>(y) * edison::Screen::kWidth];
                for (int x = 0; x < edison::Screen::kWidth; ++x) row[x] = lut[src[x]];
            }
            SDL_UnlockTexture(texture_);
        }
        capture(screen, palette);
        SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
        SDL_RenderClear(renderer_);
        SDL_RenderTexture(renderer_, texture_, nullptr, nullptr);
        SDL_RenderPresent(renderer_);
    }

    bool takeClick(int* x, int* y) override {
        if (!clicked_) return false;
        clicked_ = false;
        *x = clickX_;
        *y = clickY_;
        return true;
    }

    bool takeRightClick(int* x, int* y) override {
        if (!rightClicked_) return false;
        rightClicked_ = false;
        *x = rightX_;
        *y = rightY_;
        return true;
    }

    void mouse(int* x, int* y, bool* down) override {
        if (autoMouse_) {  // an automated click moves the (virtual) mouse there
            *x = dragging_ ? autoX_ : clickX_;
            *y = dragging_ ? autoY_ : clickY_;
            *down = dragging_ && autoDown_;
            return;
        }
        float wx, wy;
        const SDL_MouseButtonFlags buttons = SDL_GetMouseState(&wx, &wy);
        float gx = wx, gy = wy;
        SDL_RenderCoordinatesFromWindow(renderer_, wx, wy, &gx, &gy);
        *x = static_cast<int>(gx);
        *y = static_cast<int>(gy);
        *down = (buttons & SDL_BUTTON_LMASK) != 0;
    }

    int takeKey() override {
        if (keys_.empty()) return 0;
        const int k = keys_.front();
        keys_.erase(keys_.begin());
        return k;
    }

    bool escapeHeld() override { return SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_ESCAPE]; }

    bool keyHeld(int key) override {
        const bool* state = SDL_GetKeyboardState(nullptr);
        switch (key) {
        case kLeft: return state[SDL_SCANCODE_LEFT];
        case kRight: return state[SDL_SCANCODE_RIGHT];
        case kUp: return state[SDL_SCANCODE_UP];
        case kDown: return state[SDL_SCANCODE_DOWN];
        default: return false;
        }
    }

    bool wavPlaying() override { return wavStream_ && SDL_GetAudioStreamQueued(wavStream_) > 0; }
    void stopWav() override {
        if (wavStream_) SDL_ClearAudioStream(wavStream_);
    }

    void playWav(const std::vector<uint8_t>& wav) override {
        if (!device_) return;
        SDL_AudioSpec spec;
        Uint8* data = nullptr;
        Uint32 length = 0;
        if (!SDL_LoadWAV_IO(SDL_IOFromConstMem(wav.data(), wav.size()), true, &spec, &data, &length)) {
            std::fprintf(stderr, "bad WAV: %s\n", SDL_GetError());
            return;
        }
        if (!wavStream_) {
            wavStream_ = SDL_CreateAudioStream(&spec, nullptr);
            SDL_BindAudioStream(device_, wavStream_);
        }
        // One sample at a time, like the original's waveOut: a new one cuts the last.
        SDL_ClearAudioStream(wavStream_);
        SDL_SetAudioStreamFormat(wavStream_, &spec, nullptr);
        SDL_PutAudioStreamData(wavStream_, data, static_cast<int>(length));
        SDL_FlushAudioStream(wavStream_);  // so the resampler's tail drains and wavPlaying() ends
        SDL_free(data);
    }

    void setFmDriver(const std::string& dllPath) override {
        if (!music_ || !device_ || dllPath == fmDll_) return;
        if (fmStream_) {
            SDL_DestroyAudioStream(fmStream_);  // stops the callback first
            fmStream_ = nullptr;
        }
        std::lock_guard<std::mutex> lock(fmMutex_);
        renderer16_.reset();
        if (dllPath.empty()) {  // no driver: silence
            driver_.reset();
            fmDll_.clear();
            return;
        }
        driver_ = std::make_unique<edison::ArtechFmDriver>();
        std::string error;
        if (!driver_->loadDll(dllPath, &error)) {
            std::fprintf(stderr, "no FM music: %s\n", error.c_str());
            driver_.reset();
            fmDll_.clear();
            return;
        }
        fmDll_ = dllPath;
        driver_->init();
        renderer16_ = std::make_unique<edison::FmRenderer>(*driver_, kSampleRate);
        const SDL_AudioSpec spec{SDL_AUDIO_S16, 2, kSampleRate};
        fmStream_ = SDL_CreateAudioStream(&spec, nullptr);
        SDL_SetAudioStreamGetCallback(fmStream_, &SdlPlatform::fmCallback, this);
        SDL_BindAudioStream(device_, fmStream_);
    }

    void sendFm(uint16_t sound) override {
        if (!driver_) return;
        std::lock_guard<std::mutex> lock(fmMutex_);
        driver_->sendSound(sound);
    }

    void withFm(const std::function<void(edison::ArtechFmDriver&)>& fn) override {
        std::lock_guard<std::mutex> lock(fmMutex_);
        if (driver_) fn(*driver_);
    }

private:
    void capture(const edison::Screen& screen, const edison::Palette& palette) {
        if (automation.captureDir.empty() || milliseconds() < nextCapture_) return;
        nextCapture_ = milliseconds() + automation.captureEvery;
        SDL_Surface* s = SDL_CreateSurfaceFrom(edison::Screen::kWidth, edison::Screen::kHeight,
                                               SDL_PIXELFORMAT_INDEX8,
                                               const_cast<uint8_t*>(screen.pixels.data()),
                                               edison::Screen::kWidth);
        if (!s) return;
        SDL_Palette* pal = SDL_CreateSurfacePalette(s);
        SDL_Color colours[256];
        for (int i = 0; i < 256; ++i) colours[i] = {palette[i].r, palette[i].g, palette[i].b, 255};
        SDL_SetPaletteColors(pal, colours, 0, 256);
        char name[64];
        std::snprintf(name, sizeof name, "/%05llu.bmp", static_cast<unsigned long long>(milliseconds()));
        SDL_SaveBMP(s, (automation.captureDir + name).c_str());
        SDL_DestroySurface(s);
    }

    static void SDLCALL fmCallback(void* self, SDL_AudioStream* stream, int additional, int) {
        auto* p = static_cast<SdlPlatform*>(self);
        const int frames = additional / 4;
        if (frames <= 0) return;
        p->fmBuffer_.resize(static_cast<size_t>(frames) * 2);
        {
            std::lock_guard<std::mutex> lock(p->fmMutex_);
            p->renderer16_->render(p->fmBuffer_.data(), static_cast<uint32_t>(frames));
        }
        SDL_PutAudioStreamData(stream, p->fmBuffer_.data(), frames * 4);
    }

    bool fail(std::string* error, const char* what) {
        if (error) *error = std::string(what) + ": " + SDL_GetError();
        return false;
    }

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture* texture_ = nullptr;
    SDL_AudioDeviceID device_ = 0;
    SDL_AudioStream* fmStream_ = nullptr;
    SDL_AudioStream* wavStream_ = nullptr;
    std::unique_ptr<edison::ArtechFmDriver> driver_;
    std::unique_ptr<edison::FmRenderer> renderer16_;
    std::mutex fmMutex_;
    std::vector<int16_t> fmBuffer_;
    bool music_ = true;
    std::string fmDll_;
    std::vector<int> keys_;
    uint64_t start_ = 0;
    uint64_t nextCapture_ = 0;
    bool clicked_ = false;
    bool rightClicked_ = false;
    int rightX_ = 0, rightY_ = 0;
    int clickX_ = 0, clickY_ = 0;
    bool autoMouse_ = false;
    bool dragging_ = false, autoDown_ = false;  // an automated drag
    int autoX_ = 0, autoY_ = 0;
};

}  // namespace

int main(int argc, char** argv) {
    edison::Launcher::Options options;
    options.cdDir = "original/cd/DSK3";
    Automation automation;
    std::string startGame;
    int startLevel = -1;
    int startPuzzle = -1;
    std::string saveDir = "save";
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--capture" && i + 2 < argc) {
            automation.captureDir = argv[++i];
            automation.captureEvery = std::strtoull(argv[++i], nullptr, 10);
        } else if (a == "--rclick" && i + 3 < argc) {
            const uint64_t at = std::strtoull(argv[++i], nullptr, 10);
            const int x = std::atoi(argv[++i]);
            automation.rightClicks.push_back({at, x, std::atoi(argv[++i])});
        } else if (a == "--click" && i + 3 < argc) {
            const uint64_t at = std::strtoull(argv[++i], nullptr, 10);
            const int x = std::atoi(argv[++i]);
            automation.clicks.push_back({at, x, std::atoi(argv[++i])});
        } else if (a == "--drag" && i + 6 < argc) {
            Automation::Drag d{};
            d.at = std::strtoull(argv[++i], nullptr, 10);
            d.x0 = std::atoi(argv[++i]);
            d.y0 = std::atoi(argv[++i]);
            d.x1 = std::atoi(argv[++i]);
            d.y1 = std::atoi(argv[++i]);
            d.ms = std::strtoull(argv[++i], nullptr, 10);
            automation.drags.push_back(d);
        } else if (a == "--type" && i + 2 < argc) {
            const uint64_t at = std::strtoull(argv[++i], nullptr, 10);
            automation.typed.push_back({at, argv[++i]});
        } else if (a == "--hidden") {
            automation.hidden = true;
        } else if (a == "--quit-after" && i + 1 < argc) {
            automation.quitAfter = std::strtoull(argv[++i], nullptr, 10);
        } else if (a == "--puzzle" && i + 1 < argc) {
            startPuzzle = std::atoi(argv[++i]);
        } else if (a == "--level" && i + 1 < argc) {
            startLevel = std::atoi(argv[++i]);
        } else if (a == "--save" && i + 1 < argc) {
            saveDir = argv[++i];
        } else if (a == "--game" && i + 1 < argc) {
            startGame = argv[++i];
        } else if (a == "-O" || a == "-o") options.skipOpening = true;
        else if (a == "-A" || a == "-a") options.music = false;
        else options.cdDir = a;
    }

    auto platform = std::make_unique<SdlPlatform>();
    platform->automation = automation;
    std::string error;
    if (!platform->open(options.music, &error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }
    auto runMystery = [&]() -> bool {
        edison::Mystery::Options mo;
        mo.cdDir = options.cdDir;
        mo.music = options.music;
        mo.startLevel = startLevel;
        mo.startPuzzle = startPuzzle;
        mo.saveDir = saveDir;
        startPuzzle = -1;
        startLevel = -1;  // only the first time
        auto game = std::make_unique<edison::Mystery>(*platform);
        if (!game->load(mo, &error)) {
            std::fprintf(stderr, "%s\n", error.c_str());
            return false;
        }
        game->run();
        return true;
    };
    auto runRockBach = [&]() -> bool {
        edison::RockBach::Options ro;
        ro.cdDir = options.cdDir;
        ro.music = options.music;
        ro.saveDir = saveDir;
        ro.startActivity = startLevel;
        startLevel = -1;
        auto game = std::make_unique<edison::RockBach>(*platform);
        if (!game->load(ro, &error)) {
            std::fprintf(stderr, "%s\n", error.c_str());
            return false;
        }
        game->run();
        return true;
    };
    auto runScience = [&]() -> bool {
        edison::Science::Options so;
        so.cdDir = options.cdDir;
        so.music = options.music;
        so.saveDir = saveDir;
        auto game = std::make_unique<edison::Science>(*platform);
        if (!game->load(so, &error)) {
            std::fprintf(stderr, "%s\n", error.c_str());
            return false;
        }
        game->run();
        return true;
    };
    try {
        if (startGame == "science") {
            if (!runScience()) return 1;
            options.skipOpening = true;
        }
        if (startGame == "rockbach") {
            if (!runRockBach()) return 1;
            options.skipOpening = true;
        }
        if (startGame == "mystery") {
            if (!runMystery()) return 1;
            options.skipOpening = true;  // like "edison.exe -O" after a game
        }
        for (;;) {
            auto launcher = std::make_unique<edison::Launcher>(*platform);
            if (!launcher->load(options, &error)) {
                std::fprintf(stderr, "%s\n", error.c_str());
                if (!automation.quitAfter)
                    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Adventures with Edison", error.c_str(), nullptr);
                return 1;
            }
            const auto choice = launcher->run();
            if (choice == edison::Launcher::kQuit) break;
            if (choice == edison::Launcher::kMystery) {
                if (!runMystery()) return 1;
            } else if (choice == edison::Launcher::kRockAndBach) {
                if (!runRockBach()) return 1;
            } else if (choice == edison::Launcher::kWildScience) {
                if (!runScience()) return 1;
            }
            options.skipOpening = true;  // like coming back from a game
        }
    } catch (const edison::GameContext::Closed&) {
    }
    return 0;
}
