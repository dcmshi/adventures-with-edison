// WMAIN.EXE: the arcade's table. A room is its own camera (segment 25) and
// the root of a tree of boxes (segment 12) read from S<n>.SRF (segment 27);
// the boxes' faces (segment 34) are the ball's height map. See the table's
// notes in docs/SCIENCE.md.

#include <algorithm>
#include <cctype>
#include <fstream>
#include <functional>
#include <iterator>

#include "science/science.h"

namespace edison {

namespace {

constexpr uint16_t kBackFront = 0x1005;  // the textures (f12_2a09)
constexpr uint16_t kSides = 0x1006;
constexpr uint16_t kTop = 0x100D;

}  // namespace

Science::Rect Science::intersect(const Rect& a, const Rect& b) {
    // f11_0c12
    const int x0 = std::max(a.x, b.x), y0 = std::max(a.y, b.y);
    const int x1 = std::min(a.x + a.w, b.x + b.w), y1 = std::min(a.y + a.h, b.y + b.h);
    return {x0, y0, std::max(0, x1 - x0), std::max(0, y1 - y0)};
}

bool Science::inside(const Rect& r, int x, int y) {
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

namespace {

// The shape's text: numbers, and bytes 1 (a child follows) and 2 (no more).
class ShapeReader {
public:
    explicit ShapeReader(std::vector<char> text) : text_(std::move(text)) {}
    void skip() {
        while (at_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[at_]))) ++at_;
    }
    int number() {
        skip();
        bool negative = false;
        if (at_ < text_.size() && text_[at_] == '-') negative = true, ++at_;
        int n = 0;
        while (at_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[at_]))) n = n * 10 + (text_[at_++] - '0');
        return negative ? -n : n;
    }
    char marker() {
        skip();
        return at_ < text_.size() ? text_[at_++] : '\2';
    }

private:
    std::vector<char> text_;
    size_t at_ = 0;
};

}  // namespace

bool Science::loadTable(int room) {
    // f41_0000 and the like: room(parent, [1FEE] = 809, [1FF0] = 789, the
    // play area), the gravity from [1FF2] first; f25_03bf the camera (45
    // degrees, f25_07cb: segment 86's table), f25_0779 the scroll.
    auto word = [&](size_t at) { return static_cast<int16_t>(data_[at] | data_[at + 1] << 8); };
    Table& t = table_;
    t = Table{};
    t.view = {54, 6, 530, 280};
    t.bottom = t.view.y + t.view.h - 1;
    t.scrollX = -word(0x1FF2);
    t.scrollY = 0;
    t.sin = 23170;  // seg86:0800 / 2
    t.cos = 23152;  // seg86:07FE / 2
    t.root.bottom = t.root.top = {0, 0, word(0x1FEE), word(0x1FF0)};

    // f27_0ad8, f27_0d4a: the tree. Each box is read relative to its
    // parent and made by the room's method 1 (f27_12b6 → f25_057b), which
    // puts the parent's corner and height back: so the file's numbers are
    // the world's. The root's own numbers are read and left (it's the room).
    std::ifstream in(options_.cdDir + "/S" + std::to_string(room) + ".SRF", std::ios::binary);
    if (!in) return false;
    ShapeReader r(std::vector<char>(std::istreambuf_iterator<char>(in), {}));
    std::function<void(Box*)> read = [&](Box* parent) {
        // The top first, then the bottom (f27_0d4a passes the second read
        // as the bottom).
        Rect second, first;
        second.x = r.number(), second.y = r.number(), second.w = r.number(), second.h = r.number();
        first.x = r.number(), first.y = r.number(), first.w = r.number(), first.h = r.number();
        const int height = r.number();
        Box* box = &t.root;
        if (parent) {
            // f12_3e0d: the bottom within the parent's top; none if that's
            // empty. Then f12_04a1: the top within the bottom, but only if
            // its corner is inside the bottom (else the top is the bottom).
            const Rect bottom = intersect(first, parent->top);
            if (bottom.w == 0 || bottom.h == 0) {
                box = nullptr;
            } else {
                auto child = std::make_unique<Box>();
                child->parent = parent;
                child->bottom = child->top = bottom;
                child->height = height;
                if (inside(bottom, second.x, second.y)) {
                    Rect top = second;
                    top.w = std::max(top.w, 1), top.h = std::max(top.h, 1);
                    top = intersect(top, bottom);
                    if (second.w == 0) top.w = 0;
                    if (second.h == 0) top.h = 0;
                    child->top = top;
                }
                child->stepX = child->top.w < parent->stepX ? 0 : parent->stepX;
                child->stepY = child->top.h < parent->stepY ? 0 : parent->stepY;
                hiddenSides(*child);
                box = child.get();
                parent->children.push_back(std::move(child));
            }
        }
        for (char m = r.marker(); m != '\2'; m = r.marker()) {
            if (m != '\1') break;
            read(box ? box : parent);
        }
    };
    read(nullptr);
    return true;
}

std::pair<int, int> Science::project(int x, int y, int h) const {
    // f25_0813: oblique, depth at 5/10 ([1F56] / [1F58]) of the angle's
    // cosine across and its sine up.
    const Table& t = table_;
    const int across = static_cast<int>(static_cast<int64_t>(y) * t.cos * 5 / (10LL * 0x7FFF));
    const int up = static_cast<int>(static_cast<int64_t>(y) * t.sin * 5 / (10LL * 0x7FFF));
    return {across + x + t.view.x + t.scrollX, t.bottom - (up + h) + t.scrollY};
}

void Science::hiddenSides(Box& box) const {
    // f12_093b (only angles under 0x4000: "Left perspective case not
    // implemented"), from three projected corners.
    const Rect& T = box.top;
    const Rect& B = box.bottom;
    const int P = box.parentHeight();
    if (box.height < P) {
        // A pit: its right and front.
        box.hideLeft = box.hideBack = false;
        const auto t = project(T.x + T.w - 1, T.y, box.height);
        const auto u = project(B.x + B.w - 1, B.y, P);
        const auto v = project(B.x + B.w - 1, B.y + B.h - 1, P);
        const int dx = v.first - t.first;
        box.hideRight = dx == 0 || static_cast<int64_t>(t.second - v.second) * (v.first - u.first) >
                                       static_cast<int64_t>(dx) * (u.second - v.second);
        box.hideFront = u.second < t.second;
    } else {
        // Standing up: its left and back.
        box.hideRight = box.hideFront = false;
        const auto t = project(T.x, T.y + T.h - 1, box.height);
        const auto u = project(B.x, B.y + B.h - 1, P);
        const auto v = project(B.x, B.y, P);
        const int dx = t.first - v.first;
        box.hideLeft = dx == 0 || static_cast<int64_t>(v.second - t.second) * (u.first - v.first) >
                                      static_cast<int64_t>(dx) * (v.second - u.second);
        box.hideBack = t.second < u.second;
    }
}

int Science::faceAt(const Box& box, int x, int y) const {
    // f34_02fa: the top, else which slope of the bottom's border.
    const Rect& T = box.top;
    const Rect& B = box.bottom;
    if (inside(T, x, y)) return 1;
    if (!inside(B, x, y)) return 0;
    if (x >= T.x && x < T.x + T.w) return B.y == T.y || T.y < y ? 4 : 5;
    if (y >= T.y && y < T.y + T.h) return B.x == T.x || T.x < x ? 3 : 2;
    // The corners: which of the two slopes, by the diagonal.
    int64_t dx = x - T.x;
    const int64_t dy = y - T.y;
    if (dx < 1) {
        if (dy < 1) return dy * (B.x - T.x) <= dx * (B.y - T.y) ? 2 : 5;
        return (dy - T.h) * (B.x - T.x) < dx * (B.y + B.h - T.y - T.h) ? 4 : 2;
    }
    dx -= T.w;
    if (dy < 1) return dy * (B.x + B.w - T.x - T.w) < dx * (B.y - T.y) ? 5 : 3;
    return (dy - T.h) * (B.x + B.w - T.x - T.w) <= dx * (B.y + B.h - T.y - T.h) ? 3 : 4;
}

int Science::heightAt(const Box& box, int x, int y) const {
    // f12_44f9: the face under the point (f12_443b), its height there
    // (f34_07ec, set up by f34_016a): the top's, or across a slope from the
    // top's edge (its own height) to the bottom's (the parent's).
    const int face = faceAt(box, x, y);
    if (face == 0) return 0;
    if (face == 1) return box.height;
    const Rect& T = box.top;
    const Rect& B = box.bottom;
    const int low = box.parentHeight(), high = box.height;
    int edge = 0, foot = 0, v = x;  // +9, +7, along x (or y: +B)
    switch (face) {
        case 2: edge = T.x, foot = B.x; break;
        case 3: edge = T.x + T.w, foot = B.x + B.w; break;
        case 4: edge = T.y + T.h, foot = B.y + B.h, v = y; break;
        default: edge = T.y, foot = B.y, v = y; break;
    }
    if (foot == edge) return low;
    return static_cast<int>((static_cast<int64_t>(v - edge) * low + static_cast<int64_t>(foot - v) * high) / (foot - edge));
}

void Science::faceFill(std::vector<std::pair<int, int>> points, int look, bool texture) {
    // f12_0ddf: clipped to the view (f83_0065), then a bitmap fill
    // stretched onto it (f63_20e4) or one colour (f14_07a0).
    if (texture) fillPolygonStretched(points, static_cast<uint16_t>(look));
    else fillPolygon(points, static_cast<uint8_t>(look));
}

void Science::tableLine(int x0, int y0, int x1, int y1, uint8_t colour) {
    // f14_15f1: each end clamped into the clip (f11_0aea), then the line.
    const Rect& v = table_.view;
    auto clampX = [&](int x) { return std::clamp(x, v.x, v.x + v.w - 1); };
    auto clampY = [&](int y) { return std::clamp(y, v.y, v.y + v.h - 1); };
    line(clampX(x0), clampY(y0), clampX(x1), clampY(y1), colour);
}

bool Science::gridOn(int face) const {
    // A face gets the grid if it has the texture and the camera sees it
    // (f12_2a09 keeps which at [11E8]-[11F0]); points on no face go on.
    return face < 1 || face > 5 || gridFaces_[face];
}

void Science::gridX(const Box& box, int face, int x, int y, int end) {
    // f12_335e: a line from (x, y) to (end, y) if `end` is still on the
    // face, else the halves (the second's face where it starts).
    if (end <= x) return;
    const int f = faceAt(box, end, y);
    if (f == face && f != 0) {
        if (!gridOn(f)) return;
        const auto a = project(x, y, heightAt(box, x, y));
        const auto b = project(end, y, heightAt(box, end, y));
        tableLine(a.first - 1, a.second, b.first - 1, b.second, 0x84);
        tableLine(a.first, a.second, b.first, b.second, 0x81);
        return;
    }
    const int mid = (x + end) / 2;
    gridX(box, face, x, y, mid);
    if (mid == end) return;
    gridX(box, faceAt(box, mid + 1, y), mid + 1, y, end);
}

void Science::gridY(const Box& box, int face, int x, int y, int end) {
    // f12_35c5: the same along y, the shadow a row up.
    if (end <= y) return;
    const int f = faceAt(box, x, end);
    if (f == face && f != 0) {
        if (!gridOn(f)) return;
        const auto a = project(x, y, heightAt(box, x, y));
        const auto b = project(x, end, heightAt(box, x, end));
        tableLine(a.first, a.second - 1, b.first, b.second - 1, 0x84);
        tableLine(a.first, a.second, b.first, b.second, 0x81);
        return;
    }
    const int mid = (y + end) / 2;
    gridY(box, face, x, y, mid);
    if (mid == end) return;
    gridY(box, faceAt(box, x, mid + 1), x, mid + 1, end);
}

void Science::drawBox(const Box& box) {
    // f12_2a09: the faces back to front, then the grid.
    const Rect& v = table_.view;
    setPolygonClip(v.x, v.y, v.w, v.h);  // [11B6], f14_08cc / f14_126e
    std::fill(std::begin(gridFaces_), std::end(gridFaces_), false);
    const Rect& T = box.top;
    const Rect& B = box.bottom;
    auto top = [&] {
        // f12_2871: the top, the far corners a pixel to the right.
        auto a = project(T.x, T.y, box.height), b = project(T.x, T.y + T.h - 1, box.height);
        auto c = project(T.x + T.w - 1, T.y, box.height), d = project(T.x + T.w - 1, T.y + T.h - 1, box.height);
        ++b.first, ++d.first;
        faceFill({b, a, c, d}, kTop, true);
        gridFaces_[1] = true;
    };
    if (!box.parent) {
        top();
    } else {
        const int P = box.parentHeight();
        const bool pit = box.height < P;
        const Box& parent = *box.parent;
        const auto A = project(T.x, T.y, box.height), Bk = project(T.x, T.y + T.h - 1, box.height);
        const auto C = project(T.x + T.w - 1, T.y, box.height), D = project(T.x + T.w - 1, T.y + T.h - 1, box.height);
        const auto a = project(B.x, B.y, P), b = project(B.x, B.y + B.h - 1, P);
        const auto c = project(B.x + B.w - 1, B.y, P), d = project(B.x + B.w - 1, B.y + B.h - 1, P);
        // (A face's own look, +3C, isn't read yet: the textures.)
        faceFill({Bk, D, d, b}, kBackFront, true);  // 4, the back
        gridFaces_[4] = !box.hideBack;
        faceFill({Bk, A, a, b}, kSides, true);      // 2, the left
        gridFaces_[2] = !box.hideLeft;
        // 3, the right, and 5, the front: a pit's only where they aren't
        // its parent's edge.
        if (!pit || B.x + B.w != parent.bottom.x + parent.bottom.w) {
            faceFill({D, C, c, d}, kSides, true);
            gridFaces_[3] = !box.hideRight;
        }
        if (!pit || B.y != parent.bottom.y) {
            faceFill({A, C, c, a}, kBackFront, true);
            gridFaces_[5] = !box.hideFront;
        }
        top();
    }
    if (box.stepX && box.stepY) {  // and not [11F2]
        const int xEnd = B.x + B.w, yEnd = B.y + B.h;
        for (int y = B.y / box.stepY * box.stepY; y < yEnd; y += box.stepY)
            for (int x = B.x / box.stepX * box.stepX; x < xEnd; x += box.stepX) {
                const int face = faceAt(box, x, y);
                if (!gridOn(face)) continue;
                gridX(box, face, x, y, x + box.stepX);
                gridY(box, face, x, y, y + box.stepY);
            }
    }
}

void Science::drawStanding(const Box& box) {
    // f12_38ad: what stands up drawn, and where a pit is (its mouth, f12_10cd
    // case 6, and a side on the parent's edge) cleared to colour 0, which
    // lets the pits drawn on screen 2 through.
    if (box.height < box.parentHeight()) {
        const Rect& T = box.top;
        const Rect& B = box.bottom;
        const int P = box.parentHeight();
        const auto a = project(B.x, B.y, P), b = project(B.x, B.y + B.h - 1, P);
        const auto c = project(B.x + B.w - 1, B.y, P), d = project(B.x + B.w - 1, B.y + B.h - 1, P);
        const auto A = project(T.x, T.y, box.height), C = project(T.x + T.w - 1, T.y, box.height);
        const auto D = project(T.x + T.w - 1, T.y + T.h - 1, box.height);
        const Rect& v = table_.view;
        setPolygonClip(v.x, v.y, v.w, v.h);
        // f12_3835 → f12_0f0b: only a polygon with some width and height.
        auto cut = [&](std::vector<std::pair<int, int>> pts) {
            bool wide = false, tall = false;
            for (auto& p : pts) wide |= p.first != pts[0].first, tall |= p.second != pts[0].second;
            if (wide && tall) fillPolygon(pts, 0);
        };
        cut({b, d, c, a});
        if (B.y == box.parent->bottom.y) cut({A, C, c, a});
        if (B.x + B.w == box.parent->bottom.x + box.parent->bottom.w) cut({D, C, c, d});
    } else {
        drawBox(box);
    }
    for (const auto& child : box.children) drawStanding(*child);
}

void Science::drawPits(const Box& box) {
    // f12_39b5
    if (box.height < box.parentHeight()) drawBox(box);
    for (const auto& child : box.children) drawPits(*child);
}

void Science::drawTable() {
    // f27_0ec5, the room's method 0: on screen 3 the view's border in
    // colour 0 and what stands up, on screen 2 the pits, then 3 over 2
    // (colour 0 showing 2).
    const Rect& v = table_.view;
    select(3);
    tableLine(v.x, v.y + v.h - 1, v.x + v.w - 1, v.y + v.h - 1, 0);
    tableLine(v.x, v.y, v.x, v.y + v.h - 1, 0);
    tableLine(v.x + v.w - 1, v.y, v.x + v.w - 1, v.y + v.h - 1, 0);
    tableLine(v.x, v.y, v.x + v.w - 1, v.y, 0);
    drawStanding(table_.root);
    select(2);
    drawPits(table_.root);
    copyKeyed(3, 2, v.x, v.y, v.w, v.h);
    clearPolygonClip();
    // f27_0e5b (from the room's method 4, its last argument 0): the
    // machine (2002, with the look) on screen 3 and the table into its
    // window (+60); not yet to the display.
    select(3);
    showScreenWithLook(0x2002, 3);
    copyArea(2, 3, v.x, v.y, v.w, v.h);
}

void Science::roomPictures(int room) {
    // The room's method 4 after f27_0e5b: its own pictures on screen 3, at
    // fixed places, each only if it fits (f14_1179: show_Clogo).
    struct Picture {
        int x, y;
        uint16_t id;
    };
    static const Picture kRoom1[] = {
        // f41_0126: the holes' labels (HIGH Score, Lab, Credits, play room,
        // LEVEL 1, 4 and 5), the logo, EXIT.
        {58, 168, 0x1399}, {104, 139, 0x139A}, {144, 98, 0x139B}, {236, 80, 0x139C}, {320, 76, 0x139D},
        {408, 76, 0x139E}, {504, 29, 0x139F}, {54, 6, 0x13D0}, {478, 212, 0x13D2}};
    select(3);
    if (room == 1)
        for (const Picture& p : kRoom1) drawLogo(p.x, p.y, p.id);
}

void Science::redrawTable(const Rect& area) {
    // f27_1e36, the room's method 3: within the view, screen 2 cleared to
    // colour 0, the room's objects painted back to front (not yet), the
    // score and shots boxes, then screen 3 where screen 2 is still colour 0
    // (f14_0c88 → f65_0294), and the area to the display.
    const Rect a = intersect(area, table_.view);
    if (a.w == 0 || a.h == 0) return;
    Screen& two = ctx_.screens[2];
    for (int y = a.y; y < a.y + a.h; ++y)
        std::fill_n(two.pixels.begin() + static_cast<size_t>(y) * Screen::kWidth + a.x, a.w, uint8_t{0});
    select(2);
    // The score (+F35, at +F25) and " shots: n" (+F39, at +F2D, DS:2040):
    // the yellow box 1425 and the number in colour 10 (f76_0021).
    const Bitmap& box = ctx_.bitmap(0x1425);
    auto overlaps = [&](int x, int y) { return intersect(a, {x, y, box.width, box.height}).w > 0 && intersect(a, {x, y, box.width, box.height}).h > 0; };
    if (overlaps(480, 8)) {
        drawLogo(480, 8, 0x1425);
        textAt(480 + 0x17, 8 + 3, std::to_string(score_), 0x10);
    }
    if (overlaps(58, 8)) {
        drawLogo(58, 8, 0x1425);
        textAt(58 + 3, 8 + 3, dataString(0x2040) + std::to_string(shots_), 0x10);
    }
    const Screen& three = ctx_.screens[3];
    for (int y = a.y; y < a.y + a.h; ++y)
        for (int x = a.x; x < a.x + a.w; ++x) {
            const size_t at = static_cast<size_t>(y) * Screen::kWidth + x;
            if (two.pixels[at] == 0) two.pixels[at] = three.pixels[at];
        }
    copyArea(2, 1, a.x, a.y, a.w, a.h);
}

void Science::enterRoom(int room) {
    // f31_0783 for rooms 1-100: screen 2 filled with colour 2, the room
    // built (its shape, objects and panel, then its method 4: the table and
    // its pictures on screen 3), screen 3 to the display, then event 5: the
    // redraw of the whole (the player's method 4, f31_27de).
    std::fill(ctx_.screens[2].pixels.begin(), ctx_.screens[2].pixels.end(), uint8_t{2});
    if (!loadTable(room)) {
        logLine("Wild Science Arcade: no S" + std::to_string(room) + ".SRF");
        return;
    }
    score_ = 0, shots_ = 0;
    drawTable();
    roomPictures(room);
    toDisplay(3);
    redrawTable({0, 0, Screen::kWidth, Screen::kHeight});
}

void Science::showTable(int room) {
    // For testing (--room 1): the room as the original first shows it
    // (not yet its objects or panel), till a click.
    loadLook();
    looksConverted_ = true;
    enterRoom(room);
    select(1);
    int x, y;
    for (;;) {
        ctx_.pump();
        if (ctx_.platform.takeClick(&x, &y)) return;
    }
}

}  // namespace edison
