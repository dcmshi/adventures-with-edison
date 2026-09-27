// WINMAIN.EXE segment 20: songs. A song is 16 slots of 8 sounds each
// (sounds 73 + 8 * slot) in the FM driver's sound table; sound 70 plays them
// in turn (sound 71 points at where it starts, sound 72 is the list). Each
// slot gets a copy of a "track" of 8 riffs (sounds 8 + 8 * track), moved to
// another key and mode (the style) on the way. The game does all this in the
// driver's memory, through GETADDR.

#include "rockbach/rockbach.h"

#include "audio/artech_fm_driver.h"

namespace edison {

void RockBach::musicReset() {
    // f20_0000: no key change, the event byte cleared, tempo C0, every
    // slot back to track 7.
    if (!options_.music) return;
    keyShift_ = modeShift_ = 0;
    ctx_.platform.withFm([](ArtechFmDriver& d) { d.poke(d.getVar(), 0); });
    tempo_ = 0xC0;
    for (int slot = 0; slot < 16; ++slot) setSlot(7, slot, -1);  // f20_0286
}

void RockBach::musicStop() {
    // f20_0248: sound 0 silences; then 0.1 s.
    if (!options_.music) return;
    ctx_.platform.sendFm(0);
    ctx_.countdown[3] = 1;  // [4F84]
    while (ctx_.countdown[3] != 0) ctx_.pump();
}

void RockBach::musicPlay(int from) {
    // f20_0070 (from 0) / f20_00e8: sound 71 points into the slot list
    // (sound 72, 0x18 bytes a slot), sound 6's tempo byte is set, and the
    // event byte says where it starts.
    if (!options_.music) return;
    const uint8_t tempo = tempo_;
    ctx_.platform.withFm([from, tempo](ArtechFmDriver& d) {
        d.pokeWord(d.soundAddress(0x71), static_cast<uint16_t>(d.soundAddress(0x72) + from * 0x18));
        d.poke(static_cast<uint16_t>(d.soundAddress(6) + 1), tempo);
        d.poke(d.getVar(), static_cast<uint8_t>(from));
        d.sendSound(0x70);
    });
}

void RockBach::musicPlaySlot(int slot) {
    // g20_016c: one slot's 8 sounds at once.
    if (!options_.music) return;
    const uint8_t tempo = tempo_;
    ctx_.platform.withFm([slot, tempo](ArtechFmDriver& d) {
        d.sendSound(0);
        d.poke(static_cast<uint16_t>(d.soundAddress(6) + 1), tempo);
        d.sendSound(7);
        for (int k = 0; k < 8; ++k) d.sendSound(static_cast<uint16_t>(slot * 8 + 0x73 + k));
    });
}

void RockBach::setSong(const int tracks[16][2]) {
    // f20_0386: slot i gets track tracks[i][0] in style tracks[i][1]
    // (a negative track leaves the slot alone).
    for (int slot = 0; slot < 16; ++slot)
        if (tracks[slot][0] >= 0) setSlot(tracks[slot][0], slot, tracks[slot][1]);
}

void RockBach::setSlot(int track, int slot, int style) {
    // f20_03fa / f20_0532: the style picks a key (DS:20B8, first word) and
    // a mode (second word). Riffs 0-3 and 7 of the 8 are moved; 4-6 (the
    // drums, presumably) aren't.
    if (!options_.music) return;
    keyShift_ = style < 0 ? 0 : dataWord(static_cast<uint16_t>(0x20B8 + style * 4));
    modeShift_ = style < 0 ? 0 : dataWord(static_cast<uint16_t>(0x20BA + style * 4));
    const int key = keyShift_, mode = modeShift_;
    const std::vector<uint8_t>& data = data_;
    ctx_.platform.withFm([track, slot, key, mode, &data](ArtechFmDriver& d) {
        for (int i = 0; i < 8; ++i) {
            const uint16_t to = d.soundAddress(static_cast<uint16_t>(slot * 8 + 0x73 + i));
            const uint16_t from = d.soundAddress(static_cast<uint16_t>(track * 8 + 8 + i));
            for (int k = 0; k < 0x100; ++k)
                d.poke(static_cast<uint16_t>(to + k), d.peek(static_cast<uint16_t>(from + k)));
            if ((i > 3 && i < 7) || (key == 0 && mode == 0)) continue;
            // f20_0618: past the 12-byte header, notes (bytes up to 7B, 4
            // bytes each) are moved; commands (2 bytes) are skipped, up to
            // the end (84 or 88).
            uint16_t p = static_cast<uint16_t>(to + 0xC);
            for (int guard = 0; guard < 0x100; ++guard) {
                const uint8_t b = d.peek(p);
                if (b > 0x7B) {
                    p = static_cast<uint16_t>(p + 2);
                } else {
                    const int semitone = b & 0xF;
                    int n = (b >> 4) * 12 + semitone;
                    if (b != 0x30) n += static_cast<int8_t>(data[0x2104 + mode * 12 + semitone]);
                    n += static_cast<int8_t>(data[0x20F8 + key]);
                    n = static_cast<int8_t>(n);
                    d.poke(p, static_cast<uint8_t>((n / 12) << 4 | (n % 12)));
                    p = static_cast<uint16_t>(p + 4);
                }
                const uint8_t last = d.peek(static_cast<uint16_t>(p - 2));
                if (last == 0x84 || last == 0x88) break;
            }
        }
    });
}

}  // namespace edison
