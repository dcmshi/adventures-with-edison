#include "formats/codeview.h"

namespace edison {

namespace {

uint16_t u16(const std::vector<uint8_t>& d, size_t at) {
    return static_cast<uint16_t>(d[at] | (d[at + 1] << 8));
}

uint32_t u32(const std::vector<uint8_t>& d, size_t at) {
    return u16(d, at) | (static_cast<uint32_t>(u16(d, at + 2)) << 16);
}

std::string pascalString(const std::vector<uint8_t>& d, size_t at) {
    if (at >= d.size()) return {};
    const size_t n = d[at];
    if (at + 1 + n > d.size()) return {};
    return std::string(d.begin() + static_cast<long>(at + 1), d.begin() + static_cast<long>(at + 1 + n));
}

constexpr uint16_t kSegMap = 0x12D;

bool isSymbolSection(uint16_t sst) {
    return (sst >= 0x122 && sst <= 0x125) || sst == 0x129 || sst == 0x12A || sst == 0x134;
}

size_t symbolStart(uint16_t sst) {
    if (sst == 0x124 || sst == 0x125) return 4;                // module tables: signature
    if (sst == 0x129 || sst == 0x12A || sst == 0x134) return 8;  // global tables: hash header
    return 0;
}

}  // namespace

std::vector<CodeViewSymbol> readCodeViewSymbols(const std::vector<uint8_t>& file) {
    std::vector<CodeViewSymbol> out;
    if (file.size() < 16) return out;
    const size_t tail = file.size() - 8;
    if (file[tail] != 'N' || file[tail + 1] != 'B' || file[tail + 2] != '0' || file[tail + 3] != '9') return out;
    const uint32_t size = u32(file, tail + 4);
    if (size > file.size() || size < 16) return out;
    const std::vector<uint8_t> cv(file.end() - size, file.end());
    if (cv[0] != 'N' || cv[1] != 'B') return out;

    const uint32_t dir = u32(cv, 4);
    if (dir + 8 > cv.size()) return out;
    const uint16_t headerSize = u16(cv, dir);
    const uint16_t entrySize = u16(cv, dir + 2);
    const uint32_t count = u32(cv, dir + 4);

    std::map<uint16_t, std::pair<uint16_t, uint32_t>> segmap;  // logical -> (frame, offset)
    std::vector<CodeViewSymbol> logical;
    for (uint32_t i = 0; i < count; ++i) {
        const size_t e = dir + headerSize + static_cast<size_t>(i) * entrySize;
        if (e + 12 > cv.size()) break;
        const uint16_t sst = u16(cv, e);
        const uint32_t lfo = u32(cv, e + 4);
        const uint32_t cb = u32(cv, e + 8);
        if (lfo + cb > cv.size()) continue;
        const std::vector<uint8_t> sub(cv.begin() + lfo, cv.begin() + lfo + cb);

        if (sst == kSegMap && sub.size() >= 4) {
            const uint16_t segs = u16(sub, 0);
            for (uint16_t n = 0; n < segs && 4 + (n + 1u) * 20 <= sub.size(); ++n) {
                const size_t s = 4 + n * 20u;
                segmap[static_cast<uint16_t>(n + 1)] = {u16(sub, s + 6), u32(sub, s + 12)};
            }
        } else if (isSymbolSection(sst)) {
            size_t pos = symbolStart(sst);
            while (pos + 4 <= sub.size()) {
                const uint16_t reclen = u16(sub, pos);
                const uint16_t type = u16(sub, pos + 2);
                if (reclen < 2) break;
                const size_t body = pos + 4;
                if ((type >= 0x0101 && type <= 0x0103) || type == 0x0109) {  // data / public / label
                    if (body + 4 <= sub.size()) {
                        const size_t nameAt = type == 0x0109 ? body + 5 : body + 6;
                        logical.push_back({u16(sub, body + 2), u16(sub, body), pascalString(sub, nameAt)});
                    }
                } else if (type == 0x0104 || type == 0x0105) {  // procedures
                    const size_t at = body + 18;
                    if (at + 4 <= sub.size())
                        logical.push_back({u16(sub, at + 2), u16(sub, at), pascalString(sub, at + 7)});
                }
                pos += reclen + 2u;
            }
        }
    }

    for (const auto& s : logical) {
        auto it = segmap.find(s.segment);
        if (it == segmap.end()) {
            out.push_back(s);
        } else {
            out.push_back({it->second.first, static_cast<uint16_t>(it->second.second + s.offset), s.name});
        }
    }
    return out;
}

std::map<std::pair<uint16_t, uint16_t>, std::string> codeViewNameMap(const std::vector<uint8_t>& file) {
    std::map<std::pair<uint16_t, uint16_t>, std::string> names;
    for (const auto& s : readCodeViewSymbols(file)) names.emplace(std::make_pair(s.segment, s.offset), s.name);
    return names;
}

}  // namespace edison
