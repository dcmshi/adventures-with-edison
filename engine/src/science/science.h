#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "artech/game.h"

namespace edison {

// The Wild Science Arcade (WMAIN.EXE): see docs/SCIENCE.md. So far the
// title and the story; the lab and the arcade's rooms come next.
class Science : public ArtechGame {
public:
    struct Options {
        std::string cdDir;              // the CD's DSK3 folder (WMAIN.EXE, GRAFX.DAT, S*.SRF)
        bool music = true;              // not -A
        std::string saveDir = "save";   // wscience.edi, wscience.hs
    };

    explicit Science(Platform& platform) : ArtechGame(platform) {}

    bool load(const Options& options, std::string* error);
    // Plays until the player leaves (the original then runs "edison.exe -O").
    void run();

private:
    // --- the framework's helpers ---
    void showScreen(uint16_t picture, int screen);  // f63_09d7: a full screen and its palette
    // f14_092c: the same with Edison's look in its colours E1-ED (the
    // tables at seg95:004F, 0097, 00DF, 013F; the look at DS:1C56).
    void showScreenWithLook(uint16_t picture, int screen);
    void toDisplay(int screen);                     // f20_00f3: the screen and its palette shown
    void clearDisplay();                            // f20_0094: screen 3 in colour 2, shown
    void fmSound(uint16_t sound);                   // f32_135d: SADLIB's SENDSND
    // f36_00ad: <CD>\science\<name>.wav, else data\<name>.wav; `story`
    // picks the names at seg97:0519 (else seg97:0000).
    void narration(int n, bool story);
    void waitNarration();                           // while [92BC]
    // A wait of `ticks` 50ths of a second (f32_07aa, the 50 Hz counter
    // [12F8:0002] that f32_0777(50) starts); a click or key ends it.
    bool waitTicks(int ticks);
    bool escapePressed();                           // bit 1 of the keys held (DS:9560)

    // --- the opening (segments 32 and 38) ---
    void title();                                   // f32_0319 with [26CE] set
    void story();                                   // f38_0718

    std::string dataString(uint16_t offset) const;  // DGROUP (segment 103)

    Options options_;
    std::string cdRoot_;               // the CD's root (its SCIENCE folder of sounds)
    std::vector<uint8_t> data_;        // DGROUP
    std::vector<uint8_t> strings_;     // segment 97: the sounds' names
    std::vector<uint8_t> looks_;       // segment 95: the look's colour tables
    uint8_t look_[4] = {};             // DS:1C56: hair, face, shirt, trousers
};

}  // namespace edison
