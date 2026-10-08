// Replays every sound of every ADLIB-family DLL through the native driver and
// compares the OPL register stream with the reference logs recorded from the
// original driver by tools/oplref.py (same order, tempo and tick limit, since
// driver state carries over between sounds there too).
//
// Usage: seqtest <cd/DSK3 dir> <extracted/oplref dir> [DLL ...] [--cases <dir> [--cases-only]]
//                [--trace <file>] [--verbose]
//   --cases: also run the differential cases made by tools/oplfuzz.py

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <memory>
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

// Opcode executions summed over every DLL, to report untested opcodes.
std::array<uint64_t, 0x53> gCoverage{};

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
    for (size_t op = 0; op < gCoverage.size(); ++op) gCoverage[op] += driver.opcodeCounts()[op];
    return r;
}

// Differential cases from tools/oplfuzz.py: synthetic songs injected into the
// data segment, with the original driver's output as the expectation.
// --trace: write the driver state (variables and channel blocks, 01E0-052F)
// after every tick, for diffing against tools/oplfuzz.py --trace.
std::string gTracePath;
constexpr uint16_t kTraceStart = 0x1E0, kTraceEnd = 0x530;

Result runCases(const std::string& cdDir, const std::string& casesDir, bool verbose) {
    Result r;
    std::vector<std::filesystem::path> files;
    if (!std::filesystem::exists(casesDir)) {
        std::printf("cases: %s not found (generate with tools/oplfuzz.py)\n", casesDir.c_str());
        r.missing = 1;
        return r;
    }
    if (std::filesystem::is_regular_file(casesDir)) {
        files.push_back(casesDir);
    } else {
        for (const auto& e : std::filesystem::recursive_directory_iterator(casesDir))
            if (e.is_regular_file() && e.path().extension() == ".txt") files.push_back(e.path());
    }
    std::sort(files.begin(), files.end());
    std::ofstream trace;
    if (!gTracePath.empty()) trace.open(gTracePath);

    int shown = 0;
    for (const auto& file : files) {
        std::ifstream in(file);
        std::string dll, line;
        int tempo = 0x80, maxTicks = 1500;
        std::vector<std::pair<uint16_t, std::vector<uint8_t>>> pokes;
        std::vector<uint16_t> sends;
        std::vector<std::string> expected;
        bool inExpect = false;
        while (std::getline(in, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (inExpect) { expected.push_back(line); continue; }
            std::istringstream s(line);
            std::string key;
            s >> key;
            if (key == "dll") s >> dll;
            else if (key == "tempo") s >> tempo;
            else if (key == "maxticks") s >> maxTicks;
            else if (key == "poke") {
                std::string addr, byte;
                s >> addr;
                std::vector<uint8_t> bytes;
                while (s >> byte) bytes.push_back(static_cast<uint8_t>(std::stoul(byte, nullptr, 16)));
                pokes.emplace_back(static_cast<uint16_t>(std::stoul(addr, nullptr, 16)), bytes);
            } else if (key == "send") {
                for (int id; s >> id;) sends.push_back(static_cast<uint16_t>(id));
            } else if (key == "expect") {
                inExpect = true;
            }
        }

        auto driver = std::make_unique<edison::ArtechFmDriver>();
        std::string error;
        if (!driver->loadDll(cdDir + "/" + dll, &error)) {
            std::printf("  %s: %s\n", file.string().c_str(), error.c_str());
            ++r.failed;
            continue;
        }
        int tick = 0;
        std::vector<std::string> log;
        driver->setOplWrite([&](uint8_t reg, uint8_t value) {
            char buf[32];
            std::snprintf(buf, sizeof buf, "%d %02x %02x", tick, reg, value);
            log.emplace_back(buf);
        });
        driver->init();
        driver->setGlobalTempo(static_cast<uint8_t>(tempo));
        for (const auto& [addr, bytes] : pokes)
            for (size_t i = 0; i < bytes.size(); ++i) driver->poke(static_cast<uint16_t>(addr + i), bytes[i]);
        log.clear();
        for (uint16_t id : sends) driver->sendSound(id);
        while (tick < maxTicks) {
            ++tick;
            driver->update();
            if (trace.is_open()) {
                trace << tick;
                char hex[4];
                for (uint16_t a = kTraceStart; a < kTraceEnd; ++a) {
                    std::snprintf(hex, sizeof hex, " %02x", driver->peek(a));
                    trace << hex;
                }
                trace << '\n';
            }
            if (driver->idle()) break;
        }
        for (size_t op = 0; op < gCoverage.size(); ++op) gCoverage[op] += driver->opcodeCounts()[op];

        if (log == expected) {
            ++r.passed;
            continue;
        }
        ++r.failed;
        if (verbose || shown < 5) {
            ++shown;
            size_t i = 0;
            while (i < expected.size() && i < log.size() && expected[i] == log[i]) ++i;
            std::printf("  %s: first difference at write %zu of %zu (native has %zu)\n",
                        (file.parent_path().filename() / file.filename()).string().c_str(), i,
                        expected.size(), log.size());
            for (size_t k = (i >= 2 ? i - 2 : 0); k < i + 3; ++k) {
                std::printf("    %-14s | %s\n", k < expected.size() ? expected[k].c_str() : "-",
                            k < log.size() ? log[k].c_str() : "-");
            }
        }
    }
    std::printf("cases   %3d/%zu match\n", r.passed, files.size());
    return r;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s <cd/DSK3 dir> <oplref dir> [DLL ...] [--verbose]\n", argv[0]);
        return 2;
    }
    std::vector<std::string> dlls;
    std::string casesDir;
    bool verbose = false;
    bool casesOnly = false;
    for (int i = 3; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--verbose") verbose = true;
        else if (arg == "--cases" && i + 1 < argc) casesDir = argv[++i];
        else if (arg == "--cases-only") casesOnly = true;
        else if (arg == "--trace" && i + 1 < argc) gTracePath = argv[++i];
        else dlls.push_back(arg);
    }
    if (dlls.empty() && !casesOnly) dlls = {"ADLIB", "ADLIB1", "ADLIB2", "ADLIB3", "ADLIB4", "CADLIB", "MADLIB", "SADLIB"};

    Result total;
    auto add = [&total](const Result& r) {
        total.passed += r.passed;
        total.failed += r.failed;
        total.missing += r.missing;
    };
    for (const auto& dll : dlls) add(testDll(argv[1], argv[2], dll, verbose));
    if (!casesDir.empty()) add(runCases(argv[1], casesDir, verbose));
    std::printf("TOTAL: %d passed, %d failed, %d without reference\n", total.passed, total.failed, total.missing);

    // Opcodes with a handler (the rest of the table points at EOS).
    static const uint8_t kDefined[] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E,
        0x10, 0x11, 0x12, 0x13, 0x15, 0x1A, 0x1C, 0x1D, 0x1E, 0x20, 0x21, 0x24, 0x26, 0x27, 0x29,
        0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x33, 0x35, 0x36, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3F,
        0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x48, 0x49, 0x50, 0x51, 0x52};
    std::string untested;
    int covered = 0;
    for (uint8_t op : kDefined) {
        if (gCoverage[op] != 0) {
            ++covered;
        } else {
            char buf[8];
            std::snprintf(buf, sizeof buf, " %02X", op);
            untested += buf;
        }
    }
    std::printf("opcode coverage: %d/%zu defined opcodes exercised%s%s\n", covered,
                sizeof kDefined, untested.empty() ? "" : "; never used:", untested.c_str());
    return (total.failed == 0 && total.missing == 0) ? 0 : 1;
}
