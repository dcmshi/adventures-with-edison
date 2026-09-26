// Replays every sound of every ADLIB-family DLL through the native driver and
// compares the OPL register stream with the reference logs recorded from the
// original driver by tools/oplref.py (same order, tempo and tick limit, since
// driver state carries over between sounds there too).
//
// Usage: seqtest <cd/DSK3 dir> <extracted/oplref dir> [DLL ...] [--verbose]

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "audio/artech_fm_driver.h"

namespace {

constexpr uint8_t kTempo = 0x80;  // tools/oplref.py default
constexpr int kMaxTicks = 6000;

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

std::vector<std::string> readLines(const std::string& path, bool* ok) {
    std::ifstream in(path);
    *ok = static_cast<bool>(in);
    std::vector<std::string> lines;
    for (std::string line; std::getline(in, line);) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(line);
    }
    return lines;
}

struct Result {
    int passed = 0;
    int failed = 0;
    int missing = 0;
};

Result testDll(const std::string& cdDir, const std::string& refDir, const std::string& dll, bool verbose) {
    Result r;
    edison::ArtechFmDriver driver;
    std::string error;
    if (!driver.loadDll(cdDir + "/" + dll + ".DLL", &error)) {
        std::printf("%s: load failed: %s\n", dll.c_str(), error.c_str());
        r.failed = 1;
        return r;
    }
    int tick = 0;
    std::vector<std::string> log;
    driver.setOplWrite([&](uint8_t reg, uint8_t value) {
        char buf[32];
        std::snprintf(buf, sizeof buf, "%d %02x %02x", tick, reg, value);
        log.emplace_back(buf);
    });

    driver.init();
    const auto sounds = driver.listSounds().sounds;
    int shown = 0;
    std::vector<uint16_t> oddOpcodes;
    for (uint16_t id : sounds) {
        const int unknownBefore = driver.unknownOpcodes();
        driver.init();
        driver.setGlobalTempo(kTempo);
        log.clear();
        tick = 0;
        driver.sendSound(id);
        while (tick < kMaxTicks) {
            ++tick;
            driver.update();
            if (driver.idle()) break;
        }

        if (driver.unknownOpcodes() != unknownBefore) oddOpcodes.push_back(id);

        char name[16];
        std::snprintf(name, sizeof name, "%03d.txt", id);
        bool ok = false;
        const auto expected = readLines(refDir + "/" + lower(dll) + "/" + name, &ok);
        if (!ok) {
            ++r.missing;
            continue;
        }
        if (expected == log) {
            ++r.passed;
            continue;
        }
        ++r.failed;
        if (verbose || shown < 3) {
            ++shown;
            size_t i = 0;
            while (i < expected.size() && i < log.size() && expected[i] == log[i]) ++i;
            std::printf("  %s #%d: first difference at write %zu of %zu (native has %zu)\n",
                        dll.c_str(), id, i, expected.size(), log.size());
            for (size_t k = (i >= 2 ? i - 2 : 0); k < i + 3; ++k) {
                std::printf("    %-14s | %s\n", k < expected.size() ? expected[k].c_str() : "-",
                            k < log.size() ? log[k].c_str() : "-");
            }
        }
    }
    std::printf("%-7s %3d/%zu match%s", dll.c_str(), r.passed, sounds.size(),
                r.missing ? " (reference logs missing for some)" : "");
    if (!oddOpcodes.empty()) {
        std::printf("  [opcodes past the jump table in sound");
        for (uint16_t id : oddOpcodes) std::printf(" %d", id);
        std::printf("]");
    }
    std::printf("\n");
    return r;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s <cd/DSK3 dir> <oplref dir> [DLL ...] [--verbose]\n", argv[0]);
        return 2;
    }
    std::vector<std::string> dlls;
    bool verbose = false;
    for (int i = 3; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--verbose") verbose = true;
        else dlls.push_back(arg);
    }
    if (dlls.empty()) dlls = {"ADLIB", "ADLIB1", "ADLIB2", "ADLIB3", "ADLIB4", "CADLIB", "MADLIB"};

    Result total;
    for (const auto& dll : dlls) {
        const Result r = testDll(argv[1], argv[2], dll, verbose);
        total.passed += r.passed;
        total.failed += r.failed;
        total.missing += r.missing;
    }
    std::printf("TOTAL: %d passed, %d failed, %d without reference\n", total.passed, total.failed, total.missing);
    return (total.failed == 0 && total.missing == 0) ? 0 : 1;
}
