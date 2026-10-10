// edison: the native Adventures with Edison launcher.
//
//   edison [CD DSK3 folder] [-O] [-A]
//     -O  skip the opening (as the original's -O)
//     -A  no FM music (as the original's -A)
//     --game mystery   start Mystery at the Museums directly
//     --game rockbach  start Rock and Bach Studio directly (--level N: straight to hallway spot N)
//     --game science   start the Wild Science Arcade directly (--room 501: the lab; 505-510: a lesson; 1-110: a room's table, still)
//     --level N        (with --game mystery) skip setup and play level N (0-7)
//     --puzzle K       (with --game mystery) play only puzzle K (0-15), at difficulty --level;
//                      16 is the bonus maze, 17 the winning end of a game, 18 the losing one,
//                      19 the winning end after the final quiz, 20 the custom level editor
//     --save DIR       where the games keep high scores and players (default: save)
//   For testing without a person at the keyboard:
//     --capture DIR MS    save the display to DIR/NNNNN.bmp every MS milliseconds
//     --capture-dense FROM TO MS   every MS milliseconds instead from FROM till TO
//                         (repeatable: an animation frame by frame)
//     --click T X Y       click at game coordinates X, Y at T milliseconds (repeatable)
//     --rclick T X Y      the same with the right button
//     --move T X Y        move the mouse to X, Y at T milliseconds (its button up)
//     --drag T X0 Y0 X1 Y1 MS   press at X0, Y0 at T, move to X1, Y1 over MS, release
//     --type T TEXT       type TEXT at T milliseconds ('|' is Enter; repeatable)
//     --press T TEXT      the same as key presses (letters, digits, '|'), Caps Lock
//                         on: the characters from the keys and Shift alone
//     --key T NAME MS     press key NAME (esc, kp5, left, right, up, down, shift) at T and
//                         hold it MS milliseconds
//     --quit-after MS     close after MS milliseconds
//     --hidden            no window shown, nothing drawn but the captures, the sound
//                         muted (test runs in the background)
//     --volume N          the sound's volume, 0-100 (default 100; 0 with --hidden)
//     --virtual-clock     time is 1 ms per event pump, not the wall clock (the
//                         times above, the captures and the game's ticks the
//                         same on every run, however busy the machine; a
//                         sample plays as long as its length says)
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
    struct Dense { uint64_t from, to, every; };
    std::vector<Dense> dense;
    struct Click { uint64_t at; int x, y; };
    std::vector<Click> clicks, rightClicks, moves;  // moves: the mouse there, its button up
    // Press at (x0, y0), move to (x1, y1) over `ms`, release.
    struct Drag { uint64_t at; int x0, y0, x1, y1; uint64_t ms; bool started = false, done = false; };
    std::vector<Drag> drags;
    struct Typed { uint64_t at; std::string text; };  // '|' types Enter
    std::vector<Typed> typed;
    std::vector<Typed> pressed;  // --press: as SDL key events
    struct Key { uint64_t at; int key; uint64_t ms; bool down = false, done = false; };
    std::vector<Key> keys;
    uint64_t quitAfter = 0;
    bool hidden = false;  // --hidden: no window shown, sound muted (for test runs in the background)
    int volume = -1;      // --volume N: 0-100 (default 100; 0 with --hidden)
    bool virtualClock = false;  // --virtual-clock: 1 ms per pumpEvents (deterministic test runs)
};

// The Artech library's characters by scan code (MALL DS:72C8 unshifted,
// DS:7328 with Shift; the same tables in EDISON, WINMAIN and WMAIN): a US
// keyboard's, whatever the layout, and Caps Lock not read. The port takes
// the keyboard's own layout instead, Shift and AltGr, still not Caps Lock;
// the keypad's characters come from here (its digits only with Shift).
const char kLibraryChars[2][0x54] = {
    {0, '\x1B', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',
     'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\r', 0, 'a', 's',
     'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\', 'z', 'x', 'c', 'v',
     'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ', 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '-', 0, 0, 0, '+', 0,
     0, 0, 0, 0},
    {0, '\x1B', '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b', '\t',
     'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\r', 0, 'A', 'S',
     'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0, '|', 'Z', 'X', 'C', 'V',
     'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' ', 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, '7', '8', '9', '-', '4', '5', '6', '+', '1',
     '2', '3', '0', '.'},
};

// A key's scan code as Windows gives it in WM_KEYDOWN (the low byte: an
// extended key's is its plain twin's, the keypad's Enter 1Ch); 0 for those
// without a character in the tables.
int scanCode(SDL_Scancode s) {
    if (s >= SDL_SCANCODE_A && s <= SDL_SCANCODE_Z) {
        static const uint8_t kLetters[26] = {0x1E, 0x30, 0x2E, 0x20, 0x12, 0x21, 0x22, 0x23, 0x17, 0x24, 0x25, 0x26, 0x32,
                                             0x31, 0x18, 0x19, 0x10, 0x13, 0x1F, 0x14, 0x16, 0x2F, 0x11, 0x2D, 0x15, 0x2C};
        return kLetters[s - SDL_SCANCODE_A];
    }
    if (s >= SDL_SCANCODE_1 && s <= SDL_SCANCODE_0) return 2 + (s - SDL_SCANCODE_1);
    switch (s) {
    case SDL_SCANCODE_ESCAPE: return 0x01;
    case SDL_SCANCODE_MINUS: return 0x0C;
    case SDL_SCANCODE_EQUALS: return 0x0D;
    case SDL_SCANCODE_BACKSPACE: return 0x0E;
    case SDL_SCANCODE_TAB: return 0x0F;
    case SDL_SCANCODE_LEFTBRACKET: return 0x1A;
    case SDL_SCANCODE_RIGHTBRACKET: return 0x1B;
    case SDL_SCANCODE_RETURN: case SDL_SCANCODE_KP_ENTER: return 0x1C;
    case SDL_SCANCODE_SEMICOLON: return 0x27;
    case SDL_SCANCODE_APOSTROPHE: return 0x28;
    case SDL_SCANCODE_GRAVE: return 0x29;
    case SDL_SCANCODE_BACKSLASH: return 0x2B;
    case SDL_SCANCODE_COMMA: return 0x33;
    case SDL_SCANCODE_PERIOD: return 0x34;
    case SDL_SCANCODE_SLASH: case SDL_SCANCODE_KP_DIVIDE: return 0x35;
    case SDL_SCANCODE_KP_MULTIPLY: return 0x37;
    case SDL_SCANCODE_SPACE: return 0x39;
    case SDL_SCANCODE_KP_7: case SDL_SCANCODE_HOME: return 0x47;
    case SDL_SCANCODE_KP_8: case SDL_SCANCODE_UP: return 0x48;
    case SDL_SCANCODE_KP_9: case SDL_SCANCODE_PAGEUP: return 0x49;
    case SDL_SCANCODE_KP_MINUS: return 0x4A;
    case SDL_SCANCODE_KP_4: case SDL_SCANCODE_LEFT: return 0x4B;
    case SDL_SCANCODE_KP_5: return 0x4C;
    case SDL_SCANCODE_KP_6: case SDL_SCANCODE_RIGHT: return 0x4D;
    case SDL_SCANCODE_KP_PLUS: return 0x4E;
    case SDL_SCANCODE_KP_1: case SDL_SCANCODE_END: return 0x4F;
    case SDL_SCANCODE_KP_2: case SDL_SCANCODE_DOWN: return 0x50;
    case SDL_SCANCODE_KP_3: case SDL_SCANCODE_PAGEDOWN: return 0x51;
    case SDL_SCANCODE_KP_0: case SDL_SCANCODE_INSERT: return 0x52;
    case SDL_SCANCODE_KP_PERIOD: case SDL_SCANCODE_DELETE: return 0x53;
    default: return 0;
    }
}

class SdlPlatform : public edison::Platform {
public:
    static constexpr int kShift = 0x1000;  // --key's shift: a key down without a character
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
        texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING,
                                     edison::Screen::kWidth, edison::Screen::kHeight);
        if (!texture_) return fail(error, "SDL_CreateTexture");
        SDL_SetTextureScaleMode(texture_, SDL_SCALEMODE_NEAREST);

        device_ = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
        if (!device_) {
            std::fprintf(stderr, "no audio: %s\n", SDL_GetError());
            return true;
        }
        // --volume, or muted for --hidden: the whole device's gain.
        const int volume = automation.volume >= 0 ? automation.volume : automation.hidden ? 0 : 100;
        SDL_SetAudioDeviceGain(device_, static_cast<float>(std::clamp(volume, 0, 100)) / 100.0f);
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
        if (automation.virtualClock) ++virtualNow_;
        const uint64_t now = milliseconds();
        if (automation.quitAfter && now >= automation.quitAfter) return false;
        for (auto& t : automation.pressed)
            if (t.at && now >= t.at) {
                for (char c : t.text) {
                    SDL_Event e{};
                    e.type = SDL_EVENT_KEY_DOWN;
                    e.key.down = true;
                    e.key.mod = SDL_KMOD_CAPS;
                    if (c == '|') {
                        e.key.scancode = SDL_SCANCODE_RETURN;
                        e.key.key = SDLK_RETURN;
                    } else if (c >= '1' && c <= '9') {
                        e.key.scancode = static_cast<SDL_Scancode>(SDL_SCANCODE_1 + (c - '1'));
                    } else if (c == '0') {
                        e.key.scancode = SDL_SCANCODE_0;
                    } else {
                        const bool upper = c >= 'A' && c <= 'Z';
                        e.key.scancode = static_cast<SDL_Scancode>(SDL_SCANCODE_A + ((upper ? c + 32 : c) - 'a'));
                        if (upper) e.key.mod |= SDL_KMOD_LSHIFT;
                    }
                    SDL_PushEvent(&e);
                }
                t.at = 0;
            }
        for (auto& t : automation.typed)
            if (t.at && now >= t.at) {
                for (char c : t.text) keys_.push_back(c == '|' ? static_cast<int>(kEnter) : c);
                keyDown_ = true;
                t.at = 0;
            }
        for (auto& k : automation.keys) {
            if (!k.down && !k.done && now >= k.at) {
                if (k.key != kShift) keys_.push_back(k.key);
                keyDown_ = true;
                k.down = true;
            }
            if (k.down && now >= k.at + k.ms) k.down = false, k.done = true;
        }
        for (auto& c : automation.clicks)
            if (c.at && now >= c.at) {
                clicked_ = pressed_ = true;
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
                clicked_ = pressed_ = true;
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
        // (After the drags: a move at a drag's end isn't undone by it.)
        for (auto& c : automation.moves)
            if (c.at && now >= c.at) {
                autoMouse_ = true;
                dragging_ = true;
                autoDown_ = false;
                autoX_ = c.x;
                autoY_ = c.y;
                c.at = 0;
            }
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) return false;
            if (e.type == SDL_EVENT_KEY_DOWN) {
                keyDown_ = true;
                // The keypad by its keys (the games read scan codes: its 4,
                // 8, 6 and 2 are the arrows' and its 5 is 4Ch, NumLock or not).
                switch (e.key.scancode) {
                case SDL_SCANCODE_KP_4: keys_.push_back(kLeft); break;
                case SDL_SCANCODE_KP_6: keys_.push_back(kRight); break;
                case SDL_SCANCODE_KP_8: keys_.push_back(kUp); break;
                case SDL_SCANCODE_KP_2: keys_.push_back(kDown); break;
                case SDL_SCANCODE_KP_5: keys_.push_back(kCentre); break;
                default: break;
                }
                const bool keypad = e.key.scancode == SDL_SCANCODE_KP_4 || e.key.scancode == SDL_SCANCODE_KP_6 ||
                                    e.key.scancode == SDL_SCANCODE_KP_8 || e.key.scancode == SDL_SCANCODE_KP_2 ||
                                    e.key.scancode == SDL_SCANCODE_KP_5;
                if (!keypad) switch (e.key.key) {
                case SDLK_LEFT: keys_.push_back(kLeft); break;
                case SDLK_RIGHT: keys_.push_back(kRight); break;
                case SDLK_UP: keys_.push_back(kUp); break;
                case SDLK_DOWN: keys_.push_back(kDown); break;
                default: break;
                }
                // The character, Caps Lock not read (the library takes Shift
                // alone): the keyboard's layout, Backspace, Tab, Enter and Esc
                // (8, 9, 0Dh, 1Bh); the keypad by the library's table. None
                // with Alt alone (WM_SYSKEYDOWN: 39:02e4 takes any as Alt).
                const SDL_Keymod mod = e.key.mod;
                if (!(mod & SDL_KMOD_ALT) || (mod & SDL_KMOD_CTRL) || (mod & SDL_KMOD_MODE)) {
                    const bool shift = (mod & SDL_KMOD_SHIFT) != 0;
                    const bool pad = e.key.scancode >= SDL_SCANCODE_KP_DIVIDE && e.key.scancode <= SDL_SCANCODE_KP_PERIOD;
                    if (pad) {
                        if (const int scan = scanCode(e.key.scancode))
                            if (const char c = kLibraryChars[shift ? 1 : 0][scan]) keys_.push_back(c);
                    } else {
                        const SDL_Keycode k = SDL_GetKeyFromScancode(
                            e.key.scancode, static_cast<SDL_Keymod>(mod & (SDL_KMOD_SHIFT | SDL_KMOD_MODE)), false);
                        if ((k >= 32 && k < 127) || k == kBackspace || k == kTab || k == kEnter || k == kEscape)
                            keys_.push_back(static_cast<int>(k));
                    }
                }
            }
            if (e.type == SDL_EVENT_MOUSE_MOTION) autoMouse_ = false;
            if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT) {
                SDL_ConvertEventToRenderCoordinates(renderer_, &e);
                clicked_ = pressed_ = true;
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

    uint64_t milliseconds() override { return automation.virtualClock ? virtualNow_ : SDL_GetTicks() - start_; }

    void present(const edison::Screen& screen, const edison::Palette& palette) override {
        if (automation.hidden) {  // (no window to show: only the captures)
            capture(screen, palette);
            return;
        }
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

    bool lastPress(int* x, int* y) override {
        // (clickX_, clickY_ stay after the click is taken.)
        if (!pressed_) return false;
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

    bool takeKeyDown() override {
        const bool down = keyDown_;
        keyDown_ = false;
        return down;
    }

    bool takeKeyIf(int key) override {
        const auto it = std::find(keys_.begin(), keys_.end(), key);
        if (it == keys_.end()) return false;
        keys_.erase(it);
        return true;
    }

    bool escapeHeld() override { return keyHeld(kEscape); }

    bool keyHeld(int key) override {
        for (const auto& k : automation.keys)
            if (k.down && k.key == key) return true;
        const bool* state = SDL_GetKeyboardState(nullptr);
        switch (key) {
        case kEscape: return state[SDL_SCANCODE_ESCAPE];
        case kLeft: return state[SDL_SCANCODE_LEFT] || state[SDL_SCANCODE_KP_4];
        case kRight: return state[SDL_SCANCODE_RIGHT] || state[SDL_SCANCODE_KP_6];
        case kUp: return state[SDL_SCANCODE_UP] || state[SDL_SCANCODE_KP_8];
        case kDown: return state[SDL_SCANCODE_DOWN] || state[SDL_SCANCODE_KP_2];
        default: return false;
        }
    }

    bool wavPlaying() override {
        if (automation.virtualClock) return virtualNow_ < wavEnd_;
        return wavStream_ && SDL_GetAudioStreamQueued(wavStream_) > 0;
    }
    void stopWav() override {
        wavEnd_ = 0;
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
        // (--virtual-clock: it plays for its length in that clock.)
        const uint64_t bytesPerSecond = static_cast<uint64_t>(spec.freq) * spec.channels * SDL_AUDIO_BYTESIZE(spec.format);
        if (bytesPerSecond) wavEnd_ = virtualNow_ + length * 1000ull / bytesPerSecond;
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
        if (automation.captureDir.empty()) return;
        const uint64_t now = milliseconds();
        uint64_t every = automation.captureEvery;
        for (const auto& d : automation.dense)
            if (now >= d.from && now < d.to) every = d.every;
        if (captured_ && now < lastCapture_ + every) return;
        captured_ = true;
        lastCapture_ = now;
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
    bool keyDown_ = false;  // a key went down (takeKeyDown)
    uint64_t start_ = 0;
    uint64_t virtualNow_ = 0, wavEnd_ = 0;  // --virtual-clock
    uint64_t lastCapture_ = 0;
    bool captured_ = false;
    bool clicked_ = false;
    bool pressed_ = false;  // a left press has happened (lastPress)
    bool rightClicked_ = false;
    int rightX_ = 0, rightY_ = 0;
    int clickX_ = 0, clickY_ = 0;
    bool autoMouse_ = false;
    bool dragging_ = false, autoDown_ = false;  // an automated drag
    int autoX_ = 0, autoY_ = 0;
};

}  // namespace

static const char kUsage[] =
    "  edison [CD DSK3 folder] [-O] [-A]\n"
    "    -O  skip the opening (as the original's -O)\n"
    "    -A  no FM music (as the original's -A)\n"
    "    --game mystery   start Mystery at the Museums directly\n"
    "    --game rockbach  start Rock and Bach Studio directly (--level N: straight to hallway spot N)\n"
    "    --game science   start the Wild Science Arcade directly (--room 501: the lab; 505-510: a lesson; 1-110: a room's table, still)\n"
    "    --level N        (with --game mystery) skip setup and play level N (0-7)\n"
    "    --puzzle K       (with --game mystery) play only puzzle K (0-15), at difficulty --level;\n"
    "                     16 is the bonus maze, 17 the winning end of a game, 18 the losing one,\n"
    "                     19 the winning end after the final quiz, 20 the custom level editor\n"
    "    --save DIR       where the games keep high scores and players (default: save)\n"
    "  For testing without a person at the keyboard:\n"
    "    --capture DIR MS    save the display to DIR/NNNNN.bmp every MS milliseconds\n"
    "    --capture-dense FROM TO MS   every MS milliseconds instead from FROM till TO\n"
    "                        (repeatable: an animation frame by frame)\n"
    "    --click T X Y       click at game coordinates X, Y at T milliseconds (repeatable)\n"
    "    --rclick T X Y      the same with the right button\n"
    "    --move T X Y        move the mouse to X, Y at T milliseconds (its button up)\n"
    "    --drag T X0 Y0 X1 Y1 MS   press at X0, Y0 at T, move to X1, Y1 over MS, release\n"
    "    --type T TEXT       type TEXT at T milliseconds ('|' is Enter; repeatable)\n"
    "    --press T TEXT      the same as key presses (letters, digits, '|'), Caps Lock\n"
    "                        on: the characters from the keys and Shift alone\n"
    "    --key T NAME MS     press key NAME (esc, kp5, left, right, up, down, shift) at T and\n"
    "                        hold it MS milliseconds\n"
    "    --quit-after MS     close after MS milliseconds\n"
    "    --hidden            no window shown, nothing drawn but the captures, the sound\n"
    "                        muted (test runs in the background)\n"
    "    --volume N          the sound's volume, 0-100 (default 100; 0 with --hidden)\n"
    "    --virtual-clock     time is 1 ms per event pump, not the wall clock (the\n"
    "                        times above, the captures and the game's ticks the\n"
    "                        same on every run, however busy the machine; a\n"
    "                        sample plays as long as its length says)\n";

int main(int argc, char** argv) {
    edison::Launcher::Options options;
    options.cdDir = "original/cd/DSK3";
    Automation automation;
    std::string startGame;
    int startLevel = -1;
    int startRoom = -1;  // --room (Wild Science)
    int startPuzzle = -1;
    std::string saveDir = "save";
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--capture" && i + 2 < argc) {
            automation.captureDir = argv[++i];
            automation.captureEvery = std::strtoull(argv[++i], nullptr, 10);
        } else if (a == "--capture-dense" && i + 3 < argc) {
            const uint64_t from = std::strtoull(argv[++i], nullptr, 10);
            const uint64_t to = std::strtoull(argv[++i], nullptr, 10);
            automation.dense.push_back({from, to, std::strtoull(argv[++i], nullptr, 10)});
        } else if (a == "--rclick" && i + 3 < argc) {
            const uint64_t at = std::strtoull(argv[++i], nullptr, 10);
            const int x = std::atoi(argv[++i]);
            automation.rightClicks.push_back({at, x, std::atoi(argv[++i])});
        } else if (a == "--move" && i + 3 < argc) {
            const uint64_t at = std::strtoull(argv[++i], nullptr, 10);
            const int x = std::atoi(argv[++i]);
            automation.moves.push_back({at, x, std::atoi(argv[++i])});
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
        } else if (a == "--key" && i + 3 < argc) {
            const uint64_t at = std::strtoull(argv[++i], nullptr, 10);
            const std::string name = argv[++i];
            const int key = name == "esc" ? SdlPlatform::kEscape : name == "kp5" ? SdlPlatform::kCentre
                            : name == "left" ? SdlPlatform::kLeft : name == "right" ? SdlPlatform::kRight
                            : name == "up" ? SdlPlatform::kUp : name == "down" ? SdlPlatform::kDown
                            : name == "shift" ? SdlPlatform::kShift : 0;
            const uint64_t ms = std::strtoull(argv[++i], nullptr, 10);
            if (key) automation.keys.push_back({at, key, ms});
        } else if (a == "--type" && i + 2 < argc) {
            const uint64_t at = std::strtoull(argv[++i], nullptr, 10);
            automation.typed.push_back({at, argv[++i]});
        } else if (a == "--press" && i + 2 < argc) {
            const uint64_t at = std::strtoull(argv[++i], nullptr, 10);
            automation.pressed.push_back({at, argv[++i]});
        } else if (a == "--room" && i + 1 < argc) {
            startRoom = std::atoi(argv[++i]);
        } else if (a == "--virtual-clock") {
            automation.virtualClock = true;
        } else if (a == "--hidden") {
            automation.hidden = true;
        } else if (a == "--volume" && i + 1 < argc) {
            automation.volume = std::atoi(argv[++i]);
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
        else if (a == "--help" || a == "-h" || a == "-?" || a == "/?") {
            std::fputs(kUsage, stdout);
            return 0;
        } else if (a.size() > 1 && a[0] == '-') {
            // Not taken as the folder ("cannot open --x/SHELL.D01").
            std::fprintf(stderr, "edison: unknown option %s (or its arguments missing); --help lists them\n", a.c_str());
            return 2;
        } else options.cdDir = a;
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
        so.startRoom = startRoom;
        startRoom = -1;  // only the first time
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
