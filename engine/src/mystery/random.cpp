// MALL.EXE segment 46: the game's random numbers.
//
// f46_0000 is an additive lagged generator on six words (DS:7638-7643): the
// sum, with carries, of the first five becomes the sixth and the others
// shift down. Nothing ever seeds it: it starts from the data segment's own
// words, and each change of screen (f04_005c) stirs it by drawing up to 199
// more (f06_2cf8). So the original deals the same boards and puzzles for
// the same play, and the port, drawing in the same order, deals the same.

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "mystery/mystery.h"

namespace edison {

namespace {

uint16_t state[6];
FILE* rngLog = nullptr;  // EDISON_RNGLOG: each draw (for testing)

}  // namespace

void seedRandom(const uint8_t* words) {
    for (int i = 0; i < 6; ++i) state[i] = static_cast<uint16_t>(words[2 * i] | words[2 * i + 1] << 8);
    // EDISON_RNG: 24 hex digits, the six words as memory has them (for
    // testing: the original's state read with memwatch.py).
    if (const char* s = std::getenv("EDISON_RNG"); s && std::strlen(s) == 24) {
        uint8_t b[12];
        for (int i = 0; i < 12; ++i) {
            unsigned v = 0;
            std::sscanf(s + 2 * i, "%2x", &v);
            b[i] = static_cast<uint8_t>(v);
        }
        for (int i = 0; i < 6; ++i) state[i] = static_cast<uint16_t>(b[2 * i] | b[2 * i + 1] << 8);
    }
    if (const char* path = std::getenv("EDISON_RNGLOG")) rngLog = std::fopen(path, "w");
}

uint16_t rand16() {
    unsigned sum = 0, carry = 0;
    for (int i = 0; i < 5; ++i) {
        sum += state[i] + carry;  // adc
        carry = sum >> 16;
        sum &= 0xFFFF;
        state[i] = state[i + 1];
    }
    state[5] = static_cast<uint16_t>(sum);
    return state[5];
}

int random(int n) {
    const uint16_t r = rand16();
    int v = r;
    if (n < 0x10000) v = n > 0 ? r % n : 0;  // (div leaves dx 0 for n = 0)
    if (rngLog) {  // n, the value, and the six words after
        std::fprintf(rngLog, "%d %d", n, v);
        for (uint16_t w : state) std::fprintf(rngLog, " %04x", w);
        std::fprintf(rngLog, "\n");
        std::fflush(rngLog);
    }
    return v;
}

void stirRandom() {
    for (int k = random(200); k > 0; --k) rand16();
}

}  // namespace edison
