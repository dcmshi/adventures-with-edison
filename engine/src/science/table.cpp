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
    std::string word() {
        skip();
        std::string w;
        while (at_ < text_.size() && !std::isspace(static_cast<unsigned char>(text_[at_]))) w += text_[at_++];
        return w;
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

    // The objects (f61_011d): OBJn x y type a b c d e f, then PANEL.
    t.objects.clear();
    std::string w = r.word();
    for (; w.rfind("OBJ", 0) == 0; w = r.word()) {
        Object o;
        o.x = r.number(), o.y = r.number(), o.type = r.number();
        for (int& v : o.args) v = r.number();
        t.objects.push_back(o);
    }
    // PANEL: (flags, value) for gravity, friction, power and the ball type
    // (f61_0f76, read in that order); none: all 0.
    panel_ = PanelState{};
    if (w == "PANEL") {
        panel_.gravityFlags = r.number(), panel_.gravity = r.number();
        panel_.frictionFlags = r.number(), panel_.friction = r.number();
        panel_.powerFlags = r.number(), panel_.power = r.number();
        panel_.ballTypeFlags = r.number(), panel_.ballType = r.number();
    } else {
        panel_ = {0, 0, 0, 0};
    }
    // f30_3339 keeps each within its slider's range; the ball type is taken
    // mod 6 (f30_29cc).
    panel_.gravity = std::clamp(panel_.gravity, -16, 4);
    panel_.friction = std::clamp(panel_.friction, 0, 16);
    panel_.power = std::clamp(panel_.power, 0, 16);
    panel_.ballType = ((panel_.ballType % 6) + 6) % 6;
    return true;
}

int Science::heightUnder(int x, int y) const {
    // f27_0903 then f34_07ec: the height of the deepest box's face under
    // the point.
    const Box* box = &table_.root;
    for (bool deeper = true; deeper;) {
        deeper = false;
        for (const auto& child : box->children)
            if (inside(child->bottom, x, y)) {
                box = child.get(), deeper = true;
                break;
            }
    }
    return heightAt(*box, x, y);
}

std::pair<int, int> Science::objectCentre(int x, int y, int z, int w, int d, int h) const {
    // f25_0a51 / f27_16ae: the box's near bottom and far top corners
    // projected give the object's rectangle; sprites go at its centre.
    const auto a = project(x, y, z);
    const auto b = project(x + w - 1, y + d - 1, z + h - 1);
    const int rw = b.first - a.first + 2, rh = a.second - b.second + 2;
    return {a.first + (rw >> 1), b.second + (rh >> 1)};
}

void Science::objectSprite(int cx, int cy, uint16_t id) {
    // f14_0d69 at 1:1: centred, colour 0 left out, and only when it lies
    // wholly inside the clip ([1706]: the whole screen, as for the lines;
    // only the view's part reaches the display, so a ball in a pit at the
    // view's foot shows cut off by its edge).
    const Bitmap& bmp = ctx_.bitmap(id);
    const int x = cx - (bmp.width >> 1), y = cy - (bmp.height >> 1);
    const Rect v{0, 0, Screen::kWidth, Screen::kHeight};
    if (!inside(v, x, y) || !inside(v, x + bmp.width, y + bmp.height)) return;
    ctx_.screens.drawSprite(current(), bmp, x, y);
}

namespace {

constexpr int kHoleTicks = 22;  // [212C]

// A drawable of the room's list (+1AD, 0x17 bytes each): its box (x, y, z,
// w, d, h; at its object's +6E), its rectangle on the screen, its kind (the
// object's +2A: 3 a hole, whose +24 is its wall) and how it's drawn.
struct Drawable {
    struct Area {
        int x, y, w, h;
    };
    int box[6];
    Area rect;
    int kind = 0, wall = 0;
    std::function<void()> draw;
};

bool spans(int a, int aw, int c, int cw) {
    // f25_0000 / 00a9 / 0157: the ranges [a, a + aw - 1] and [c, ...] meet.
    if (aw == 0 || cw == 0) return false;
    const int ce = c + cw - 1, ae = a + aw - 1;
    return (c <= a && a <= ce) || (c <= ae && ae <= ce) || (a <= c && ce <= ae);
}

// f35_0744 for two objects (not table boxes): 0 when a is in front of c
// (drawn after it), 2 when it's behind, 1 when they don't overlap.
int depthOrder(const Drawable& a, const Drawable& c) {
    const Drawable::Area& A = a.rect;
    const Drawable::Area& B = c.rect;
    // f11_0a26: the rectangles meet.
    if (B.x + B.w - 1 < A.x || A.x + A.w - 1 < B.x || !(A.y <= B.y + B.h - 1 && B.y <= A.y + A.h - 1)) return 1;
    // Apart along the projection's slant.
    const int aBottom = A.y + A.h - 1, bBottom = B.y + B.h - 1;
    if ((aBottom - a.box[5]) - bBottom > (B.x + c.box[3]) - A.x) return 1;
    if ((bBottom - c.box[5]) - aBottom > (A.x + a.box[3]) - B.x) return 1;
    // Apart along an axis: the nearer (smaller y), the further right, the
    // higher in front.
    if (!spans(a.box[1], a.box[4], c.box[1], c.box[4])) return a.box[1] <= c.box[1] ? 0 : 2;
    if (!spans(a.box[0], a.box[3], c.box[0], c.box[3])) return c.box[0] <= a.box[0] ? 0 : 2;
    if (!spans(a.box[2], a.box[5], c.box[2], c.box[5])) return c.box[2] <= a.box[2] ? 0 : 2;
    // A hole: by the centres across its wall.
    if (a.kind == 3) {
        if (a.wall == 0) return (a.box[3] >> 1) + a.box[0] <= (c.box[3] >> 1) + c.box[0] ? 2 : 0;
        return (c.box[4] >> 1) + c.box[1] <= (a.box[4] >> 1) + a.box[1] ? 2 : 0;
    }
    if (c.kind == 3) {
        if (c.wall == 0) return (c.box[3] >> 1) + c.box[0] <= (a.box[3] >> 1) + a.box[0] ? 0 : 2;
        return (a.box[4] >> 1) + a.box[1] <= (c.box[4] >> 1) + c.box[1] ? 0 : 2;
    }
    // f11_0edd: their common box; along its thinnest side (z first on a
    // tie, then y).
    const int w = std::min(a.box[0] + a.box[3], c.box[0] + c.box[3]) - std::max(a.box[0], c.box[0]);
    const int d = std::min(a.box[1] + a.box[4], c.box[1] + c.box[4]) - std::max(a.box[1], c.box[1]);
    const int h = std::min(a.box[2] + a.box[5], c.box[2] + c.box[5]) - std::max(a.box[2], c.box[2]);
    if (d < w ? h <= d : h <= w) return c.box[2] <= a.box[2] ? 0 : 2;
    if (d < w) return a.box[1] <= c.box[1] ? 0 : 2;
    return c.box[0] <= a.box[0] ? 0 : 2;
}

}  // namespace

Science::Rect Science::objectRect(int x, int y, int z, int w, int d, int h) const {
    // f25_0a51 / f27_16ae: the near bottom and far top corners projected.
    const auto a = project(x, y, z);
    const auto b = project(x + w - 1, y + d - 1, z + h - 1);
    return {a.first, b.second, b.first - a.first + 2, a.second - b.second + 2};
}

void Science::drawObjects() {
    // f27_1e36: the room's drawables (+1AD) in the painter's order: each
    // pair compared (f27_1af3 → f27_19d9 → f35_0744) into "drawn after"
    // (+5FD, 48 x 48); then, in the list's order, each with nothing in
    // front of it is drawn after (f27_1d2f) all those behind it, then any
    // left. The table's own boxes (type 1) aren't in it here: the port
    // draws the table under all the objects (enough while no standing box
    // hides one).
    auto area = [](const Rect& r) { return Drawable::Area{r.x, r.y, r.w, r.h}; };
    std::vector<Drawable> list;
    for (const Object& o : table_.objects) {
        if (o.type == 8) {
            // A hole (f28_00f3, g28_0003, drawn by f28_0d82): a = the room
            // it leads to, b = its wall (0 the left one), c = big, d = its
            // height (-1: the face's under it). Frame 0 at rest. Its box is
            // a cube of side 2r, r to the left on the left wall.
            const int wall = o.args[1], big = o.args[2] != 0;
            const int r = big ? 17 : 11;
            const int z = o.args[3] != -1 ? o.args[3] : heightUnder(o.x, o.y);
            const int x = o.x - (wall == 0 ? r : 0);
            Drawable dr{{x, o.y, z, 2 * r, 2 * r, 2 * r}, area(objectRect(x, o.y, z, 2 * r, 2 * r, 2 * r)), 3, wall, {}};
            // Its frame (f28_0d82): 0 at rest; swallowing (+21) 1-5 as it
            // counts, spitting (+23) 5-1. The sprites by size and wall
            // (DS:20FC small, DS:2114 big; 6 a wall); frames 1-4 show the
            // ball, by its type (f28_0cd5: 29 sprites a type).
            int frame = 0;
            if (o.swallow) frame = std::min(static_cast<int>(static_cast<unsigned>(o.swallow) * 5 / kHoleTicks) + 1, 5);
            else if (o.spit) frame = std::clamp(6 - (static_cast<int>(static_cast<unsigned>(o.spit) * 5 / kHoleTicks) + 1), 1, 5);
            static const int kTypeOffset[6] = {4, 5, 0, 1, 2, 3};
            const int typeOffset = kTypeOffset[std::clamp(panel_.ballType, 0, 5)] * 0x1D;
            dr.draw = [this, x, y = o.y, z, r, wall, big, frame, typeOffset] {
                auto [cx, cy] = objectCentre(x, y, z, 2 * r, 2 * r, 2 * r);
                const size_t at = (big ? 0x2114u : 0x20FCu) + 2u * static_cast<size_t>((wall ? 6 : 0) + frame);
                uint16_t id = static_cast<uint16_t>(data_[at] | data_[at + 1] << 8);
                if (frame > 0 && frame < 5) id = static_cast<uint16_t>(id + typeOffset);
                if (!big) cy -= wall == 0 ? 3 : 0;
                else cy += wall == 0 ? 0 : 3;
                objectSprite(cx, cy, id);
            };
            list.push_back(dr);
        } else if (o.type == 10 && o.args[2] != 6) {
            // A point target (drawn by f13_0bb1): its box a 26 cube on the
            // ground at its corner (8 bigger each way once hit); idle its
            // kind's frames (DS:15CE + 24 a kind), hit its sequence, scored
            // its points (1236 + hundreds - 1).
            const int g = heightUnder(o.x, o.y), grow = o.hit ? 8 : 0;
            const int bx = o.x - grow, by = o.y - grow, bz = g - grow, side = 26 + 2 * grow;
            Drawable dr{{bx, by, bz, side, side, side}, area(objectRect(bx, by, bz, side, side, side)), 0, 0, {}};
            dr.draw = [this, &o, bx, by, bz, side] {
                if (!o.shown) return;
                const auto [cx, cy] = objectCentre(bx, by, bz, side, side, side);
                const int frame = std::max(o.frame, 0);
                size_t at;
                uint16_t id;
                if (o.scored) {
                    id = static_cast<uint16_t>(0x1236 + std::clamp(o.points / 100 - 1, 0, 9));
                } else {
                    at = o.sequence ? static_cast<size_t>(o.sequence) + 2u * frame : 0x15CEu + 0x18u * o.kind + 2u * frame;
                    id = static_cast<uint16_t>(data_[at] | data_[at + 1] << 8);
                }
                objectSprite(cx, cy, id);
            };
            list.push_back(dr);
        } else if ((o.type == 1 || o.type == 3) && hasBall_) {
            // The ball (f06_0043, radius 10; kind 1), then its shadow
            // object and its target.
            const Ball& b = ball_;
            const int s = 2 * b.r + 1;
            Drawable ball{{b.cx - b.r, b.cy - b.r, b.cz - b.r, s, s, s}, area(objectRect(b.cx - b.r, b.cy - b.r, b.cz - b.r, s, s, s)), 1, 0, {}};
            ball.draw = [this] {
                const Ball& b = ball_;
                if (b.hidden) return;
                if (b.state != 0) {
                    // Breaking (f13_01ce, +7C): its type's frames (+22: DS:13E4
                    // Ice, 13BA Stone, 1390 Rubber, 1438 Iron, 1462 Glass, 140E
                    // Magic; 6 bytes each, the sprite first), no shadow.
                    static const uint16_t kBreak[6] = {0x13E4, 0x13BA, 0x1390, 0x1438, 0x1462, 0x140E};
                    const size_t at = kBreak[std::clamp(b.kind, 0, 5)] + 6u * static_cast<size_t>(b.drawFrame >> 1);
                    const auto [bx, by] = objectCentre(b.cx - b.r, b.cy - b.r, b.cz - b.r, 2 * b.r + 1, 2 * b.r + 1, 2 * b.r + 1);
                    objectSprite(bx, by, static_cast<uint16_t>(data_[at] | data_[at + 1] << 8));
                    return;
                }
                const auto [bx, by] = objectCentre(b.cx - b.r, b.cy - b.r, b.cz - b.r, 2 * b.r + 1, 2 * b.r + 1, 2 * b.r + 1);
                // f13_01ce: its shadow first (1040, the radius less one
                // below the centre: while the shadow object, its +16, is
                // hidden (its +60), and not [14E0]), then the ball in its
                // type's rolling frames (f07_04d5: DS:1348 Ice, 1330 Stone,
                // 1300 Rubber, 1378 Iron, 1318 Glass, 1360 Magic).
                if (!shadowShown_ && !noShadow_) objectSprite(bx, by + b.r - 1, 0x1040);
                static const uint16_t kFrames[6] = {0x1348, 0x1330, 0x1300, 0x1378, 0x1318, 0x1360};
                const size_t table = kFrames[std::clamp(panel_.ballType, 0, 5)] + 2u * static_cast<size_t>(b.drawFrame >> 1);
                objectSprite(bx, by, static_cast<uint16_t>(data_[table] | data_[table + 1] << 8));
            };
            list.push_back(ball);
            // The shadow object (f07_12d6; drawn by f13_04ab): 1040 at its
            // box's centre projected, (x, y, ground + 1).
            const int sx = shadowX_ - b.r, sy = shadowY_ - b.r;
            Drawable shadow{{sx, sy, shadowZ_, s, s, 2}, area(objectRect(sx, sy, shadowZ_, s, s, 2)), 0, 0, {}};
            shadow.draw = [this] {
                if (!shadowShown_) return;
                const auto [x, y] = project(shadowX_, shadowY_, shadowZ_);
                objectSprite(x, y, 0x1040);
            };
            list.push_back(shadow);
            // The target (f06_0877, kept at +F79), hidden while the ball
            // moves (f06_0aa8): the ring's back 1042 here, its front 1043
            // over everything ([1508]). Its position is its box's centre;
            // once moved, its box sits a unit above the ground (as the
            // original shows).
            const int tz = heightUnder(targetX_, targetY_) + (targetMoved_ ? 1 : 0);
            Drawable target{{targetX_ - 10, targetY_ - 10, tz, 20, 20, 10}, area(objectRect(targetX_ - 10, targetY_ - 10, tz, 20, 20, 10)), 0, 0, {}};
            target.draw = [this, tz] {
                if (ballMoving_) return;
                const auto [tx, ty] = objectCentre(targetX_ - 10, targetY_ - 10, tz, 20, 20, 10);
                objectSprite(tx + 1, ty, 0x1042);
            };
            list.push_back(target);
        }
    }
    const size_t n = list.size();
    std::vector<uint8_t> after(n * n, 0);  // after[i * n + j]: i is drawn after j
    std::vector<bool> free(n, true), drawn(n, false);
    for (size_t i = 0; i < n; ++i)
        for (size_t j = i + 1; j < n; ++j) {
            const int o = depthOrder(list[i], list[j]);
            if (o == 0) after[i * n + j] = 1, after[j * n + i] = 0, free[j] = false;
            else if (o == 2) after[j * n + i] = 1, after[i * n + j] = 0, free[i] = false;
        }
    std::function<void(size_t)> draw = [&](size_t i) {
        drawn[i] = true;
        for (size_t j = 0; j < n; ++j)
            if (j != i && after[i * n + j] && !drawn[j]) draw(j);
        list[i].draw();
    };
    for (size_t i = 0; i < n; ++i)
        if (free[i]) draw(i);
    for (size_t i = 0; i < n; ++i)
        if (!drawn[i]) draw(i);
    if (hasBall_ && !ballMoving_) {
        const int tz = heightUnder(targetX_, targetY_) + (targetMoved_ ? 1 : 0);
        const auto [tx, ty] = objectCentre(targetX_ - 10, targetY_ - 10, tz, 20, 20, 10);
        objectSprite(tx + 1, ty, 0x1043);
    }
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
    // f14_15f1: each end clamped into the clip [1706] (f11_0aea), which is
    // the whole screen here (not the view: lines running out of the view
    // land on parts the machine covers later).
    auto clampX = [&](int x) { return std::clamp(x, 0, Screen::kWidth - 1); };
    auto clampY = [&](int y) { return std::clamp(y, 0, Screen::kHeight - 1); };
    libraryLine(clampX(x0), clampY(y0), clampX(x1), clampY(y1), colour);
}

void Science::libraryLine(int x0, int y0, int x1, int y1, uint8_t colour) {
    // f63_1cd5 → f80_0024 (32-bit code: ndisasm -b 32): the line as one
    // run of pixels a row, from its top end down. The runs' ends step by
    // dx / dy in 16.16, taken at the half rows: P = 2 x + (2k + 1) step,
    // the next end ceil(int(P) / 2).
    Screen& scr = ctx_.screens[current()];
    auto run = [&](int x, int y, int len) {
        if (y < 0 || y >= Screen::kHeight) return;
        for (int i = 0; i < len; ++i)
            if (x + i >= 0 && x + i < Screen::kWidth) scr.pixels[static_cast<size_t>(y) * Screen::kWidth + x + i] = colour;
    };
    const int top = std::min(y0, y1);
    const int dx = std::abs(x0 - x1), dy = std::abs(y0 - y1);
    if (dx == 0) {  // one pixel a row
        for (int y = top; y <= top + dy; ++y) run(x1, y, 1);
        return;
    }
    if (dy == 0) {
        run(std::min(x0, x1), top, dx + 1);
        return;
    }
    const int start = y0 < y1 ? x0 : x1, end = y0 < y1 ? x1 : x0;  // the top end's x first
    const uint32_t step = (static_cast<uint32_t>(dx / dy) << 16) | ((static_cast<uint32_t>(dx % dy) << 16) / static_cast<uint32_t>(dy));
    int x = start, y = top;
    if (end > start) {
        uint32_t p = (static_cast<uint32_t>(2 * start) << 16) + step;
        for (int k = 0; k < dy; ++k, ++y, p += 2 * step) {
            const int i = static_cast<int>(p >> 16), next = (i >> 1) + (i & 1);
            run(x, y, std::max(next - x, 1));
            x = next;
        }
        run(x, y, end - x + 1);
    } else {
        uint32_t p = (static_cast<uint32_t>(2 * start) << 16) - step;
        for (int k = 0; k < dy; ++k, ++y, p -= 2 * step) {
            const int i = static_cast<int>(p >> 16), next = (i >> 1) + (i & 1);
            if (next == x) run(x, y, 1);
            else run(next + 1, y, x - next);
            x = next;
        }
        run(end, y, x - end + 1);
    }
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
    // fixed places, each only if it fits (f14_1179: show_Clogo). Each
    // room's class (segments 41-60) has its own list; room 1's are the
    // holes' labels (HIGH Score, Lab, Credits, play room, LEVEL 1, 4 and 5),
    // the logo and EXIT.
    struct Picture {
        int x, y;
        uint16_t id;
    };
    struct Room {
        int room;
        std::vector<Picture> pictures;
    };
    // (Taken from each room's method 4 by tools/testing/roompics.py --cpp:
    // x, y, the picture.)
    static const Room kRooms[] = {
        {1, {{58, 168, 0x1399}, {104, 139, 0x139A}, {144, 98, 0x139B}, {236, 80, 0x139C}, {320, 76, 0x139D}, {408, 76, 0x139E}, {504, 29, 0x139F}, {54, 6, 0x13D0}, {478, 212, 0x13D2}}},  // f41_0123
        {2, {{190, 104, 0x10D3}, {497, 143, 0x1245}}},  // f41_0968
        {3, {{160, 6, 0x106F}, {360, 6, 0x1070}}},  // f41_0ea2
        {4, {{166, 6, 0x1100}, {414, 6, 0x1101}}},  // f41_1098
        {5, {{206, 6, 0x1079}, {416, 6, 0x107A}, {54, 6, 0x107B}, {412, 156, 0x107C}, {128, 174, 0x10D1}, {474, 176, 0x10D2}}},  // f41_13e6
        {6, {{54, 16, 0x10F0}}},  // f42_01c6
        {7, {{54, 206, 0x112A}, {356, 58, 0x112B}}},  // f42_03e8
        {8, {{174, 138, 0x107E}, {56, 140, 0x107F}, {370, 138, 0x1080}}},  // f42_05a1
        {9, {{264, 48, 0x10D4}}},  // f42_0968
        {10, {{260, 60, 0x10D5}}},  // f42_0ff8
        {11, {{139, 6, 0x1081}, {54, 6, 0x1082}, {332, 6, 0x1083}}},  // f43_018d
        {12, {{176, 83, 0x10D7}}},  // f43_064b
        {13, {{54, 22, 0x1136}, {288, 190, 0x1137}}},  // f43_09fa
        {14, {{98, 6, 0x1084}, {106, 230, 0x1085}, {342, 6, 0x1086}}},  // f43_0c85
        {15, {{54, 232, 0x112C}, {220, 6, 0x112D}, {404, 162, 0x112E}}},  // f43_0e7f
        {16, {{190, 30, 0x10DA}}},  // f44_05b6
        {17, {{306, 6, 0x110F}, {292, 120, 0x1121}}},  // f44_07cb
        {18, {{250, 90, 0x10DC}, {534, 166, 0x1245}}},  // f44_141e
        {20, {{82, 50, 0x1122}, {490, 6, 0x1123}}},  // f44_174d
        {21, {{192, 6, 0x1087}, {392, 6, 0x1088}}},  // f45_019e
        {22, {{192, 6, 0x1089}, {392, 6, 0x108A}}},  // f45_08a6
        {23, {{164, 6, 0x108B}, {364, 6, 0x108C}}},  // f45_0b53
        {26, {{132, 6, 0x108D}, {210, 268, 0x108E}, {328, 6, 0x108F}}},  // f46_018a
        {28, {{276, 38, 0x110D}, {114, 224, 0x110E}}},  // f46_0530
        {29, {{222, 6, 0x1110}, {450, 58, 0x1111}}},  // f46_0726
        {30, {{138, 94, 0x1102}, {356, 136, 0x1103}}},  // f46_0aa3
        {31, {{192, 6, 0x1099}, {376, 6, 0x109A}}},  // f47_015d
        {32, {{72, 32, 0x10FE}, {462, 32, 0x10FF}}},  // f47_03cf
        {33, {{134, 136, 0x10CB}, {54, 214, 0x10CC}, {214, 8, 0x10F1}}},  // f47_063b
        {34, {{76, 210, 0x1094}, {192, 6, 0x1095}, {342, 164, 0x1096}, {392, 20, 0x1092}}},  // f47_0bdb
        {35, {{170, 164, 0x1097}, {192, 6, 0x1098}}},  // f47_13e5
        {36, {{174, 80, 0x109B}, {394, 80, 0x109C}}},  // f48_05e1
        {37, {{176, 124, 0x109F}, {312, 228, 0x109D}}},  // f48_0932
        {38, {{268, 172, 0x10A1}, {468, 86, 0x10A2}, {466, 206, 0x109E}}},  // f48_0b82
        {39, {{86, 122, 0x10A3}, {386, 38, 0x10A4}, {386, 182, 0x10A5}}},  // f48_0e74
        {40, {{228, 76, 0x10A6}}},  // f48_115a
        {41, {{150, 70, 0x1119}, {352, 70, 0x111A}}},  // f49_025f
        {42, {{54, 256, 0x10A7}}},  // f49_05ca
        {43, {{208, 48, 0x10A9}}},  // f49_0780
        {44, {{208, 46, 0x10AA}, {418, 66, 0x10AB}}},  // f49_0918
        {45, {{86, 184, 0x10AC}, {310, 124, 0x10AD}}},  // f49_0f6a
        {46, {{194, 132, 0x10B1}, {420, 196, 0x10B2}}},  // f50_0187
        {47, {{188, 106, 0x10BE}, {376, 108, 0x10BF}, {54, 256, 0x10C0}, {328, 256, 0x10C1}, {248, 30, 0x10EC}, {498, 36, 0x10ED}, {54, 84, 0x10EE}}},  // f50_08d6
        {48, {{172, 150, 0x10C2}, {436, 150, 0x10C3}, {54, 218, 0x10C4}}},  // f50_0d7a
        {49, {{54, 138, 0x10C9}, {332, 138, 0x10CA}}},  // f50_1010
        {50, {{134, 136, 0x10CB}, {54, 214, 0x10CC}, {214, 8, 0x10F1}}},  // f50_1253
        {51, {{54, 194, 0x1117}, {394, 108, 0x1118}}},  // f51_01de
        {52, {{62, 82, 0x1138}, {410, 106, 0x1139}}},  // f51_0527
        {54, {{296, 28, 0x10B5}, {422, 38, 0x10B3}}},  // f51_0d47
        {55, {{324, 6, 0x10EF}}},  // f51_1cca
        {57, {{54, 32, 0x10FD}}},  // f52_0671
        {58, {{82, 78, 0x1104}, {332, 78, 0x1105}}},  // f52_0b8b
        {59, {{228, 100, 0x1163}}},  // f52_0ecd
        {60, {{54, 210, 0x112F}}},  // f52_11d7
        {63, {{162, 192, 0x10E4}, {382, 140, 0x10E5}, {190, 88, 0x10FA}, {152, 36, 0x10FB}}},  // f53_041a
        {64, {{54, 210, 0x1130}}},  // f53_0793
        {65, {{150, 192, 0x10EB}, {174, 6, 0x1132}}},  // f53_0981
        {66, {{132, 90, 0x10E6}}},  // f54_01e4
        {67, {{220, 176, 0x10E7}, {230, 14, 0x1113}, {144, 168, 0x1114}, {54, 142, 0x1115}}},  // f54_04b1
        {69, {{66, 210, 0x1131}}},  // f54_099d
        {70, {{142, 98, 0x1107}}},  // f54_0e45
        {71, {{54, 226, 0x1108}, {256, 226, 0x1109}, {304, 18, 0x110A}}},  // f55_0160
        {72, {{96, 152, 0x110B}, {388, 100, 0x110C}}},  // f55_039c
        {73, {{96, 116, 0x1106}}},  // f55_0582
        {74, {{128, 156, 0x1112}}},  // f55_07cb
        {75, {{68, 118, 0x1129}}},  // f55_0996
        {76, {{68, 14, 0x111B}, {326, 18, 0x111C}}},  // f56_0151
        {77, {{144, 6, 0x111D}, {376, 6, 0x111E}}},  // f56_035c
        {91, {{162, 144, 0x10E2}, {54, 254, 0x10E3}}},  // f59_0385
        {92, {{94, 56, 0x10DE}, {410, 162, 0x10DF}, {318, 170, 0x10E0}}},  // f59_0840
        {93, {{54, 6, 0x1133}, {226, 134, 0x1134}}},  // f59_0a5a
        {94, {{144, 84, 0x1128}}},  // f59_0c4a
        {96, {{166, 12, 0x1116}}},  // f60_03be
        {97, {{102, 78, 0x1124}, {314, 12, 0x1125}}},  // f60_0595
        {98, {{54, 6, 0x1126}, {490, 6, 0x1127}}},  // f60_0794
        {99, {{54, 6, 0x111F}, {242, 6, 0x1120}}},  // f60_099f
    };
    select(3);
    for (const Room& r : kRooms)
        if (r.room == room)
            for (const Picture& p : r.pictures) drawLogo(p.x, p.y, p.id);
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
    drawObjects();
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
    // The glass's marks (+F65, +F3D): sprite 1163 + the stage (2 at most).
    for (int i = 0; i < 5; ++i)
        if (crackStage_[i] != 0 && intersect(a, crackRect_[i]).w > 0 && intersect(a, crackRect_[i]).h > 0)
            drawLogo(crackRect_[i].x, crackRect_[i].y, static_cast<uint16_t>(0x1163 + std::min(crackStage_[i], 2)));
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
    currentRoom_ = room;
    worldW_ = table_.root.bottom.w, worldD_ = table_.root.bottom.h;
    applyPanelPhysics();
    // The ball (type 1, on the face under it) and its target, under it.
    hasBall_ = false;
    for (const Object& o : table_.objects)
        if (o.type == 1 || o.type == 3) {
            // (Type 3, f61_09bd → f06_0348: the player's ball with a
            // segment 5 part as well, not ported: as type 1.)
            hasBall_ = true;
            // f06_0043: radius 10 on the face under the point.
            ball_ = Ball{};
            ball_.cx = o.x, ball_.cy = o.y;
            ball_.cz = faceHeight(faceUnder(o.x, o.y), o.x, o.y) + ball_.r;
            ball_.kind = panel_.ballType;
            ball_.startX = o.x, ball_.startY = o.y;
            targetX_ = o.x + 10, targetY_ = o.y + 10;
            targetMoved_ = false;
            ballMoving_ = false, shadowShown_ = false;
            lastCentre_[0] = ball_.cx, lastCentre_[1] = ball_.cy, lastCentre_[2] = ball_.cz;
            for (int i = 0; i < 3; ++i) shadowSeen_[i] = lastCentre_[i];
        }
    captured_ = Control::None;
    ballTypePressed_ = shootPressed_ = false;
    roomBusy_ = false, exitRoom_ = 0, exitHole_ = -1;
    // The builder's settings; the targets (type 10 but kind 6, a suckhole:
    // not ported) counted ([30A]); each drawable's creation number (+1E:
    // the ball makes three, its shadow and target).
    roomConfig(room);
    targetsHit_ = targets_ = 0;
    int made = 0;
    for (Object& o : table_.objects) {
        o.id = made;
        made += (o.type == 1 || o.type == 3) ? 3 : 1;
        if (o.type != 10 || o.args[2] == 6) continue;
        o.kind = std::min(o.args[2], 10);
        o.points = std::min(o.args[1], 1000) / 100 * 100;
        o.frames = static_cast<int16_t>(data_[0x15CC + 0x18 * o.kind] | data_[0x15CC + 0x18 * o.kind + 1] << 8);
        o.hit = o.scored = o.frame = o.sequence = 0;
        o.live = true, o.shown = true;
        ++targets_;
    }
    for (int i = 0; i < 5; ++i) crackStage_[i] = 0;
    columns_[0] = columns_[1] = Column{};
    drawTable();
    roomPictures(room);
    toDisplay(3);
    redrawTable({0, 0, Screen::kWidth, Screen::kHeight});
    // The player's objects (f31_27de: +A4 the panel, +AA and +AC the
    // columns, their method +40); the columns keep the player's counts
    // (+BA, +BC: 7 and 0 for a new game, f31_1b48).
    lockControls();
    drawPanel();
    drawColumn(false);
    drawColumn(true);
}

void Science::showTable(int room) {
    // For testing (--room 1): the rooms from there, played till Escape.
    loadLook();
    looksConverted_ = true;
    arcade(room);
}

}  // namespace edison
