// The Folded Cube (segment 29, with segment 30's panels): a cube of
// coloured triangles turns in the middle; pick the flat net that folds up
// into it. Five cubes.

#include <algorithm>

#include "mystery/mystery.h"

namespace edison {

namespace {

constexpr int kSecondSlot = 6;
constexpr uint16_t kNets = 0x60EE;  // 37 nets of 3 x 4 cells; 0-28 fold up, 29-36 don't
constexpr int kGoodNets = 0x1D, kBadNets = 8;
// Panel DS:674A: the four nets.
constexpr int kNetsX = 0x1B2, kNetsY = 0xDE;
constexpr int kNet[4][4] = {{0x10, 0xE, 0x4C, 0x38}, {0x68, 0xE, 0x4C, 0x38}, {0x10, 0x52, 0x4C, 0x38}, {0x68, 0x52, 0x4C, 0x38}};
// Panel DS:6708: the arrows that turn the cube (DS:66E0).
constexpr int kArrowsX = 0x40, kArrowsY = 0x110;
constexpr int kArrow[4][4] = {{0x18, 2, 0x28, 0x19}, {0x28, 0x39, 0x28, 0x17}, {0, 0x1E, 0x28, 0x19}, {0x40, 0x19, 0x20, 0x1C}};
constexpr int kArrowMid[4][2] = {{0x14, 0xD}, {0x14, 0xC}, {0x14, 0xD}, {0x10, 0xE}};
// The preview of the cube.
constexpr int kCubeX = 0x10E, kCubeY = 0x89, kCubeW = 100, kCubeH = 0x55;

}  // namespace

void Mystery::cubeGadget() {
    // g30_0ad2: the machine at the bottom.
    static constexpr int kA[] = {0, 1, 2, 3, 4, 5, -1, 4, 6, 0, 1, 0};
    static constexpr int kB[] = {0, 1, 2, 3, 0, 1, 2, 3, 1, -1, -1, -1};
    music(0x1D);
    select(1);
    drawOpaque(0x15E, 0x162, 0x2252);
    for (size_t i = 0; i < std::size(kA); ++i) {
        if (kA[i] >= 0) drawOpaque(2, 0x10, static_cast<uint16_t>(0x2253 + kA[i]));
        if (kB[i] >= 0) drawOpaque(0xD4, 0x22, static_cast<uint16_t>(0x225A + kB[i]));
        waitCountdown(kA[i] >= 0 ? 2 : 4);
    }
    drawOpaque(0x15E, 0x162, 0x2251);
}

bool Mystery::foldedCube(int level) {
    // g29_10c0.
    level = std::clamp(level, 0, 7);
    static constexpr int kSeconds[8] = {120, 120, 180, 180, 240, 240, 300, 300};  // DS:62AA
    const int limit = kSeconds[level];
    int timeLeft = limit, timeShown = -1;  // [C770], [66BA]
    int points = 0;                         // [B73A]
    int misses = 0;                         // [B396]
    int cubesLeft = 5;                      // [C655]
    bool allRight = true;                   // DS:C27A: each cube got right
    auto word = [&](uint16_t at) { return static_cast<int16_t>(dataWord(at)); };

    // The cube: 8 corners, 12 triangles (two to a face), each triangle's
    // colour 0x21 + 2k (DS:B722).
    std::vector<Point3> corners;
    for (int k = 0; k < 8; ++k)
        corners.push_back({word(static_cast<uint16_t>(0x62E4 + 6 * k)), word(static_cast<uint16_t>(0x62E6 + 6 * k)),
                           word(static_cast<uint16_t>(0x62E8 + 6 * k))});
    int tri[12][3];
    for (int t = 0; t < 12; ++t)
        for (int k = 0; k < 3; ++k) tri[t][k] = word(static_cast<uint16_t>(0x6392 + 2 * (t * 3 + k)));
    int colour[12];
    uint16_t turnA = 0, turnC = 0;  // [9396], [939A]

    auto showScore = [&] { intBox(0x90, 0x3E, 0x2C, 0x14, points); };  // g30_0000
    auto showTime = [&] {                                                           // g30_0046
        if (timeLeft == timeShown) return;
        digitalTime(0x1AA, 0x42, 0x28, 0x14, timeLeft);
        timeShown = timeLeft;
    };
    auto drawCube = [&] {  // g29_0000(1): back to front, each triangle with its edges
        select(2);
        duplicateArea(2, 2, 0, 0, kCubeW, kCubeH, kCubeX, kCubeY);
        int16_t m[9];
        rotation(turnA, 0, turnC, m);
        std::vector<Point3> pts = corners;
        transform(pts, m);
        translate(pts, 0, 0x300, 0);
        int order[12];
        long depth[12];
        for (int t = 0; t < 12; ++t) {
            order[t] = t;
            depth[t] = (static_cast<long>(pts[tri[t][0]].y) + pts[tri[t][1]].y + pts[tri[t][2]].y) / 3;
        }
        for (int i = 0; i < 12; ++i)
            for (int f = 0; f < 11 - i; ++f)
                if (depth[f] <= depth[f + 1]) {
                    std::swap(order[f], order[f + 1]);
                    std::swap(depth[f], depth[f + 1]);
                }
        const auto flat = project(pts);
        if (flat.size() == 8) {
            for (int o : order) {
                std::vector<std::pair<int, int>> p;
                for (int k = 0; k < 3; ++k) p.push_back(flat[static_cast<size_t>(tri[o][k])]);
                // draw_poly's list (f32_2e46): the triangle, then its
                // edges as two-point polygons of colour 0 (each record's
                // header word is colour << 8 | count: 0x0002).
                fillPolygonSolid(p, static_cast<uint8_t>(colour[o]));
                for (int k = 0; k < 3; ++k) fillPolygonSolid({p[k], p[(k + 1) % 3]}, 0);
            }
        }
        copyArea(2, 1, kCubeX, kCubeY, kCubeW, kCubeH);
        select(1);
    };

    // DS:B7B2: the four nets: right or not, which net, answered.
    struct Choice {
        bool right = false, answered = false;
        int net = 0;
        int x[12] = {}, y[12] = {}, turned[12] = {};  // where each triangle's square is
    } choices[4];
    // Each face's square: the triangle coloured into 0x9E and into 0x9F.
    static constexpr int kFace[7][2] = {{0, 0}, {3, 2}, {11, 10}, {7, 6}, {4, 5}, {0, 1}, {8, 9}};
    auto square = [&](Choice& c, int x, int y, int face, int turned, const int* col) {
        drawOpaque(x, y, static_cast<uint16_t>(0x207D + turned));
        recolour(x, y, 0x10, 0x10, 0x9E, static_cast<uint8_t>(col[kFace[face][0]]));
        recolour(x, y, 0x10, 0x10, 0x9F, static_cast<uint8_t>(col[kFace[face][1]]));
        for (int t : kFace[face]) c.x[t] = x, c.y[t] = y, c.turned[t] = turned;
    };
    auto newCube = [&] {  // g29_0518
        // Drawn face by face (DS:B728, B726, B738, ...: kFace's order).
        static constexpr int kDrawn[12] = {3, 2, 11, 10, 7, 6, 4, 5, 0, 1, 8, 9};
        for (int t : kDrawn) colour[t] = random(6) * 2 + 0x21;
        for (Choice& c : choices) c = Choice{};
        const int right = random(4);
        choices[right].right = true;
        choices[right].net = random(kGoodNets);
        for (Choice& c : choices)
            if (!c.right) c.net = random(kGoodNets);
        // Some wrong ones are nets that don't fold at all.
        for (int bad = level == 0 ? 3 : level == 1 ? 2 : level == 2 ? 1 : 0; bad > 0; --bad) {
            int k;
            do k = random(4);
            while (choices[k].right || choices[k].net >= kGoodNets);
            choices[k].net = random(kBadNets) + kGoodNets;
        }
        select(1);
        for (int k = 0; k < 4; ++k) {
            Choice& c = choices[k];
            const int bx = kNetsX + kNet[k][0], by = kNetsY + kNet[k][1];
            fill(bx, by, kNet[k][2], kNet[k][3], 0x7A);
            for (int r = 0; r < 3; ++r)
                for (int col = 0; col < 4; ++col) {
                    int face = data_[static_cast<uint16_t>(kNets + c.net * 12 + r * 4 + col)];
                    if (!face) continue;
                    const int turned = face >= 8;
                    if (turned) face -= 8;
                    if (face >= 1 && face <= 6) square(c, bx + 4 + col * 0x10, by + 4 + r * 0x10, face, turned, colour);
                }
        }
        // And the colours of the wrong ones are spoilt: two faces traded
        // (the hardest level), or some squares recoloured.
        static constexpr int kPairs[6][2] = {{2, 3}, {10, 11}, {6, 7}, {5, 4}, {9, 8}, {1, 0}};
        for (Choice& c : choices) {
            if (c.right) continue;
            if (level == 7) {
                if (c.net >= kGoodNets) continue;
                const int u = random(6);
                int v;
                do v = random(6);
                while (v == u);
                auto put = [&](int at, int from) {
                    const int t = kPairs[at][0], t2 = kPairs[at][1];
                    drawOpaque(c.x[t], c.y[t], static_cast<uint16_t>(0x207D + c.turned[t]));
                    recolour(c.x[t], c.y[t], 0x10, 0x10, 0x9F, static_cast<uint8_t>(colour[kPairs[from][0]]));
                    recolour(c.x[t2], c.y[t2], 0x10, 0x10, 0x9E, static_cast<uint8_t>(colour[kPairs[from][1]]));
                };
                put(u, v);
                put(v, u);
            } else {
                for (int n = level < 4 ? 6 : level == 4 ? 4 : level == 5 ? 2 : 1; n > 0; --n) {
                    const int t = random(12);
                    int to;
                    do to = random(6) * 2 + 0x21;
                    while (to == colour[t]);
                    recolour(c.x[t], c.y[t], 0x10, 0x10, static_cast<uint8_t>(colour[t]), static_cast<uint8_t>(to));
                }
            }
        }
    };

    // The screen, and the face colours (resource 218: screen 2's colours 0x20-0x2B).
    clearInput();
    ctx_.blackout();
    ctx_.showFullScreen(0x1002, 2);
    {
        std::vector<uint8_t> raw;
        if (ctx_.read(0x218, raw))
            for (size_t k = 0; k + 2 < raw.size() && k / 3 < 12; k += 3)
                ctx_.screens[2].palette[0x20 + k / 3] = Rgb{raw[k + 2], raw[k + 1], raw[k]};
    }
    select(2);
    drawOpaque(0x15E, 0x162, 0x2251);
    text(0x132, 0x168, "5", 0x9D);
    showScore();

    bool quit = false, redraw = false, gadget = false;
    int pressedNet = -1;
    bool roundDone = false;
    panels_.clear();
    Panels::Panel arrows;  // DS:6708
    arrows.x = kArrowsX, arrows.y = kArrowsY, arrows.w = 0x62, arrows.h = 0x56;
    for (const auto& a : kArrow) arrows.buttons.push_back({a[0], a[1], a[2], a[3]});
    auto arrowAt = [&](int k, uint16_t id) {
        drawCentred(kArrowsX + kArrow[k][0] + kArrowMid[k][0], kArrowsY + kArrow[k][1] + kArrowMid[k][1], id);
    };
    arrows.onPress = [&](int k) {  // g30_067e
        if (k < 0) return;
        music(0x12);
        arrowAt(k, static_cast<uint16_t>(0x2071 + k));
    };
    arrows.onRelease = [&](int k) {  // g30_0758
        if (k >= 0) arrowAt(k, static_cast<uint16_t>(0x206D + k));
    };
    arrows.whileHeld = [&](int k) {  // g30_0828
        if (k == 0) turnA = static_cast<uint16_t>(turnA - 0x180);
        else if (k == 1) turnA = static_cast<uint16_t>(turnA + 0x180);
        else if (k == 2) turnC = static_cast<uint16_t>(turnC - 0x180);
        else if (k == 3) turnC = static_cast<uint16_t>(turnC + 0x180);
        if (k >= 0) redraw = true;
    };
    panels_.add(arrows);
    Panels::Panel nets;  // DS:674A
    nets.x = kNetsX, nets.y = kNetsY, nets.w = 0xC6, nets.h = 0x9C;
    for (const auto& n : kNet) nets.buttons.push_back({n[0], n[1], n[2], n[3]});
    nets.onPress = [&](int k) {  // g30_0988: a tick or a cross
        if (k < 0 || choices[k].answered) return;
        pressedNet = k;
        drawCentred(kNetsX + kNet[k][0] + kNet[k][2] / 2, kNetsY + kNet[k][1] + kNet[k][3] / 2,
                    choices[k].right ? 0x2081 : 0x2082);
        if (!choices[k].right) ++misses;
    };
    nets.onRelease = [&](int k) {  // g30_09fe
        if (k < 0 || k != pressedNet || choices[k].answered) return;
        if (choices[k].right) {
            music(0x25);
            roundDone = true;
            points += 0x32;
        } else {
            music(0x26);
            points -= 0x32;
        }
        showScore();
        choices[k].answered = true;
        pressedNet = -1;
    };
    panels_.add(nets);
    Panels::Panel exit;  // DS:66C6
    exit.x = 0x218, exit.y = 0x34, exit.w = 0x28, exit.h = 0x20;
    exit.buttons = {{0, 0, 0x28, 0x20}};
    exit.onPress = [&](int k) {  // g30_0632
        if (k >= 0) quit = true;
    };
    panels_.add(exit);
    Panels::Panel machine;  // DS:676E
    machine.x = 0x15E, machine.y = 0x162, machine.w = 0x2A, machine.h = 0x20;
    machine.buttons = {{0, 0, 0x2A, 0x20}};
    machine.onPress = [&](int k) {  // g30_0a9a
        if (k >= 0) gadget = true;
    };
    panels_.add(machine);
    setView(0xAA, 0x54, 0x1D6, 0x110);
    helpPanel(0xC, 0x22, 0x4A, 0x2A);
    ctx_.timer.setPeriodic(kSecondSlot, 1, [&] {  // g30_0026
        if (timeLeft > 0) --timeLeft;
    });

    while (cubesLeft > 0 && !quit) {
        turnA = static_cast<uint16_t>(random(0x10000));
        turnC = static_cast<uint16_t>(random(0x10000));
        if (cubesLeft == 5) {  // f04_005c(2, 4) after the turn's draws (its stir too)
            show(2);
            computeUiColours();
            select(1);
            showTime();
            // The empty preview is kept at the top left of screen 2 (once
            // shown: 29:127B).
            duplicateArea(2, 2, kCubeX, kCubeY, kCubeW, kCubeH, 0, 0);
        }
        roundDone = false;
        newCube();
        drawCube();
        while (!roundDone && !quit) {
            panels_.poll(ctx_.platform);
            ctx_.pump();
            showTime();
            if (redraw) {
                redraw = false;
                drawCube();
            }
            if (gadget) {
                gadget = false;
                cubeGadget();
            }
            idleHint(3);
            if (helpPressed_) {
                select(1);
                drawOpaque(0xC, 0x22, 0x207F);
                help(0x3F05);
                drawOpaque(0xC, 0x22, 0x2080);
            }
        }
        if (!roundDone) break;
        --cubesLeft;
        select(1);
        fill(0x132, 0x168, 0x10, 0x10, 0xB2);
        text(0x132, 0x168, std::to_string(cubesLeft), 0x9D);
    }
    allRight = cubesLeft == 0;
    int used = 0;  // [B794]
    if (allRight) {
        used = limit - timeLeft;
        if (misses <= 5) {
            // 100 points, and 2 for each second left.
            misses = 0;
            points += 100;
            while (timeLeft > 0) {
                --timeLeft;
                showTime();
                points += 2;
                showScore();
            }
        } else {
            misses = 2;
        }
        timeLeft = limit - used;
        showTime();
    }
    drawOpaque(0x218, 0x34, 0x2075);
    if (quit) {
        points = 0;
        waitCountdown(6);
    }
    ctx_.timer.setPeriodic(kSecondSlot, 0, nullptr);
    panels_.clear();
    clearInput();
    puzzleResult(allRight, points, misses, used);
    select(1);
    fill(0, 0, Screen::kWidth, Screen::kHeight, 0);
    return allRight;
}

}  // namespace edison
