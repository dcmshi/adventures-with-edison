// WINMAIN.EXE's top level (segment 33) and the intro (segments 5 and 25).

#include "rockbach/rockbach.h"
#include "formats/paths.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <random>

#include "audio/artech_fm_driver.h"
#include "formats/ne_file.h"

namespace edison {

bool RockBach::load(const Options& options, std::string* error) {
    options_ = options;
    // The CD root holds the RB folder of speech and effects, next to DSK3.
    cdRoot_ = options.cdDir + "/..";
    if (!ctx_.archive.open(options.cdDir + "/RB.D01", error)) return false;
    NeFile exe;
    if (!exe.load(options.cdDir + "/WINMAIN.EXE", error)) return false;
    data_ = exe.segment(exe.segmentCount());  // DGROUP, the last segment
    sine_ = exe.segment(53);
    if (data_.size() < 0x2000 || dataString(0x10) != "Rock and Bach") {
        if (error) *error = "WINMAIN.EXE: unexpected data segment";
        return false;
    }
    font_ = &ctx_.font(0x0101);  // f33_0000
    return true;
}

std::string RockBach::dataString(uint16_t offset) const {
    std::string s;
    for (size_t i = offset; i < data_.size() && data_[i]; ++i) s += static_cast<char>(data_[i]);
    return s;
}

uint16_t RockBach::dataWord(uint16_t offset) const {
    return offset + 1u < data_.size() ? static_cast<uint16_t>(data_[offset] | data_[offset + 1] << 8) : 0;
}

void RockBach::run() {
    // f33_0422.
    ctx_.startTimer();
    setDriver(-1);
    select(1);
    if (options_.startActivity < 0) {
        // f33_03dc(1): the intro, with driver 4.
        corelPresents();
        setDriver(4);
        logo();
    } else {
        // (Testing: Edison's colours as the hallway would have left them,
        // which are its backdrop's own E1-ED: lookColours(0) only puts the
        // look into screen 2's palette, and DS:8CC0 keeps the display's.)
        loadLook();
        backdrop(0x1007);
        std::copy_n(ctx_.screens[2].palette.begin() + 0xE1, savedLook_.size(), savedLook_.begin());
    }
    bool again = false;
    for (;;) {
        int result = options_.startActivity;
        options_.startActivity = -1;
        if (result < 0) result = hallway(again);
        again = true;
        if (result == 1) break;
        if (result == 5) again = false;  // (unreachable: the hallway shows the credits itself)
        activity(result);
    }
    blackout();
    ctx_.pump();
}

// --- the library's screen helpers -------------------------------------------

void RockBach::blackout() {
    // f19_0000: display colours 0 black and 255 white; screen 2 black and
    // shown.
    ctx_.blackout();
}

void RockBach::show(int screen) {
    // f19_0082: colour 0 black and 255 white in the screen's palette, which
    // (entries 1-254) goes to the display with its pixels.
    Palette& pal = ctx_.screens[screen].palette;
    pal[0] = Rgb{0, 0, 0};
    pal[255] = Rgb{255, 255, 255};
    ctx_.setDisplayPalette(screen);
    ctx_.screens[1].palette = pal;
    ctx_.screens.copyAll(screen, 1);
}

void RockBach::setColours(const std::vector<Rgb>& colours, int first) {
    // f19_0116: into screen 1's palette (seg 63 + 380) and the display.
    for (size_t k = 0; k < colours.size() && first + k < 256; ++k)
        ctx_.screens[1].palette[first + k] = ctx_.displayPalette[first + k] = colours[k];
}

std::vector<uint8_t> RockBach::soundData(uint16_t id) {
    // f27_039a: the name table at DS:278C (far pointers) by id - 0x6000;
    // <CD>\RB\<name>.wav, else <name>.wav next to the game (the player's
    // own sounds).
    const int index = id - 0x6000;
    if (index < 0 || 0x278C + 4 * index + 1 >= static_cast<int>(data_.size())) return {};
    const std::string name = dataString(dataWord(static_cast<uint16_t>(0x278C + 4 * index)));
    std::ifstream in(findPath(cdRoot_ + "/RB/" + name + ".wav"), std::ios::binary);
    if (!in) in.open(findPath(options_.saveDir + "/" + name + ".wav"), std::ios::binary);
    if (!in) {
        warnOnce("missing sound " + name + ".wav");
        return {};
    }
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

void RockBach::sound(uint16_t id) {
    // f27_020e: longer than 64 KB plays nothing.
    const std::vector<uint8_t> wav = soundData(id);
    if (wav.empty() || wav.size() > 0x10000) return;
    ctx_.platform.playWav(wav);
}

void RockBach::setDriver(int driver) {
    // f02_006e: the driver (and its 72 Hz timer) is swapped.
    // Not in name order: the switch in f02_006e calls ADLIB, ADLIB1, ADLIB3,
    // ADLIB4 and ADLIB2 for drivers 0-4.
    static const char* const kDlls[5] = {"ADLIB.DLL", "ADLIB1.DLL", "ADLIB3.DLL", "ADLIB4.DLL", "ADLIB2.DLL"};
    driver_ = driver;
    if (!options_.music) return;
    ctx_.platform.setFmDriver(driver >= 0 && driver < 5 ? options_.cdDir + "/" + kDlls[driver] : std::string());
}

// --- the intro ----------------------------------------------------------------

void RockBach::corelPresents() {
    // f25_0016: backdrop 100A for 10 s or until a key or click, with
    // colours 70-7F turning one step every 1/8 s.
    const int previous = current();
    select(2);
    blackout();
    sound(0x602A);  // blank
    backdrop(0x100A);
    show(2);
    bool turn = false;  // [52AA]
    ctx_.timer.setPeriodic(8, 8, [&turn] { turn = true; });  // g25_0000
    ctx_.countdown[0] = 100;  // [4F7E]
    clearInput();
    while (ctx_.countdown[0] > 0) {
        ctx_.pump();
        if (turn) {
            std::vector<Rgb> moved(ctx_.displayPalette.begin() + 0x70, ctx_.displayPalette.begin() + 0x7F);
            const Rgb last = ctx_.displayPalette[0x7F];
            setColours(moved, 0x71);
            setColours({last}, 0x70);
            turn = false;
        }
        if (anyInput()) break;
    }
    ctx_.timer.setPeriodic(8, 0, nullptr);
    clearInput();
    select(previous);
}

void RockBach::logo() {
    // f05_04d8: the Rock and Bach Studio logo while a song plays; a
    // spotlight sweeps the stage and a band member (a new one each time the
    // song reaches its next slot) plays below. Colours 1-5F pulse. It ends
    // with the song (after 15 slots) or a key or click.
    static const int kSong[16][2] = {{9, 0}, {9, 11}, {3, 0}, {2, 4}, {2, 6}, {2, 8}, {5, 12}, {5, 15},
                                     {5, 0}, {9, 11}, {9, 12}, {6, 0}, {4, 0}, {4, 4}, {10, 0}, {9, 0}};
    static const int kMembers[16] = {0, 0x13, 0xF, 0x21, 0x16, 0x18, 0x10, 0xF, 9, 1, 0x14, 0x11, 0x1F, 3, 0x13, 0x23};
    std::array<int, 127 * 3> pulse;  // f05_002c's steps: -1 or 1 a component
    pulse.fill(-1);
    musicReset();
    setSong(kSong);
    tempo_ = 0xF0;
    spot_ = Spot{};
    blackout();
    backdrop(0x1001);
    show(2);
    select(1);
    clearInput();
    bool hit = false;  // [8CE7]: a member is playing
    member_ = kMembers[0];
    bool pulseDue = false, frameDue = false;  // [8CE9], [8CE8]
    ctx_.timer.setPeriodic(8, 9, [&frameDue] { frameDue = true; });  // g05_0016
    ctx_.timer.setPeriodic(7, 14, [&pulseDue] { pulseDue = true; });  // g05_0000
    musicPlay();
    ctx_.platform.withFm([](ArtechFmDriver& d) { d.poke(d.getVar(), 0); });
    int slot = 0;
    bool waiting = true;
    ctx_.screens.copyAll(2, 3);
    std::mt19937 rng{std::random_device{}()};
    for (bool done = false; !done;) {
        if (anyInput()) break;
        ctx_.pump();
        if (pulseDue) {
            // f05_002c: every component of colours 1-7F one step up or
            // down, turning at 0 and 255; 1-5F reach the display.
            pulseDue = false;
            std::vector<Rgb> shown(0x5F);
            for (int k = 0; k < 127; ++k) {
                Rgb c = ctx_.screens[1].palette[1 + k];
                uint8_t* comp[3] = {&c.r, &c.g, &c.b};
                for (int j = 0; j < 3; ++j) {
                    int v = *comp[j] + pulse[k * 3 + j];
                    if (v >= 0xFF) v = 0xFF, pulse[k * 3 + j] = -1;
                    if (v <= 0) v = 0, pulse[k * 3 + j] = 1;
                    *comp[j] = static_cast<uint8_t>(v);
                }
                if (k < 0x5F) shown[k] = c;
            }
            setColours(shown, 1);
        }
        if (frameDue) {
            frameDue = false;
            logoFrame();
        }
        // Now and then (1 in 50000 a pass) the member plays anyway, to the
        // crowd.
        const int roll = static_cast<int>(((rng() & 0x7FFF) * 2 + (rng() & 1)) % 50000);
        uint8_t event = 0;
        ctx_.platform.withFm([&event](ArtechFmDriver& d) {
            event = d.peek(d.getVar());
            if (event) d.poke(d.getVar(), 0);
        });
        if (event) {
            if (++slot > 15) {
                slot = 15;
                done = true;
            } else {
                waiting = false;
            }
        }
        if (!waiting) {
            member_ = kMembers[slot];
            waiting = true;
            hit = true;
            memberPlaying_[member_] = true;
            sound(0x6000);  // crowd
        }
        if (roll == 0 && !hit) {
            hit = true;
            memberPlaying_[member_] = true;
            sound(0x6000);
        }
        if (!memberPlaying_[member_]) hit = false;
    }
    memberPlaying_.fill(false);
    memberFrame_.fill(0);
    musicStop();
    ctx_.timer.setPeriodic(8, 0, nullptr);
    ctx_.timer.setPeriodic(7, 0, nullptr);
    clearInput();
}

void RockBach::logoFrame() {
    // f05_03d4: the stage (100, 100, 401 x 301) redrawn from screen 3 on
    // screen 2: the spotlight's beam (bitmap 212F in the polygon from
    // (418, 100) down to the moving spot at y 350), the spot (2130), the
    // member (f05_02c2); then onto the display.
    constexpr int kX = 0x64, kY = 0x64, kW = 0x191, kH = 0x12D;
    select(2);
    copyArea(3, 2, kX, kY, kW, kH);
    fillPolygonWith({{0x1A2, 0x64}, {spot_.left, 0x15E}, {spot_.left + 100, 0x15E}, {0x1A3, 0x64}}, 0x212F);
    drawLogo(spot_.left, 0x147, 0x2130);
    spot_.left += spot_.speed;
    if (spot_.left <= 200) spot_.speed = -spot_.speed, spot_.left = 200;
    if (spot_.left + 100 >= 400) spot_.speed = -spot_.speed, spot_.left = 300;
    // f05_02c2: the member's next frame (frames 1-5 of 2070 + 5m, from its
    // list at DS:194; FE ends it), or idling on frames 1-2.
    int frame = memberFrame_[member_] + 1;
    const uint8_t* list = &data_[0x194 + member_ * 16];
    if (memberPlaying_[member_]) {
        if (static_cast<int8_t>(list[frame]) == -2) {
            memberPlaying_[member_] = false;
            frame = 0;
        }
    } else if (frame >= 2) {
        frame = 0;
    }
    memberFrame_[member_] = frame;
    const uint16_t id = static_cast<uint16_t>(0x206F + list[frame] + member_ * 5);
    const Bitmap& b = ctx_.bitmap(id);
    drawLogo(0x140 - b.width / 2, 0x160 - b.height, id);
    select(1);
    copyArea(2, 1, kX, kY, kW, kH);
}

// --- the activities ----------------------------------------------------------

void RockBach::activity(int which) {
    // f33_0422: each activity with its FM driver.
    static const int kDriver[10] = {-1, -1, 0, 1, 3, -1, 2, -1, -1, 4};
    if (which < 0 || which > 9) return;
    setDriver(kDriver[which]);
    switch (which) {
        case 2:
            bandReset();  // f09_0000
            jukebox();
            bandStop();   // f09_0070
            break;
        case 3:
            drumClinic();
            drumsDone();  // f10_00e6
            break;
        case 4:
            pieceReset();  // f12_0000
            library();
            pieceStop();   // f12_0054
            break;
        case 6:
            harmonyReset();  // f11_0000
            harmonyHall();
            harmonyStop();   // f11_008c
            break;
        case 7:
            instrumentRoom();
            break;
        case 8:
            soundFx();
            break;
        case 9:
            studio();
            break;
        default:
            logLine("Rock and Bach: activity " + std::to_string(which) + " isn't ported yet; back to the hallway.");
            break;
    }
    setDriver(-1);
}

}  // namespace edison
