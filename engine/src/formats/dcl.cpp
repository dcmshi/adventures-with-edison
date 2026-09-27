#include "formats/dcl.h"

#include <array>

namespace edison {
namespace {

constexpr int kMaxBits = 13;

// Compact code-length tables from blast.c: each byte is (repeat - 1) << 4 | length.
constexpr uint8_t kLitLen[] = {
    11, 124, 8, 7, 28, 7, 188, 13, 76, 4, 10, 8, 12, 10, 12, 10, 8, 23, 8,
    9, 7, 6, 7, 8, 7, 6, 55, 8, 23, 24, 12, 11, 7, 9, 11, 12, 6, 7, 22, 5,
    7, 24, 6, 11, 9, 6, 7, 22, 7, 11, 38, 7, 9, 8, 25, 11, 8, 11, 9, 12,
    8, 12, 5, 38, 5, 38, 5, 11, 7, 5, 6, 21, 6, 10, 53, 8, 7, 24, 10, 27,
    44, 253, 253, 253, 252, 252, 252, 13, 12, 45, 12, 45, 12, 61, 12, 45,
    44, 173};
constexpr uint8_t kLenLen[] = {2, 35, 36, 53, 38, 23};
constexpr uint8_t kDistLen[] = {2, 20, 53, 230, 247, 151, 248};
constexpr uint16_t kBase[16] = {3, 2, 4, 5, 6, 7, 8, 9, 10, 12, 16, 24, 40, 72, 136, 264};
constexpr uint8_t kExtra[16] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8};

struct Huffman {
    std::array<uint16_t, kMaxBits + 1> count{};
    std::array<uint16_t, 256> symbol{};

    template <size_t N>
    explicit Huffman(const uint8_t (&compact)[N]) {
        std::array<uint8_t, 256> lengths{};
        size_t n = 0;
        for (uint8_t b : compact)
            for (int r = 0; r <= (b >> 4); ++r) lengths[n++] = b & 15;
        for (size_t s = 0; s < n; ++s) ++count[lengths[s]];
        std::array<uint16_t, kMaxBits + 1> offs{};
        for (int len = 1; len < kMaxBits; ++len) offs[len + 1] = offs[len] + count[len];
        for (size_t s = 0; s < n; ++s)
            if (lengths[s]) symbol[offs[lengths[s]]++] = static_cast<uint16_t>(s);
    }
};

const Huffman& litCode() { static const Huffman h(kLitLen); return h; }
const Huffman& lenCode() { static const Huffman h(kLenLen); return h; }
const Huffman& distCode() { static const Huffman h(kDistLen); return h; }

class BitReader {
public:
    BitReader(const uint8_t* data, size_t size) : data_(data), size_(size) {}

    bool bits(int need, int* value) {
        while (count_ < need) {
            if (pos_ >= size_) return false;
            buffer_ |= static_cast<uint32_t>(data_[pos_++]) << count_;
            count_ += 8;
        }
        *value = static_cast<int>(buffer_ & ((1u << need) - 1));
        buffer_ >>= need;
        count_ -= need;
        return true;
    }

    // DCL stores Huffman codes bit-inverted.
    bool decode(const Huffman& h, int* symbol) {
        int code = 0, first = 0, index = 0;
        for (int len = 1; len <= kMaxBits; ++len) {
            int bit;
            if (!bits(1, &bit)) return false;
            code |= bit ^ 1;
            const int count = h.count[len];
            if (code < first + count) {
                *symbol = h.symbol[index + code - first];
                return true;
            }
            index += count;
            first = (first + count) << 1;
            code <<= 1;
        }
        return false;
    }

private:
    const uint8_t* data_;
    size_t size_;
    size_t pos_ = 0;
    uint32_t buffer_ = 0;
    int count_ = 0;
};

bool fail(std::string* error, const char* message) {
    if (error) *error = message;
    return false;
}

}  // namespace

bool dclExplode(const uint8_t* data, size_t size, std::vector<uint8_t>& out, std::string* error) {
    BitReader in(data, size);
    int literalCoded, dictBits;
    if (!in.bits(8, &literalCoded) || literalCoded > 1) return fail(error, "bad literal flag");
    if (!in.bits(8, &dictBits) || dictBits < 4 || dictBits > 6) return fail(error, "bad dictionary size");
    const size_t start = out.size();
    for (;;) {
        int flag;
        if (!in.bits(1, &flag)) return fail(error, "unexpected end of input");
        if (flag) {
            int symbol, extra;
            if (!in.decode(lenCode(), &symbol) || !in.bits(kExtra[symbol], &extra))
                return fail(error, "bad length code");
            const int length = kBase[symbol] + extra;
            if (length == 519) return true;  // end-of-stream marker
            const int shift = length == 2 ? 2 : dictBits;
            int high, low;
            if (!in.decode(distCode(), &high) || !in.bits(shift, &low)) return fail(error, "bad distance code");
            const size_t dist = (static_cast<size_t>(high) << shift) + low + 1;
            if (dist > out.size() - start) return fail(error, "distance too far back");
            const size_t from = out.size() - dist;
            for (int i = 0; i < length; ++i) out.push_back(out[from + i]);  // copies may overlap
        } else {
            int value;
            if (literalCoded ? !in.decode(litCode(), &value) : !in.bits(8, &value))
                return fail(error, "bad literal");
            out.push_back(static_cast<uint8_t>(value));
        }
    }
}

}  // namespace edison
