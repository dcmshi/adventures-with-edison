// Unpacks every entry of every archive on the CD with the native DCL
// decompressor and decodes every bitmap, checking sizes against the
// directories (a check of formats/dcl, formats/archive, formats/bitmap).
//
// Usage: formattest <cd/DSK3 dir>

#include <cstdio>
#include <string>
#include <vector>

#include "formats/archive.h"
#include "formats/bitmap.h"

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <cd/DSK3 dir>\n", argv[0]);
        return 2;
    }
    int failed = 0;
    for (const char* name : {"SHELL.D01", "RB.D01", "MYSTERY.D01", "GRAFX.DAT"}) {
        edison::Archive archive;
        std::string error;
        if (!archive.open(std::string(argv[1]) + "/" + name, &error)) {
            std::printf("%s: %s\n", name, error.c_str());
            ++failed;
            continue;
        }
        int entries = 0, bitmaps = 0, bad = 0;
        std::vector<uint8_t> data;
        for (uint16_t id : archive.ids()) {
            ++entries;
            if (!archive.read(id, data, &error)) {
                if (bad++ < 5) std::printf("  %s %04x: %s\n", name, id, error.c_str());
                continue;
            }
            if (data.size() >= 2 && data[0] == 'B' && data[1] == 'M') {
                edison::Bitmap bmp;
                if (!edison::decodeBmp(data, bmp, &error)) {
                    if (bad++ < 5) std::printf("  %s %04x: %s\n", name, id, error.c_str());
                    continue;
                }
                ++bitmaps;
            }
        }
        std::printf("%-12s %4d entries, %4d bitmaps, %d failed\n", name, entries, bitmaps, bad);
        failed += bad;
    }
    return failed == 0 ? 0 : 1;
}
