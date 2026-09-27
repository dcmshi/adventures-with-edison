// WINMAIN.EXE's top level (segment 33) and the intro (segments 5 and 25).

#include "rockbach/rockbach.h"

#include <cstdio>
#include <fstream>
#include <iterator>

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

void RockBach::sound(uint16_t id) {
    // f27_020e: the name table at DS:278C (far pointers) by id - 0x6000;
    // <CD>\RB\<name>.wav, else <name>.wav next to the game (the player's
    // own sounds). Longer than 64 KB plays nothing.
    const int index = id - 0x6000;
    if (index < 0 || 0x278C + 4 * index + 1 >= static_cast<int>(data_.size())) return;
    const std::string name = dataString(dataWord(static_cast<uint16_t>(0x278C + 4 * index)));
    std::ifstream in(cdRoot_ + "/RB/" + name + ".wav", std::ios::binary);
    if (!in) in.open(options_.saveDir + "/" + name + ".wav", std::ios::binary);
    if (!in) {
        warnOnce("missing sound " + name + ".wav");
        return;
    }
    const std::vector<uint8_t> wav((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (wav.size() > 0x10000) return;
    ctx_.platform.playWav(wav);
}

void RockBach::setDriver(int driver) {
    // f02_006e: the driver (and its 72 Hz timer) is swapped.
    static const char* const kDlls[5] = {"ADLIB.DLL", "ADLIB1.DLL", "ADLIB2.DLL", "ADLIB3.DLL", "ADLIB4.DLL"};
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
    // f05_04d8 (the band playing under the Rock and Bach Studio logo) isn't
    // ported yet: the logo alone, until a key or click.
    blackout();
    backdrop(0x1001);
    show(2);
    select(1);
    clearInput();
    ctx_.countdown[0] = 50;
    while (ctx_.countdown[0] > 0 && !anyInput()) ctx_.pump();
    clearInput();
}

// --- the hallway ----------------------------------------------------------------

int RockBach::hallway(bool again) {
    // f24_1d4a. (Edison's greeting, the player's name and looks the first
    // time, and the animated sign aren't ported yet.)
    (void)again;
    blackout();
    backdrop(0x1007);
    show(2);
    select(1);
    backdrop(0x1006);  // the hot-spot mask, on screen 2
    clearInput();
    for (;;) {
        ctx_.pump();
        int x, y;
        if (!ctx_.platform.takeClick(&x, &y)) continue;
        if (x < 0 || y < 0 || x >= Screen::kWidth || y >= Screen::kHeight) continue;
        const int spot = ctx_.screens[2].pixels[static_cast<size_t>(y) * Screen::kWidth + x];  // f37_23ce
        if (spot >= 1 && spot != 5 && spot != 10) return spot;
        clearInput();
    }
}

void RockBach::activity(int which) {
    // f33_0422: each activity with its FM driver. None is ported yet.
    static const int kDriver[10] = {-1, -1, 0, 1, 3, -1, 2, -1, -1, 4};
    if (which < 0 || which > 9) return;
    setDriver(kDriver[which]);
    std::printf("Rock and Bach: activity %d isn't ported yet; back to the hallway.\n", which);
    setDriver(-1);
}

}  // namespace edison
