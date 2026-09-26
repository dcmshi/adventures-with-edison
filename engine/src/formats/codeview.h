#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace edison {

// Reader for the CodeView 4 (NB09) debug info Artech left in its DLLs.
// Symbols are returned at their physical NE segment:offset.
struct CodeViewSymbol {
    uint16_t segment;
    uint16_t offset;
    std::string name;
};

// Parses the debug info appended to an executable's raw bytes. Returns an
// empty list when the file has no (or an unsupported) CodeView block.
std::vector<CodeViewSymbol> readCodeViewSymbols(const std::vector<uint8_t>& file);

// Convenience: map of (segment, offset) -> name.
std::map<std::pair<uint16_t, uint16_t>, std::string> codeViewNameMap(const std::vector<uint8_t>& file);

}  // namespace edison
