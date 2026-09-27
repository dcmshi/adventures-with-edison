#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace edison {

// PKWARE Data Compression Library "implode" decompressor (a port of Mark
// Adler's blast.c). Every entry of Artech's archives is stored this way.
// Appends the decompressed bytes to `out`.
bool dclExplode(const uint8_t* data, size_t size, std::vector<uint8_t>& out, std::string* error);

}  // namespace edison
