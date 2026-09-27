// Segments 47-51: the fixed-point 3D helpers of the Liberty Planetarium,
// the Folded Cube and the 3D Ball Sculpture. Angles are 16-bit (a turn is
// 0x10000), sines Q15; y is the depth.

#include <algorithm>

#include "mystery/mystery.h"

namespace edison {

namespace {

// The high word of twice a 32-bit product sum: a Q15 multiply.
// The sum wraps at 32 bits, as the original's register pair does.
int16_t q15(int64_t sum) {
    const uint32_t doubled = static_cast<uint32_t>(sum) << 1;
    return static_cast<int16_t>(doubled >> 16);
}

}  // namespace

void Mystery::sinCos(uint16_t angle, int* sine, int* cosine) const {
    // g51_1000: segment 51 starts with a quarter sine of 2048 words.
    auto word = [&](int at) { return sine_[at] | sine_[at + 1] << 8; };
    const int at = (angle >> 2) & 0xFFE;
    int s = word(at) >> 1, c = word(0xFFE - at) >> 1;
    const int quadrant = angle >> 14;
    if (quadrant & 1) std::swap(s, c);
    if (quadrant == 1 || quadrant == 2) c = -c;
    if (quadrant & 2) s = -s;
    *sine = s;
    *cosine = c;
}

void Mystery::rotation(uint16_t a, uint16_t b, uint16_t c, int16_t m[9]) const {
    // g50_0000.
    int s0, c0, s1, c1, s2, c2;
    sinCos(a, &s0, &c0);
    sinCos(b, &s1, &c1);
    sinCos(c, &s2, &c2);
    auto mul = [](int x, int y) { return static_cast<int>(q15(x * y)); };
    const int s0s1 = mul(s1, s0), s0c1 = mul(s0, c1);
    m[0] = static_cast<int16_t>(mul(s0s1, s2) + mul(c1, c2));
    m[1] = static_cast<int16_t>(mul(s2, c0));
    m[2] = static_cast<int16_t>(mul(s0c1, s2) - mul(c2, s1));
    m[3] = static_cast<int16_t>(mul(s0s1, c2) - mul(c1, s2));
    m[4] = static_cast<int16_t>(mul(c0, c2));
    m[5] = static_cast<int16_t>(mul(s0c1, c2) + mul(s2, s1));
    m[6] = static_cast<int16_t>(mul(s1, c0));
    m[7] = static_cast<int16_t>(-s0);
    m[8] = static_cast<int16_t>(mul(c1, c0));
}

void Mystery::transform(std::vector<Point3>& pts, const int16_t m[9]) {
    // f48_0000: each point times the matrix (rows 0-2 give x, y, z).
    for (Point3& p : pts) {
        const int16_t x = q15(int64_t{p.x} * m[0] + int64_t{p.y} * m[3] + int64_t{p.z} * m[6]);
        const int16_t y = q15(int64_t{p.x} * m[1] + int64_t{p.y} * m[4] + int64_t{p.z} * m[7]);
        const int16_t z = q15(int64_t{p.x} * m[2] + int64_t{p.y} * m[5] + int64_t{p.z} * m[8]);
        p = {x, y, z};
    }
}

void Mystery::translate(std::vector<Point3>& pts, int dx, int dy, int dz) {
    // g49_0000.
    for (Point3& p : pts) {
        p.x = static_cast<int16_t>(p.x + dx);
        p.y = static_cast<int16_t>(p.y + dy);
        p.z = static_cast<int16_t>(p.z + dz);
    }
}

void Mystery::setView(int x0, int y0, int x1, int y1) {
    // f47_0000 / f47_0032: the clipping rectangle, its centre and the
    // eye's distance (half the larger side).
    view3_.left = x0, view3_.top = y0, view3_.right = x1, view3_.bottom = y1;
    const int hw = (x1 - x0) >> 1, hh = (y1 - y0) >> 1;
    view3_.cx = x0 + hw;
    view3_.cy = y0 + hh;
    view3_.d = std::max(hw, hh);
}

std::vector<std::pair<int, int>> Mystery::project(const std::vector<Point3>& pts) const {
    // f47_04de: round the polygon from its last corner, cutting where an
    // edge crosses y = d and projecting the corners beyond it.
    std::vector<std::pair<int, int>> flat;
    const int n = static_cast<int>(pts.size()), d = view3_.d;
    if (n == 0) return flat;
    const Point3* prev = &pts[n - 1];
    for (int i = 0; i < n; ++i) {
        const Point3& cur = pts[i];
        const bool prevNear = prev->y < d, curNear = cur.y < d;
        if (prevNear != curNear) {
            const int span = cur.y - prev->y, part = d - prev->y;
            flat.emplace_back(part * (cur.x - prev->x) / span + prev->x + view3_.cx,
                              -(part * (cur.z - prev->z) / span + prev->z) + view3_.cy);
        }
        if (!curNear) flat.emplace_back(cur.x * d / cur.y + view3_.cx, -(cur.z * d / cur.y) + view3_.cy);
        prev = &cur;
    }
    // f47_0068: then cut to the rectangle, an edge at a time.
    auto clip = [&](const std::vector<std::pair<int, int>>& in, auto inside, auto cross) {
        std::vector<std::pair<int, int>> out;
        for (size_t k = 0; k < in.size(); ++k) {
            const auto& a = in[(k + in.size() - 1) % in.size()];
            const auto& b = in[k];
            if (inside(b)) {
                if (!inside(a)) out.push_back(cross(a, b));
                out.push_back(b);
            } else if (inside(a)) {
                out.push_back(cross(a, b));
            }
        }
        return out;
    };
    auto atX = [](int x) {
        return [x](const std::pair<int, int>& a, const std::pair<int, int>& b) {
            return std::pair<int, int>(x, a.second + (x - a.first) * (b.second - a.second) / (b.first - a.first));
        };
    };
    auto atY = [](int y) {
        return [y](const std::pair<int, int>& a, const std::pair<int, int>& b) {
            return std::pair<int, int>(a.first + (y - a.second) * (b.first - a.first) / (b.second - a.second), y);
        };
    };
    const auto& v = view3_;
    if (flat.size() > 1) flat = clip(flat, [&](auto p) { return p.first >= v.left; }, atX(v.left));
    if (flat.size() > 1) flat = clip(flat, [&](auto p) { return p.first <= v.right; }, atX(v.right));
    if (flat.size() > 1) flat = clip(flat, [&](auto p) { return p.second >= v.top; }, atY(v.top));
    if (flat.size() > 1) flat = clip(flat, [&](auto p) { return p.second <= v.bottom; }, atY(v.bottom));
    return flat;
}

}  // namespace edison
