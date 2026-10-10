// Wild Science Arcade: each room's own word on its holes (the room's
// method 8, segments 41-60) and the room's end (f38_020f: its bonus). See
// docs/SCIENCE.md.

#include <algorithm>
#include <string>
#include <tuple>

#include "science/science.h"

namespace edison {

namespace {

// The room's own fields (roomVar_).
enum { kFA2, kFA4, kFA6, kFC0, kFC4, kFBE, kFC2 };

}  // namespace

// --- the boxes the rooms use (segment 24) ------------------------------------

void Science::say(int face, int style, uint16_t message, uint16_t narration, uint16_t picture, int align) {
    // f24_00df (style) or, with a picture, f24_01be (style 0, the picture
    // aligned in the box): one OK; Edison's face [1F1C]; the narration
    // (f24_1918: 6100h and up), then a key or click.
    const Rect& v = table_.view;
    Dialog d;
    d.centreX = v.x + (v.w >> 1), d.centreY = v.y + (v.h >> 1);
    d.face = face, d.style = picture ? 0 : style, d.lines = 1, d.single = true, d.message = message;
    d.picture = picture, d.align = align;
    dialogOpen(d);
    if (narration >= 0x6100) this->narration(narration, false);
    dialogRun(d);
}

int Science::askButtons(int face, uint16_t message, uint16_t firstLine, uint16_t narration) {
    // f24_0482: two buttons (texts firstLine, firstLine + 1), style 0.
    const Rect& v = table_.view;
    Dialog d;
    d.centreX = v.x + (v.w >> 1), d.centreY = v.y + (v.h >> 1);
    d.face = face, d.style = 0, d.lines = 2, d.message = message, d.firstLine = firstLine;
    dialogOpen(d);
    if (narration >= 0x6100) this->narration(narration, false);
    return dialogRun(d);  // the button chosen (f24_1804: its +28)
}

std::string Science::askCode(int face, uint16_t message, uint16_t narration) {
    // f24_03a3: style 1, typing.
    const Rect& v = table_.view;
    Dialog d;
    d.centreX = v.x + (v.w >> 1), d.centreY = v.y + (v.h >> 1);
    d.face = face, d.style = 1, d.lines = 1, d.single = true, d.typed = true, d.message = message;
    dialogOpen(d);
    if (narration >= 0x6100) this->narration(narration, false);
    dialogRun(d);
    return d.text;  // f24_1804: its +40
}

bool Science::sameCode(const std::string& typed, const char* code) {
    // Borland's strnicmp(typed, code, 40h) == 0.
    auto upper = [](char c) { return c >= 'a' && c <= 'z' ? static_cast<char>(c - 0x20) : c; };
    for (size_t i = 0; i < 0x40; ++i) {
        const char a = i < typed.size() ? typed[i] : 0, b = code[i];
        if (upper(a) != upper(b)) return false;
        if (a == 0) return true;
    }
    return true;
}

void Science::closeHole(Object& o) {
    // f28_1514: shut (+2F): drawn closed and no longer taking the ball.
    o.closed = true;
    viewDirty_ = true;
}

// --- each room's method 8 (segments 41-60) -----------------------------------

void Science::roomHole(Object& o) {
    // The room's method 8 when a hole has swallowed the ball (f28_156a). The
    // hole's +C (args[0]) is where it leads; holeGo is f27_2530. The rooms
    // not listed only go on (their method 8 is a thunk to it). Each box is
    // in the middle of the room's window; "say(face, style, text, sound)"
    // is a box with OK.
    // (The thunks of rooms 51-100: 51 f51_01c2, 52 f51_050b, 53 f51_0771,
    // 56 f52_0126, 61 f53_011a, 62 f53_0280, 63 f53_03fe, 68 f54_0690, 71
    // f55_0144, 74 f55_07af, 76 f56_0135, 78 f56_04f1, 79 f56_0652, 80
    // f56_07b3, 81 f57_011a, 82 f57_027b, 83 f57_03dc, 84 f57_053d, 85
    // f57_069e, 86 f58_011a, 87 f58_027b, 88 f58_03dc, 89 f58_053d, 90
    // f58_069e, 99 f60_0983, 100 f60_0b34.)
    int& to = o.args[0];
    auto go = [&] { holeGo(o); };
    auto bonus = [&](long v) { completionBonus_ = v; };
    switch (currentRoom_) {
    case 1:  // f41_02a4: EXIT asks first; the warp codes of levels 4 and 5
        if (to == 504) {
            const Rect& v = table_.view;
            Dialog d;
            d.centreX = v.x + (v.w >> 1), d.centreY = v.y + (v.h >> 1);
            d.face = 0, d.style = 0, d.lines = 2, d.message = 0x20B, d.firstLine = 0x202;
            dialogOpen(d);
            sound(0x6016);
            if (dialogRun(d) == 0) spitBall(o, 2);
            else go();
            return;
        }
        if (to == 508 || to == 509) {
            const bool right = sameCode(askCode(2, 2, 0x6102), to == 508 ? "electric" : "wildway");
            say(0, 1, right ? 0x205 : 0x204, right ? 0x6148 : 0x6147);
            if (!right) {
                spitBall(o, 2);
                return;
            }
            levelBonus_ = to == 508 ? 60000 : 75000;
        }
        go();
        return;
    case 2:  // f41_09e6: two hints (then the hole shuts), a door to 9
        if (to == 100 || to == 101) {
            say(1, 0, to == 100 ? 0x27 : 0x46, to == 100 ? 0x6126 : 0x6144);
            spitBall(o, 1);
            closeHole(o);
            return;
        }
        if (to == 1001) to = 9;
        go();
        return;
    case 5:  // f41_14f8
        if (to == 1000) bonus(2000), to = 4;
        go();
        return;
    case 9:  // f42_0893
    case 10:  // f42_0e55
        if (to == 100) {
            say(1, 1, currentRoom_ == 9 ? 0x2A : 0x2D, currentRoom_ == 9 ? 0x6129 : 0x612C);
            spitBall(o, 1);
            closeHole(o);
            return;
        }
        go();
        return;
    case 14:  // f43_0bce: the bonus door (C46h) to 10, a ball to win
        // (f27_0859 fills the shares from the one given to the last: 3-5 0.)
        if (to == 0xC46) bonusBalls_ = 1, bonus(1500), std::fill(completionShare_ + 3, completionShare_ + 6, uint8_t{0}), to = 10;
        else bonus(0);
        go();
        return;
    case 16:  // f44_01b2: two questions
        if (to == 100 || to == 101) {
            const bool right = to == 100 ? sameCode(askCode(2, 0x2F, 0x612E), "repel") : sameCode(askCode(2, 0x30, 0x612F), "attract");
            if (right) say(2, 1, 0x205, 0x6148), bonus(500);
            else say(0, 0, 0x20E, 0x6152), bonus(0);
            spitBall(o, 1);
            closeHole(o);
            return;
        }
        go();
        return;
    case 18:  // f44_0f60: the magnet (+FC4: dropped into its slot, roomTick)
        if (to == 100) {
            if (roomVar_[kFC4]) {
                say(1, 0, 0x34, 0x6133);
                spitBall(o, 1);
                return;
            }
            say(1, 1, 0x32, 0x6131);
            if (askButtons(1, 0x207, 0x208, 0x614A) == 0) {
                to = 60;  // give up
                go();
                return;
            }
            // Try again: the switch 1001 (f27_0a5a: its RETRY, OBJ3) on
            // (its method 3, f04_070b: the room started again), the ball
            // out.
            for (Object& r : table_.objects)
                if (r.type == 7 && r.args[5] == 1001) {
                    retry(r, 1);
                    break;
                }
            spitBall(o, 1);
            return;
        }
        go();
        return;
    case 21:  // f45_021c
        if (to == 35) {
            // Only with every target hit, or in one shot.
            if (targetsHit_ >= targets_ || shots_ <= 1) {
                go();
                return;
            }
            say(2, 0, 0x616, 0x618E);
            spitBall(o, 2);
            return;
        }
        if (to == 50) {
            // A warp machine: four codes, each a lesson and its points.
            const std::string code = askCode(2, 2, 0x6102);
            bool right = true;
            if (sameCode(code, "40 Newtons") || sameCode(code, "40Newtons")) levelBonus_ = 30000, to = 506;
            else if (sameCode(code, "magnetic")) levelBonus_ = 45000, to = 507;
            else if (sameCode(code, "electric")) levelBonus_ = 60000, to = 508;
            else if (sameCode(code, "wildway")) levelBonus_ = 75000, to = 509;
            else right = false;
            if (right) {
                say(0, 1, 0x205, 0x6148);
                roomEndFlag_ = true, bonus(0);
                go();
            } else {
                say(0, 1, 0x204, 0x6147);
                spitBall(o, 2);
            }
            return;
        }
        if (to == 101) {
            say(1, 1, 0, 0x6100);
            spitBall(o, 2);
            closeHole(o);
            return;
        }
        if (to == 102) {
            say(1, 0, 1, 0x6101, 0x135F, 0x11);
            spitBall(o, 2);
            closeHole(o);
            return;
        }
        go();
        return;
    case 22:  // f45_0924
        if (to == 40) bonus(2000);
        go();
        return;
    case 23:  // f45_0bd1: two counters (the doors to 109 and 110), then 44
        if (to == 109 || to == 110) {
            if (roomVar_[kFA2] || roomVar_[kFA4]) say(2, 1, 9, 0x6109);  // both set
            else say(2, 1, 8, 0x6108);                                   // the first
            if (to == 109) roomVar_[kFA2] = 1, say(2, 0, 0x5EC, 0x6164);
            else roomVar_[kFA4] = 1, say(2, 0, 0x5ED, 0x6165);
            spitBall(o, 2);
            closeHole(o);
            return;
        }
        if (to == 44) {
            if (!roomVar_[kFA2] || !roomVar_[kFA4]) {
                say(1, 1, 0xA, 0x610A);
                spitBall(o, 2);
                return;
            }
            bonus(0), roomVar_[kFA6] = 1;
            go();
            return;
        }
        if (to == 34) {
            if (!roomVar_[kFA6]) {
                say(1, 1, 0xB, 0x610B);
                if (askButtons(1, 0x207, 0x202, 0x614A) == 1) {
                    spitBall(o, 2);
                    return;
                }
            }
            go();
            return;
        }
        if (to == 23) {
            spitBall(o, 2);
            return;
        }
        go();
        return;
    case 28:  // f46_04f6
        if (to == 0xC46) bonus(3000), to = 5;
        go();
        return;
    case 30:  // f46_0942: a question; the hole shuts either way
        if (to == 1000) {
            askCode(2, 0x214, 0x6157);
            say(2, 1, 0x205, 0x6148);
            bonus(500);
            spitBall(o, 2);
            closeHole(o);
            return;
        }
        go();
        return;
    case 32:  // f47_0396
        if (to == 100) bonus(1000), to = 98;
        go();
        return;
    case 33:  // f47_060a
        if (to == 52) bonus(0);
        go();
        return;
    case 34:  // f47_097f ([FA4], [FA6]: set when coming in from 22 or 40)
        if (to == 114 || to == 117) {
            say(2, to == 114 ? 0 : 1, to == 114 ? 0xD : 0x11, to == 114 ? 0x610D : 0x6110);
            spitBall(o, 1);
            closeHole(o);
            return;
        }
        if (to == 36) {
            if (!roomVar_[kFA4] || !roomVar_[kFA6]) say(1, 1, 0xE, 0x610E);
            bonus(2000);
            go();
            return;
        }
        if (to == 40 || to == 22) bonus(0);
        go();
        return;
    case 35:  // f47_1061
        if (to == 105) {
            say(2, 1, 4, 0x6104);
            spitBall(o, 1);
            closeHole(o);
            return;
        }
        if (to == 1000 || to == 1001 || to == 1002) {
            spitBall(o, 2);
            if (to == 1000) closeHole(o);
            return;
        }
        if (to == 107 || to == 108) {
            say(1, 1, to == 107 ? 6 : 7, to == 107 ? 0x6106 : 0x6107);
            spitBall(o, 2);
            closeHole(o);
            return;
        }
        if (to == 43) {
            bonus(0);
            go();
            return;
        }
        if (to == 23) {
            if (!roomVar_[kFC0]) say(1, 1, 5, 0x6105);
            bonus(2000);
            go();
            return;
        }
        go();
        return;
    case 36:  // f48_01e8: a hole in one
        if (to == 50) {
            if (askButtons(1, 0x11, 0x208, 0x6111) == 1) {
                // Try again.
                shots_ = 0, viewDirty_ = true;
                spitBall(o, 2);
                return;
            }
            bonus(0);
            say(1, 1, 0x13, 0x6113);
            to = 506;
            go();
            roomEndFlag_ = true;
            return;
        }
        if (to == 26) {
            if (shots_ <= 1) {
                say(2, 0, 0x12, 0x6112);
                say(2, 1, 0x5EE, 0x6166);
                go();
                return;
            }
            say(1, 1, 0x3A, 0x6139);
            shots_ = 0, viewDirty_ = true;
            spitBall(o, 2);
            return;
        }
        go();
        return;
    case 39:  // f48_0d7b
        if (to == 100) {
            say(2, 0, 0x26, 0x6125, 0x10DB, 0x11);
            spitBall(o, 2);
            return;
        }
        if (to == 506) roomEndFlag_ = true, bonusBalls_ = 1;
        go();
        return;
    case 40:  // f48_1129
        if (to == 22) bonus(2000);
        go();
        return;
    case 41:  // f49_022e
        if (to == 65) bonus(2000);
        go();
        return;
    case 47:  // f50_04a7
        if (to == 101 || to == 102 || to == 103) {
            say(2, 0, to == 101 ? 0x18 : to == 102 ? 0x19 : 0x25, to == 101 ? 0x6118 : to == 102 ? 0x6119 : 0x6124);
            spitBall(o, 2);
            return;
        }
        if (to == 23) {
            if (sameCode(askCode(1, 0x1A, 0x611A), "gravity")) {
                say(2, 1, 0x205, 0x6148);
                bonus(completionBonus_ * 2);
                to = 53;
                closeHole(o);
                gameFlag_[1] = 1;
            } else {
                say(0, 0, 0x20F, 0x6152);
                bonus(0);
            }
            go();
            return;
        }
        if (to == 33) gameFlag_[0] = 1;
        go();
        return;
    case 48:  // f50_0b5d
        if (to == 21) {
            if (sameCode(askCode(2, 0x1E, 0x611D), "equals")) {
                say(2, 0, 0x205, 0x6148);
                bonus(1000);
                spitBall(o, 2);
            } else {
                say(2, 0, 0x204, 0x6147);
                bonus(0);
                go();
            }
            return;
        }
        go();
        return;
    case 49:  // f50_0fdf
        if (to == 41) bonus(2000);
        go();
        return;
    case 54:  // f51_0ac5
        if (to == 100) {
            say(2, 0, 0x22, 0x6121, 0x1360, 0x11);
            gameFlag_[0] = 1;
            spitBall(o, 2);
            closeHole(o);
            return;
        }
        if (to == 101 || to == 102) {
            say(1, to == 101 ? 0 : 1, to == 101 ? 0x23 : 0x24, to == 101 ? 0x6122 : 0x6123);
            if (to == 101) gameFlag_[1] = 1;
            spitBall(o, 2);
            closeHole(o);
            return;
        }
        if (to == 48 || to == 51) {
            if (to == 48) gameFlag_[2] = 1;
            bonus(0);
            go();
            closeHole(o);
            return;
        }
        go();
        return;
    case 55:  // f51_1859
        if (to == 100) {
            say(1, 0, 0x1B, 0x611B);
            spitBall(o, 2);
            closeHole(o);
            return;
        }
        if (to == 2) {
            if (askButtons(1, 0x207, 0x208, 0x614A) == 0) {
                say(1, 1, 0x21, 0x6120);
                roomEndFlag_ = true, bonus(4000), to = 507;
                go();
            } else {
                spitBall(o, 1);
            }
            return;
        }
        if (to == 3) {
            if (roomVar_[kFA2] && roomVar_[kFA4]) {
                go();
                return;
            }
            say(1, 1, 0x1C, 0x611C);
            spitBall(o, 2);
            return;
        }
        if (to == 101) {
            if (askButtons(1, 0x20C, 0x20D, 0x614F) == 0) {
                // Start the screen again: no shots, the counters off, the
                // magnetic balls (+FB4, +FB6) back where the builder put
                // them (f08_056e).
                if (completionBonus_ < 0) bonus(0);
                shots_ = 0, viewDirty_ = true;
                roomVar_[kFA2] = roomVar_[kFA4] = 0;
                if (roomObj_[0] >= 0) putBody(table_.objects[static_cast<size_t>(roomObj_[0])].body, 403, 223);
                if (roomObj_[1] >= 0) putBody(table_.objects[static_cast<size_t>(roomObj_[1])].body, 550, 248);
                spitBall(o, 2);
            } else {
                spitBall(o, 1);
            }
            return;
        }
        go();
        return;
    case 57:  // f52_02ff
        if (to == 100) {
            say(0, 0, 0x3B, 0, 0x10FC, 0x11);
            spitBall(o, 1);
            closeHole(o);
            return;
        }
        if (to == 101) {
            say(1, 0, 0x3C, 0x613A);
            spitBall(o, 1);
            closeHole(o);
            return;
        }
        if (to == 102) {
            if (roomVar_[kFC0]) {
                spitBall(o, 0);
                return;
            }
            if (sameCode(askCode(2, 0x211, 0x6154), "force")) say(2, 1, 0x205, 0x6148);
            else say(0, 0, 0x20F, 0x6152), bonus(0);
            ++roomVar_[kFC0];
            spitBall(o, 1);
            return;
        }
        go();
        return;
    case 58:  // f52_0814
        if (to == 0xC46) {
            if (!roomVar_[kFC0]) {
                if (sameCode(askCode(2, 0x217, 0x615A), "earth")) say(2, 1, 0x205, 0x6148);
                else say(0, 0, 0x20F, 0x6152), bonus(0);
                ++roomVar_[kFC0];
            }
            spitBall(o, 2);
            return;
        }
        if (to == 100) {
            say(1, 1, 0x3D, 0x613B);
            spitBall(o, 2);
            return;
        }
        if (to == 101) {
            say(1, 1, 0x3E, 0x613C);
            spitBall(o, 0);
            return;
        }
        if (to == 102) {
            spitBall(o, 2);
            return;
        }
        go();
        return;
    case 59:  // f52_0ddd: the end of a level: the room's end now
        if (to == 507) {
            bonus(5000), roomEndFlag_ = true, bonusBalls_ = 1;
            roomEnd();
            say(1, 1, 0x39, 0x6138);
        }
        go();
        return;
    case 60:  // f52_11b3
        to = 64;
        go();
        return;
    case 64:  // f53_076f
        to = 69;
        go();
        return;
    case 65:  // f53_093c
    case 73:  // f55_053d
        if (to == (currentRoom_ == 65 ? 510 : 509)) roomEndFlag_ = true;
        go();
        return;
    case 66:  // f54_01b3
        if (to == 77) bonus(2000);
        go();
        return;
    case 67:  // f54_0480
        if (to == 94) bonus(2000);
        go();
        return;
    case 69:  // f54_0970
        roomEndFlag_ = true, to = 508;
        go();
        return;
    case 70:  // f54_0be7
        if (to == 32) {
            if (targetsHit_ >= targets_) go();
            else spitBall(o, 0);
            return;
        }
        if (to == 0xC46) {
            if (!roomVar_[kFC0]) {
                if (sameCode(askCode(2, 0x21A, 0x615D), "electricity")) say(2, 1, 0x205, 0x6148);
                else say(0, 0, 0x20F, 0x6152), bonus(0);
                ++roomVar_[kFC0];
            }
            spitBall(o, 1);
            return;
        }
        go();
        return;
    case 72:  // f55_036b
        if (to == 97) bonus(3000);
        go();
        return;
    case 75:  // f55_0965
        if (to == 96) bonus(2000);
        go();
        return;
    case 77:  // f56_032b
        if (to == 41) bonus(2000);
        go();
        return;
    case 91:  // f59_0251
        if (to == 0xC46) {
            // (f27_0859: the completion's shares from the given shot on.)
            bonusBalls_ = 1, bonus(2000), completionShare_[2] = 0x20, to = 14;
            std::fill(completionShare_ + 3, completionShare_ + 6, uint8_t{0});
            go();
            return;
        }
        if (to == 100) {
            say(1, 1, 0x43, 0x6140);
            spitBall(o, 2);
            closeHole(o);
            return;
        }
        bonus(0);
        go();
        return;
    case 92:  // f59_07df: shares 2 on 2Ah, 3 on 0 (f27_0859)
        if (to == 0xC46) {
            bonusBalls_ = 1, bonus(1500), completionShare_[2] = 0x2A, to = 93;
            std::fill(completionShare_ + 3, completionShare_ + 6, uint8_t{0});
        }
        go();
        return;
    case 93:  // f59_0a15
        if (to == 0xC46) bonusBalls_ = 1, to = 91;
        else bonus(0);
        go();
        return;
    case 94:  // f59_0c19
        if (to == 75) bonus(2000);
        go();
        return;
    case 95:  // f59_0de4
        if (to == 75) bonus(0);
        go();
        return;
    case 96:  // f60_02c8
        if (to == 100) {
            say(1, 0, 0x36, 0x6134);
            spitBall(o, 2);
            closeHole(o);
            return;
        }
        if (to == 66) bonus(5000);
        go();
        return;
    case 97:  // f60_0564
        if (to == 71) bonus(2000);
        go();
        return;
    case 98:  // f60_0763
        if (to == 20) bonus(3000);
        go();
        return;
    default:
        // The thunks to f27_2530, rooms 3-50: f41_0f20 (3), f41_1116 (4),
        // f42_0188 (6), f42_03aa (7), f42_057d (8), f43_011a (11), f43_0421
        // (12), f43_09de (13), f43_0e63 (15), f44_07af (17), f44_15a1 (19),
        // f44_1731 (20), f45_11fe (24), f45_135f (25), f46_016e (26),
        // f46_0344 (27), f46_070a (29), f47_0141 (31), f48_0916 (37),
        // f48_0b66 (38), f49_05ae (42), f49_0764 (43), f49_08fc (44),
        // f49_0f4e (45), f50_016b (46), f50_11e0 (50).
        go();
        return;
    }
}

long Science::bonusLines(long counted, std::vector<std::string>* lines, int* count, int* highlight) const {
    // f38_0003: the bonus by the shots (+F87, 128ths of +F7B, kept within
    // 0-100000; from the fifth shot on, the fifth's); with `lines`, the
    // list "1 Shot.... Bonus: n", "k Shots... Bonus: n" for 2-4 and " Your
    // Bonus:   " with what's left of the player's (`counted` taken off),
    // which is the highlighted one.
    if (highlight) *highlight = 5;
    const long bonus = std::clamp(completionBonus_, 0L, 100000L);
    const int shots = std::min(shots_, 5);
    if (lines) {
        lines->clear();
        bool found = false;
        for (int k = 1; k < 6; ++k) {
            long v = completionShare_[k] * bonus / 0x80;
            std::string s;
            if (k == 5 && !found) {
                v = completionShare_[shots] * bonus / 0x80 - counted;
                if (highlight) *highlight = static_cast<int>(lines->size());
                found = true;
                s = dataString(0x29C2) + dataString(0x29D7);
            } else {
                s = std::to_string(k) + dataString(k == 1 ? 0x29C4 : 0x29E7) + dataString(0x29CE);
            }
            lines->push_back(s + std::to_string(v));
        }
        if (count) *count = static_cast<int>(lines->size());
    }
    return completionShare_[shots] * bonus / 0x80;
}

void Science::roomEnd() {
    // f38_020f (event 9 runs it for the last table when the next is
    // another table but room 1, or a lesson after a room that set +F71):
    // once (+F3B); a warp code's points (+F7F); a bonus ball for each new
    // 10000 points ([29BA]); then with a ball and a bonus or a ball to give
    // (+F83): "Bonus Points", the list by the shots (f24_057d, the balls
    // over it), a key or click, the bonus counted in 100 at a time (the
    // player's line flashing, f24_1829), 7 ticks each unless Escape is
    // held, then the balls to the right tube (sound 601F, at most 4).
    // After a lesson ([171C], set by every lesson's end) the same without
    // the box or the balls' sound; the warp code's points before it came
    // silently (f06_0208 neither sounds nor shows the score then).
    if (roomEnded_) return;
    roomEnded_ = true;
    if (hasBall_ && levelBonus_) addScore(levelBonus_);
    const long tens = totalScore_ > 9999 ? totalScore_ / 10000 : 0;
    if (tens > ballsAwarded_) bonusBalls_ = 1, ballsAwarded_ = tens;
    if (!hasBall_ || (completionBonus_ == 0 && bonusBalls_ == 0)) return;
    if (shots_ == 0) shots_ = 1;
    std::vector<std::string> lines;
    int count = 0, highlight = 5;
    const long total = bonusLines(0, &lines, &count, &highlight);
    if (total == 0 && bonusBalls_ == 0) return;
    auto pause = [&] {
        if (!escapePressed()) dialogWait(7);
    };
    auto showScore = [&] {
        // f06_0208's redraw of the room's score box (+F25).
        const Bitmap& box = ctx_.bitmap(0x1425);
        redrawTable({480, 8, box.width, box.height});
    };
    const bool quiet = quietScore_;
    Dialog d;
    if (!quiet) {
        const Rect& v = table_.view;
        d.centreX = v.x + (v.w >> 1), d.centreY = v.y + (v.h >> 1);
        d.face = 0, d.style = 0, d.message = 0x206;
        d.strings = lines, d.lines = count, d.highlight = highlight, d.balls = bonusBalls_;
        dialogOpen(d);
        // A key (any: [9558]) or a click.
        clearInput();
        for (;;) {
            ctx_.pump();
            if (anyInput()) break;
        }
        clearInput();
    }
    quietScore_ = false;
    long counted = 0;
    auto add = [&](long points) {
        counted += points;
        if (!quiet) {
            bonusLines(counted, &lines, nullptr, nullptr);
            if (highlight < 5) dialogLineAgain(d, lines[static_cast<size_t>(highlight)]);
        }
        addScore(points);
        showScore();
    };
    for (long i = 0; i < total / 100; ++i) {
        add(100);
        pause();
    }
    if (total % 100) add(total % 100);
    for (int i = 0; i < bonusBalls_; ++i) {
        pause();
        if (!quiet) {
            sound(0x601F);
            dialogLineAgain(d, dataString(0x29C2));
        }
        if (rightBalls_ < 4) ++rightBalls_;
        drawColumn(true);
    }
    pause();
    if (!quiet) dialogClose(d);
}

}  // namespace edison

namespace edison {

void Science::roomFaceMet(Ball& b, const Face& f) {
    // The room's +24 (f27_2657, nothing, in the base room), from the body
    // step (f08_1a42) when a body lands or rolls onto another face, with
    // the face. Most rooms that have one break the player's ball (its
    // core's +2A 1, and the room's +F77) on the box under a point
    // (f12_4284: water, lava, a hot plate): the face's box that one.
    auto on = [&](int x, int y) { return f.box == faceUnder(x, y).box; };
    if (currentRoom_ == 2) {
        // f41_0b78: the player's Iron ball (its +52 28DE) on the top (the
        // face's +2: 1) of the circuit, the box under (400, 325): on (+FC0;
        // the first time, +FC2, colour cycle 4A-4C every 6 ticks); else off
        // (stopped, f32_0f77). (The cycle only shows on a 256-colour
        // display.)
        const int was = roomVar_[kFC0];
        roomVar_[kFC0] = &b == &ball_ && hasBall_ && on(400, 325) && f.type == 1 && b.kind == 3;
        if (roomVar_[kFC0] && !roomVar_[kFC2]) roomVar_[kFC2] = 1, cycleStart(0x4A, 0x4C, 6);
        else if (!roomVar_[kFC0]) cycleStop(0x4A);
        if (was != roomVar_[kFC0] && std::getenv("SCI_DEBUG")) logLine(std::string("room 2's circuit ") + (was ? "off" : "on") + " at t" + std::to_string(timerTicks_));
        return;
    }
    if (currentRoom_ == 10) {
        // f42_0f2a: while +FC0, an Iron body (its +52 28DE) onto the box
        // under (420, 248) with its sphere's x 392 or more: +FC0 off, the
        // cycle at 4A stopped (f32_0f77), the gate's blocks (+FB4, +FB6)
        // put at 0, 0 (f08_056e: stopped, sound 6026 for a move of more
        // than 15), sound 601A.
        if (!roomVar_[kFC0] || !on(420, 248) || b.cx < 392 || b.kind != 3) return;
        roomVar_[kFC0] = 0;
        cycleStop(0x4A);
        for (int k = 0; k < 2; ++k)
            if (roomObj_[k] >= 0) putBody(table_.objects[static_cast<size_t>(roomObj_[k])].body, 0, 0);
        sound(0x601A);
        if (std::getenv("SCI_DEBUG")) logLine("room 10's gate opened at t" + std::to_string(timerTicks_));
        return;
    }
    if (currentRoom_ == 96) {
        // f60_0216: a body but the ball (the room has one) into the pit
        // under (315, 160): counted (+FC0) and put at 0, 0 (f08_056e: first
        // stopped, its +40, f08_0721: velocity, remainders and kick 0;
        // sound 6026 for a move of more than 15); four in, the hole to 66
        // opened (f28_153f: +2F clear, redrawn).
        if (!on(315, 160)) return;
        if (hasBall_ && &b != &ball_) {
            ++roomVar_[kFC0];
            putBody(b, 0, 0);
        }
        if (roomVar_[kFC0] >= 4)
            if (Object* h = holeTo(66)) h->closed = false, viewDirty_ = true;
        return;
    }
    if (currentRoom_ == 55) {
        // f51_1777: a body onto the box under (295, 388) or (636, 386): the
        // magnetic ball +FB4 sets +FA2, +FB6 +FA4 (both: the hole to 3
        // lets the ball through), the player's ball breaks, any other body
        // is put at 0, 0 (f08_056e).
        if (!on(295, 388) && !on(636, 386)) return;
        auto own = [&](int k) { return roomObj_[k] >= 0 && &b == &table_.objects[static_cast<size_t>(roomObj_[k])].body; };
        if (own(0)) roomVar_[kFA2] = 1;
        else if (own(1)) roomVar_[kFA4] = 1;
        else if (&b == &ball_ && hasBall_) b.state = 1;
        else putBody(b, 0, 0);
        if (std::getenv("SCI_DEBUG")) logLine("room 55's +FA2 " + std::to_string(roomVar_[kFA2]) + " +FA4 " + std::to_string(roomVar_[kFA4]) + " at t" + std::to_string(timerTicks_));
        return;
    }
    if (currentRoom_ == 60 || currentRoom_ == 64 || currentRoom_ == 69) {
        // f52_1046 (room 60), f53_0602 (64), f54_07fa (69): the bins. Any
        // body: +F7F cleared; the player's ball on a bin's box, its points
        // there; points set, event 9 to the next (64, 69; 69 to lesson
        // 508 with +F71 set, the room's end first).
        struct Bin { int x, y; long points; };
        static const Bin k60[4] = {{684, 188, 400}, {684, 126, 600}, {684, 73, 800}, {684, 14, 200}};
        static const Bin k64[4] = {{684, 191, 100}, {684, 132, 300}, {684, 70, 500}, {684, 22, 700}};
        static const Bin k69[4] = {{293, 163, 200}, {293, 112, 400}, {293, 61, 600}, {293, 11, 200}};
        const Bin* bins = currentRoom_ == 60 ? k60 : currentRoom_ == 64 ? k64 : k69;
        levelBonus_ = 0;
        if (&b == &ball_ && hasBall_)
            for (int i = 0; i < 4; ++i)
                if (on(bins[i].x, bins[i].y)) levelBonus_ = bins[i].points;
        if (levelBonus_ == 0) return;
        if (currentRoom_ == 69) roomEndFlag_ = true;
        exitNextTick_ = currentRoom_ == 60 ? 64 : currentRoom_ == 64 ? 69 : 508;
        return;
    }
    if (&b != &ball_ || !hasBall_) return;
    if (currentRoom_ == 91 && on(524, 208)) {
        // f59_014a: the goal (as its hole's method 8): a bonus ball, 2000,
        // the completion's shares (f27_0859: 2 at 20h, 3 on at 0) and event
        // 9 to room 14.
        bonusBalls_ = 1, completionBonus_ = 2000, completionShare_[2] = 0x20;
        std::fill(completionShare_ + 3, completionShare_ + 6, uint8_t{0});
        exitNextTick_ = 14;
        return;
    }
    // (f43_0136 and its copies: one point; rooms 37 and 74, f48_0879 and
    // f55_0722, two; f41_1548, room 5: four. Rooms 14-50's: f43_0c31 (14),
    // f44_0758 (17), f47_05b3 (33), f48_058d (36), f48_0b0f (38), f49_01d7
    // (41), f50_0f88 (49), f50_11ff (50).)
    // (Rooms 51, 66 and 92: f51_016b, f54_015c, f59_0788.)
    static const std::vector<std::pair<int, std::vector<std::pair<int, int>>>> kBreaking = {
        {5, {{730, 150}, {354, 0}, {424, 35}, {494, 0}}},
        {11, {{430, 0}}}, {14, {{306, 160}}}, {17, {{715, 625}}}, {33, {{405, 278}}},
        {36, {{301, 360}}}, {37, {{650, 210}, {346, 153}}}, {38, {{700, 300}}}, {41, {{481, 417}}}, {49, {{326, 301}}},
        {50, {{405, 278}}}, {51, {{501, 47}}}, {66, {{487, 160}}}, {74, {{369, 140}, {608, 112}}}, {91, {{439, 123}}}, {92, {{512, 245}}},
    };
    for (const auto& [room, points] : kBreaking) {
        if (room != currentRoom_) continue;
        for (const auto& [x, y] : points)
            if (on(x, y)) {
                b.state = 1;
                if (std::getenv("SCI_DEBUG")) logLine("room " + std::to_string(room) + " breaks the ball on its box at t" + std::to_string(timerTicks_));
                return;
            }
    }
}

void Science::roomObjects(int room) {
    // What a room's builder makes itself (its helper after f27_0ad8, read
    // with tools/testing/calltrace.py): looks on its boxes (f34_0000,
    // f12_0000, f12_0872 on the box under a point) and objects, after the
    // file's (so last in the list).
    struct FaceLook { int face; uint16_t id; int x, y; };
    struct BoxLook { int room, x, y; std::vector<FaceLook> faces; };
    // (All five faces: one picture, the rest 117B.)
    auto all = [](int face, uint16_t id, int x, int y) {
        std::vector<FaceLook> f;
        for (int k = 1; k <= 5; ++k) f.push_back(k == face ? FaceLook{k, id, x, y} : FaceLook{k, 0x117B, 0, 0});
        return f;
    };
    const std::vector<BoxLook> kLooks = {
        {5, 354, 0, {{5, 0x117B, 0, 0}}}, {5, 494, 0, {{5, 0x117B, 0, 0}}},          // f41_1278
        {10, 420, 248, {{3, 0x117B, 0, 0}, {2, 0x117B, 0, 0}}},                     // f42_0baf
        {13, 716, 0, all(3, 0x117B, 490, 42)},                                      // f43_0838: the glass wall
        {18, 392, 227, all(5, 0x10DD, 246, 128)}, {18, 428, 232, all(3, 0x10DD, 246, 128)},
        {18, 392, 232, all(2, 0x10DD, 246, 128)},                                   // f44_0a97
        {37, 346, 153, {{3, 0x117B, 0, 0}}},                                        // f48_07ae
        {40, 464, 0, {{5, 0x117B, 0, 0}}},                                          // f48_105b
        {42, 541, 0, all(3, 0x10A8, 314, 6)},                                       // f49_0408
        {45, 439, 120, all(5, 0x10AF, 256, 124)}, {45, 554, 120, all(3, 0x10B0, 372, 16)},
        {45, 424, 120, all(2, 0x10AE, 244, 16)},                                    // f49_0ab2
        {52, 745, 0, all(3, 0x10CE, 520, 136)},                                     // f51_0365
        {53, 346, 153, {{3, 0x117B, 0, 0}}},                                        // f51_06a3
        {55, 704, 2, all(3, 0x10D0, 478, 120)}, {55, 269, 2, all(5, 0x10CF, 54, 220)},  // f51_1152
        {92, 478, 180, all(5, 0x10E1, 316, 158)},                                   // f59_05e2
    };
    for (const BoxLook& l : kLooks) {
        if (l.room != room) continue;
        Box* box = const_cast<Box*>(faceUnder(l.x, l.y).box);
        if (!box) continue;
        for (const FaceLook& f : l.faces) box->looks[f.face] = {f.id, f.x, f.y};
    }
    std::fill(std::begin(roomObj_), std::end(roomObj_), -1);
    auto add = [&](int x, int y, int type, std::initializer_list<int> args) {
        Object o;
        o.x = x, o.y = y, o.type = type;
        int k = 0;
        for (int a : args) o.args[k++] = a;
        table_.objects.push_back(o);
        return static_cast<int>(table_.objects.size()) - 1;
    };
    if (room == 10) {
        // f42_0baf: a hole to 12 at (627, 330) on the back wall (+FBC); the
        // gate, two blocks (f07_17f2: 17, 115E, not dragged nor stepped;
        // their +34 60) at (607, 309) and (636, 309) (+FB4, +FB6); three
        // loose Rubber balls (f07_0000, radius 10). The circuit on (+FC0)
        // and colour cycle 4A-4C every 6 ticks (f32_0e7f).
        add(627, 330, 8, {12, 1, 0, -1});
        roomObj_[0] = add(607, 309, 16, {17, 0x115E, 0, 0, 60});
        roomObj_[1] = add(636, 309, 16, {17, 0x115E, 0, 0, 60});
        add(435, 226, 0, {10}), add(498, 226, 0, {10}), add(465, 226, 0, {10});
        roomVar_[kFC0] = 1;
    }
    if (room == 12) {
        // f43_0386: a magnet on the wall (f05_26f7, type 6, with OBJ1's
        // arguments: 13, -1, 1, 10) at (362, 329) (its power part +FA8),
        // switched off (its +0C), the colour cycle at 4A stopped. Its tick
        // powers it (roomTick).
        roomObj_[3] = add(362, 329, 6, {13, -1, 1, 10});
    }
    if (room == 54) {
        // f51_09ce: a bullseye (f04_0502: f04_03eb with a 0, b -1; its
        // method 3 only sets it, f04_059d: no power, no sound) at (363, 400)
        // (+FA8); two magnets that don't move (f05_210d, type 5: 13, S
        // +FB8, then N +FBA) at (506, 400), S put 80 up (f08_056e with a
        // height), N on the ground at (505, 401). Its tick works them
        // (roomTick).
        roomObj_[3] = add(363, 400, 7, {0, -1, 0, 1});
        roomObj_[2] = add(506, 400, 5, {13, -1});
        roomObj_[4] = add(506, 400, 5, {13, 1});
    }
    if (room == 18) {
        // f44_0a97: a loose magnet (f05_1749: 10, N) at (478, 252) (+FB8).
        // f44_0846: a gate, a small hole to 1000 on the ground at (668,
        // 262), over the hole to 60 (f28_00f3 with b 1, c 0, d -1; its
        // +FBC), shut on arrival (roomArrival).
        roomObj_[2] = add(478, 252, 4, {10, 1});
        add(668, 262, 8, {1000, 1, 0, -1});
    }
    if (room == 55) {
        // f51_1152: two magnetic balls (f05_11c1, type 2's: radius 10) at
        // (403, 223) and (550, 248) (+FB4, +FB6).
        roomObj_[0] = add(403, 223, 2, {10});
        roomObj_[1] = add(550, 248, 2, {10});
    }
    if (room == 2) {
        // f41_076c: the gate, a small hole to 1000 on the back wall at (606,
        // 329) on the ground (f28_00f3 with b 1, c 0, d -1; its +FBA), shut
        // on arrival (roomArrival).
        Object gate;
        gate.x = 606, gate.y = 329, gate.type = 8;
        gate.args[0] = 1000, gate.args[1] = 1, gate.args[2] = 0, gate.args[3] = -1;
        table_.objects.push_back(gate);
    }
}

void Science::cycleStart(int first, int last, int period) {
    // f32_0e7f (f32_00e3: first, last - first + 1 colours, kept within
    // 256): one already there whose colours meet these (f32_0150: the two
    // spans, f11_0a26) gets the new period and one more use; else a new
    // one with one use.
    const int count = std::min(last - first + 1, 0x100 - first);
    for (Cycle& c : cycles_)
        if (c.first + c.count - 1 >= first && first + count - 1 >= c.first) {
            c.period = period, ++c.uses;
            return;
        }
    Cycle c;
    c.first = first, c.count = count, c.period = period, c.uses = 1;
    cycles_.push_back(c);
}

void Science::cycleStop(int first) {
    // f32_0f77: one use fewer; none left, it goes.
    for (size_t i = 0; i < cycles_.size(); ++i)
        if (cycles_[i].first == first) {
            if (--cycles_[i].uses == 0) cycles_.erase(cycles_.begin() + static_cast<std::ptrdiff_t>(i));
            return;
        }
}

void Science::cycleStep() {
    // f32_1113, from event 4 (f32_11a5): only on a 256-colour display
    // ([61F9]); each cycle whose period divides [27B4] turned a step
    // (f14_0148: each colour takes the next one's, the last the first's).
    static const bool trueColour = std::getenv("SCI_TRUECOLOR") != nullptr;
    if (trueColour) return;
    Palette& p = ctx_.displayPalette;
    for (const Cycle& c : cycles_) {
        if (c.period <= 0 || gameTicks_ % c.period != 0) continue;
        const Rgb keep = p[static_cast<size_t>(c.first)];
        for (int i = c.first; i < c.first + c.count - 1; ++i) p[static_cast<size_t>(i)] = p[static_cast<size_t>(i + 1)];
        p[static_cast<size_t>(c.first + c.count - 1)] = keep;
    }
}

void Science::roomCycles(int room) {
    // The colour cycles as the room is built (the last room's objects and
    // the room gone, their destructors having stopped theirs): each
    // magnetic part (f05_0003: types 2-6 and 15, the type 3 ball) 90-97
    // and 98-9F every 3 ticks; a type 6 magnet (f05_26f7) 4A-4C every 4;
    // a pulling hole (f28_15a1) A0-A6 every 3; a hot field (f02_1234)
    // 4D-4F every 10; then the room's constructor's own, after its
    // builder (segments 41-60; room 12's builder, f43_0386, stops its two
    // type 6 magnets' 4A).
    // (The room destructors that stop their own with f32_0f77, rooms 2-50:
    // f41_08f0 (2), f41_1029 (4), f41_122c (5), f42_078d (9), f42_0b59
    // (10), f43_0b52 (14), f43_0df2 (15), f44_0141 (16), f44_06e7 (17),
    // f44_0a41 (18), f44_16b5 (20), f45_012f (21), f45_0837 (22), f45_0ae4
    // (23), f46_00fc (26), f46_0485 (28), f46_0698 (29), f46_08bb (30),
    // f47_00cf (31), f47_0541 (33), f47_090d (34), f48_0176 (36), f48_0757
    // (37), f48_0a9e (38), f50_00fa (46), f50_0436 (47), f50_0f16 (49),
    // f50_116e (50): all gone here.)
    cycles_.clear();
    for (const Object& o : table_.objects) {
        if (o.type == 2 || o.type == 3 || o.type == 4 || o.type == 5 || o.type == 6 || o.type == 15)
            cycleStart(0x90, 0x97, 3), cycleStart(0x98, 0x9F, 3);
        if (o.type == 6) cycleStart(0x4A, 0x4C, 4);
        if (o.type == 9) cycleStart(0xA0, 0xA6, 3);
        if (o.type == 14) cycleStart(0x4D, 0x4F, 10);
    }
    struct RoomCycle { int room, first, last, period; };
    static const RoomCycle kRooms[] = {
        {4, 0xC2, 0xC7, 5}, {5, 0x4D, 0x4F, 6}, {10, 0x4A, 0x4C, 6}, {14, 0x4D, 0x4F, 6}, {14, 0xB0, 0xB6, 6},
        {15, 0x4D, 0x4F, 6}, {16, 0x4A, 0x4C, 6}, {17, 0x4D, 0x4F, 6}, {20, 0x4D, 0x4F, 6}, {20, 0xB0, 0xB6, 5},
        {21, 0xB0, 0xB6, 5}, {22, 0xB0, 0xB6, 5}, {23, 0xB0, 0xB6, 5}, {26, 0xB0, 0xB6, 5}, {28, 0x4D, 0x4F, 6},
        {29, 0xB0, 0xB6, 5}, {30, 0x4D, 0x4F, 6}, {30, 0xB0, 0xB6, 3}, {30, 0xC2, 0xC7, 5}, {31, 0xB0, 0xB6, 5},
        {33, 0xC2, 0xC7, 3}, {34, 0xB0, 0xB6, 5}, {36, 0xC2, 0xC7, 3}, {37, 0x4D, 0x4F, 6}, {37, 0xB0, 0xB6, 5},
        {38, 0x4D, 0x4F, 6}, {46, 0x4D, 0x4F, 6}, {47, 0x4D, 0x4F, 6}, {49, 0xC2, 0xC7, 3}, {50, 0xC2, 0xC7, 3},
        {51, 0x4D, 0x4F, 6}, {55, 0x4D, 0x4F, 6}, {57, 0x4D, 0x4F, 6}, {63, 0x4D, 0x4F, 6}, {66, 0x4D, 0x4F, 6},
        {72, 0x4D, 0x4F, 6}, {74, 0x4D, 0x4F, 6}, {77, 0xB0, 0xB6, 4}, {91, 0x4D, 0x4F, 6}, {92, 0x4A, 0x4C, 4},
        {92, 0x4D, 0x4F, 6}, {96, 0x4D, 0x4F, 6}, {97, 0xC2, 0xC7, 3}, {99, 0x4D, 0x4F, 6}, {99, 0xA0, 0xA6, 3},
        {99, 0xB0, 0xB6, 4},
    };
    if (room == 12) cycleStop(0x4A), cycleStop(0x4A);
    for (const RoomCycle& c : kRooms)
        if (c.room == room) cycleStart(c.first, c.last, c.period);
}

void Science::roomTick() {
    // The room's +14 (f27_2434 in the base room: the objects' ticks, which
    // tickRoom runs after this).
    if (currentRoom_ == 9) {
        // f42_0805: on 15 game ticks in 16, switch 99 (f27_0a5a) on and
        // +FC0 clear: colour cycle 4A-4C every 6 ticks, +FC0 set; off: the
        // cycle stopped, +FC0 clear. (Without switch 99 the base tick
        // would be skipped; the room has it.)
        if ((gameTicks_ & 0xF) == 0) return;
        for (const Object& o : table_.objects)
            if (o.type == 7 && o.args[5] == 99) {
                if (o.state && !roomVar_[kFC0]) cycleStart(0x4A, 0x4C, 6), roomVar_[kFC0] = 1;
                else if (!o.state) cycleStop(0x4A), roomVar_[kFC0] = 0;
                break;
            }
        return;
    }
    if (currentRoom_ == 34) {
        // f47_0cbf: its sign blinking: when [27B4] mod 64 is 0, 1092 on
        // screen 3 at (392, 20), at 31 1093 (redrawn, 56 x 70).
        const int phase = gameTicks_ & 0x3F;
        if (phase == 0 || phase == 0x1F) {
            select(3);
            drawLogo(392, 20, static_cast<uint16_t>(0x1092 + (phase & 1)));
            queueArea({392, 20, 56, 70});
        }
        return;
    }
    if (currentRoom_ == 18) {
        // f44_11a7: the loose magnet (+FB8) falling (its +66 below 0) with
        // its box's centre below 62 and within (397-427, 232-264), its
        // slot, before (+FC0, +FC4 clear): +FC0 and +FC4 set, +FC2 0,
        // colour cycle 4D-4F every 3 ticks, sound 6029. While +FC0, below
        // 50, +FC2 one more: the gate (+FBC) to (677, 278) at that height
        // (f08_056e), the hole to 60 shut and hidden. At 50, the hole to 60
        // opened and shown, +FC0 clear, the cycle stopped.
        if (std::getenv("SCI_DEBUG"))
            if (const Object* g = holeTo(1000)) {
                // (As the original's memory has them: +FC0, +FC2, +FC4, the
                // gate's box's corner.)
                const int z = g->liftZ != Object::kNoLift ? g->liftZ : heightUnder(g->x, g->y);
                logLine("room18 t" + std::to_string(timerTicks_) + " " + std::to_string(roomVar_[kFC0]) + " " + std::to_string(roomVar_[kFC2]) + " " +
                        std::to_string(roomVar_[kFC4]) + " " + std::to_string(g->x) + " " + std::to_string(g->y) + " " + std::to_string(z));
            }
        if (roomObj_[2] >= 0 && !roomVar_[kFC0] && !roomVar_[kFC4]) {
            const Object& m = table_.objects[static_cast<size_t>(roomObj_[2])];
            int b[6];
            thingBox(m, b);
            const int x = b[0] + (b[3] >> 1), y = b[1] + (b[4] >> 1), z = b[2] + (b[5] >> 1) - 1;
            if (m.body.v[2] < 0 && z < 62 && x >= 397 && x <= 427 && y >= 232 && y <= 264) {
                roomVar_[kFC0] = roomVar_[kFC4] = 1, roomVar_[kFC2] = 0;
                cycleStart(0x4D, 0x4F, 3);
                sound(0x6029);
                if (std::getenv("SCI_DEBUG")) logLine("room 18's magnet in its slot at t" + std::to_string(timerTicks_));
            }
        }
        Object* h = holeTo(60);
        if (roomVar_[kFC0] && roomVar_[kFC2] < 50) {
            ++roomVar_[kFC2];
            if (Object* g = holeTo(1000)) {
                // (As room 2's gate: its box an 11 radius round the point.)
                g->x = 677 - 11, g->y = 278 - 11, g->liftZ = roomVar_[kFC2] - 6;
                viewDirty_ = true;
            }
            if (h) closeHole(*h), h->holeHidden = true;
        }
        if (roomVar_[kFC2] == 50) {
            if (h && (h->closed || h->holeHidden)) {
                h->closed = false, h->holeHidden = false, viewDirty_ = true;
                if (std::getenv("SCI_DEBUG")) logLine("room 18's hole to 60 opened at t" + std::to_string(timerTicks_));
            }
            roomVar_[kFC0] = 0;
            cycleStop(0x4D);
        }
        return;
    }
    if (currentRoom_ == 54 && roomObj_[3] >= 0) {
        // f51_0dc2: while the bullseye (+FA8) is on, every third game tick
        // ([27B4]): going down (+FC2 clear), +FC0 one more; past 29, +FC4
        // and +FC2 set and the bullseye off (its +0C: set only); with
        // +FC4, the N magnet (+FBA) at (505, 401) 16 under the S one.
        // Going up, +FC0 one less; below 1, +FC2 clear and the bullseye
        // off; the N magnet 16 under the S one. Then 10B3 + (+FC0 odd) on
        // screen 3 at (422, 38) (redrawn, 10 x 74), and the S magnet
        // (+FB8) at (506, 400), 80 - 2 +FC0 up.
        Object& sw = table_.objects[static_cast<size_t>(roomObj_[3])];
        if (std::getenv("SCI_DEBUG")) {
            // (As the original's memory has them: the bullseye's +4, +FC0,
            // +FC2, +FC4, the magnets' boxes' corners, +FB8's and +FBA's.)
            int s[6], n[6];
            thingBox(table_.objects[static_cast<size_t>(roomObj_[2])], s);
            thingBox(table_.objects[static_cast<size_t>(roomObj_[4])], n);
            logLine("room54 t" + std::to_string(timerTicks_) + " " + std::to_string(sw.state) + " " + std::to_string(roomVar_[kFC0]) + " " +
                    std::to_string(roomVar_[kFC2]) + " " + std::to_string(roomVar_[kFC4]) + " " + std::to_string(s[0]) + " " + std::to_string(s[1]) + " " +
                    std::to_string(s[2]) + " " + std::to_string(n[0]) + " " + std::to_string(n[1]) + " " + std::to_string(n[2]));
        }
        if (!sw.state || gameTicks_ % 3 != 0) return;
        Ball& s = table_.objects[static_cast<size_t>(roomObj_[2])].body;
        Ball& n = table_.objects[static_cast<size_t>(roomObj_[4])].body;
        int& count = roomVar_[kFC0];
        if (!roomVar_[kFC2]) {
            if (++count > 29) {
                roomVar_[kFC4] = roomVar_[kFC2] = 1;
                switchSet(sw, 0);
            }
            if (roomVar_[kFC4]) putBodyAt(n, 505, 401, 80 - 2 * count - 16);
        } else {
            if (--count < 1) {
                roomVar_[kFC2] = 0;
                switchSet(sw, 0);
            }
            putBodyAt(n, 505, 401, 80 - 2 * count - 16);
        }
        select(3);
        drawLogo(422, 38, static_cast<uint16_t>(0x10B3 + count % 2));
        queueArea({422, 38, 10, 74});
        putBodyAt(s, 506, 400, 80 - 2 * count);
        if (std::getenv("SCI_DEBUG") && !sw.state) logLine("room 54's magnets stop at " + std::to_string(count) + " at t" + std::to_string(timerTicks_));
        return;
    }
    if (currentRoom_ == 12 && roomObj_[3] >= 0) {
        // f43_043d: on 5 game ticks in 6 ([27B4] not a multiple of 6), +FC0
        // set while the switches 1-4 (f27_0a5a: the first type 7 whose +2,
        // its last argument, is that) are all on (+4). All on and the
        // magnet (+FA8) off: on (its +0C: sound 6027), +FC2, colour cycle
        // 4A-4C every 6 ticks, the lights 10D9 on screen 3 at (371, 108)
        // and (470, 108), redrawn (f38_0494, 28 x 28). Else, the magnet on:
        // off, and with +FC2 (cleared) the cycle at 4A stopped and the
        // lights 10D8.
        if (gameTicks_ % 6 == 0) return;
        roomVar_[kFC0] = 1;
        for (int id = 1; id < 5; ++id)
            for (const Object& o : table_.objects)
                if (o.type == 7 && o.args[5] == id) {
                    if (o.state == 0) roomVar_[kFC0] = 0;
                    break;
                }
        Object& m = table_.objects[static_cast<size_t>(roomObj_[3])];
        auto lights = [&](uint16_t id) {
            select(3);
            drawLogo(371, 108, id), drawLogo(470, 108, id);
            queueArea({371, 108, 28, 28}), queueArea({470, 108, 28, 28});
            if (std::getenv("SCI_DEBUG")) logLine(std::string("room 12's magnet ") + (m.powered ? "on" : "off") + " at t" + std::to_string(timerTicks_));
        };
        if (!roomVar_[kFC0]) {
            if (!m.powered) return;
            m.powered = false, viewDirty_ = true;
            if (!roomVar_[kFC2]) return;
            roomVar_[kFC2] = 0;
            cycleStop(0x4A);
            lights(0x10D8);
        } else if (!m.powered) {
            sound(0x6027);
            m.powered = true, roomVar_[kFC2] = 1;
            cycleStart(0x4A, 0x4C, 6);
            lights(0x10D9);
        }
        return;
    }
    if (currentRoom_ == 2) {
        // f41_0c21: with the circuit on (+FC0), every third game tick
        // ([27B4]): at 24, the hole to 1001 (+FBC) still shut, sound 6029,
        // shown (+1C) and opened (f28_153f); below 40, the gate (+FBA) one
        // higher (+FBE): f08_056e to (616, 345) at that height (its sphere's
        // centre its radius higher, its box round it, f07_11c5).
        if (!roomVar_[kFC0] || gameTicks_ % 3 != 0) return;
        if (roomVar_[kFBE] == 24)
            if (Object* h = holeTo(1001); h && h->closed) {
                sound(0x6029);
                h->holeHidden = false, h->closed = false;
                if (std::getenv("SCI_DEBUG")) logLine("room 2's hole to 1001 opened at t" + std::to_string(timerTicks_));
                viewDirty_ = true;
            }
        if (roomVar_[kFBE] < 40) {
            ++roomVar_[kFBE];
            if (Object* g = holeTo(1000)) {
                // (Read in the original's memory: its box then a 23 cube at
                // (605, 334, +FBE - 6).)
                g->x = 605, g->y = 334, g->liftZ = roomVar_[kFBE] - 6;
                viewDirty_ = true;
            }
        }
    }
}

void Science::putBody(Ball& b, int x, int y) {
    // f08_056e (height 0): the body stopped (its +40, f08_0721: velocity,
    // remainders and kick 0), sound 6026 for a move of more than 15, and
    // put on the ground at (x, y).
    for (int k = 0; k < 3; ++k) b.v[k] = 0, b.rem[k] = 0, b.kick[k] = 0;
    b.kickTicks = 0;
    const int dx = b.cx - x, dy = b.cy - y;
    if (dx * dx + dy * dy > 15 * 15) sound(0x6026);
    b.cx = x, b.cy = y, b.cz = heightUnder(x, y) + b.r;
    b.onGround = true;
    viewDirty_ = true;
}

void Science::putBodyAt(Ball& b, int x, int y, int h) {
    // f08_056e with a height (its fourth argument 1): stopped, its sphere's
    // centre at (x, y) its radius above h (its +3C), no sound; +5E set.
    for (int k = 0; k < 3; ++k) b.v[k] = 0, b.rem[k] = 0, b.kick[k] = 0;
    b.kickTicks = 0;
    b.cx = x, b.cy = y, b.cz = h + b.r;
    b.onGround = true;
    viewDirty_ = true;
}

Science::Object* Science::holeTo(int room) {
    // f27_09dc: the room's object whose class says 10 (its +48: f28_062f,
    // a door or a pulling hole) and whose +C is `room`.
    for (Object& o : table_.objects)
        if (isHole(o) && o.args[0] == room) return &o;
    return nullptr;
}

void Science::roomArrival(int room) {
    // The builders' own ends (segments 41-60), after the room's pictures:
    // a greeting box on a black screen (f24_1ee3: screen 2's play area,
    // f31_0385, 40 taller, in colour 2; [275C] is set while event 9 builds
    // a room, so its closing
    // redraws nothing), some only when coming from a given room (the
    // player's +90) or not from the same one; doors met on arrival (the
    // ball coming out of the one it came through); holes shut by the
    // game's flags.
    const int from = previousRoom_;
    roomCycles(room);
    std::vector<std::tuple<int, int, uint16_t, uint16_t>> boxes;  // face, style, text, sound
    auto shut = [&](int to) {
        if (Object* h = holeTo(to)) closeHole(*h);
    };
    switch (room) {
    case 2:
        // f41_076c: the gate (+FBA, roomObjects) shut; the hole to 1001
        // (+FBC) shut and hidden (its +18, f08_0469).
        if (Object* g = holeTo(1000)) closeHole(*g);
        if (Object* h = holeTo(1001)) closeHole(*h), h->holeHidden = true;
        break;
    case 3: if (from == 55) boxes = {{1, 1, 0x36, 0x6135}}; break;  // f41_0ce1
    case 6: case 7: boxes = {{1, 1, 0x33, 0x6132}}; break;          // f42_0000, f42_021c
    case 9: roomVar_[3] = 0, boxes = {{2, 0, 0x28, 0x6127}}; break;  // f42_0641
    case 10: if (from != 10) boxes = {{2, 0, 0x2C, 0x612B}}; break;  // f42_09be
    case 13: if (from == 7) boxes = {{1, 1, 0x37, 0x6136}}; break;  // f43_06a1
    case 16: boxes = {{2, 0, 0x2E, 0x612D}}; break;                 // f44_0000
    case 18:
        // f44_0846: the hole to 60 shut and hidden (+18), the gate (+FBC)
        // shut; the greeting unless coming from room 18.
        if (Object* h = holeTo(60)) closeHole(*h), h->holeHidden = true;
        if (Object* g = holeTo(1000)) closeHole(*g);
        if (from != 18) boxes = {{2, 0, 0x31, 0x6130}};
        break;
    case 22:
        // f45_0722: in through the door from 40, that door shut.
        if (from == 40) shut(40);
        break;
    case 23:
        // f45_0955: in through the door from 44: +FA6 set, the ball out of
        // the hole to 44 (its +2C, mode 1), the doors to 44, 109 and 110
        // shut. (Else f24_1ee3 alone: screen 2's play area in colour 2,
        // all redrawn before it's seen.)
        if (from == 44) {
            roomVar_[kFA6] = 1;
            if (Object* h = holeTo(44)) spitBall(*h, 1);
            shut(44), shut(109), shut(110);
        }
        break;
    case 32: if (from != 32) boxes = {{2, 0, 0x3F, 0x613D}}; break;  // f47_01d8
    case 33:
        // f47_044a: in through the door from 52, that door shut.
        if (from == 52) shut(52);
        break;
    case 34:
        // f47_06db
        if (from == 22 || from == 40) {
            // In through a door: the ball out of the hole to 117, the doors
            // to 22 and 40 shut.
            roomVar_[1] = roomVar_[2] = 1;
            if (Object* h = holeTo(117)) spitBall(*h, 0);
            shut(22), shut(40);
        } else {
            boxes = {{2, 0, 0xC, 0x610C}};
        }
        break;
    case 35:
        if (from == 43) {
            // f47_0d46: in through the door from 43: the ball hidden and
            // spat out of the hole to 1000 (its +2C, mode 2), the hole to
            // 43 shut, 2 shots (+F39, its box redrawn), +FC0 set.
            if (Object* h = holeTo(1000)) spitBall(*h, 2);
            shut(43);
            shots_ = 2, roomVar_[kFC0] = 1;
            viewDirty_ = true;
        } else {
            roomVar_[kFC0] = 0;
            shut(1000);
            boxes = {{2, 0, 3, 0x6103}, {2, 0, 0x5E8, 0x6160}};
        }
        break;
    case 36: if (from != 36) boxes = {{1, 1, 0xF, 0x610F}}; break;  // f48_0000
    case 40:
        // f48_0f14: in through the door from 22, that door shut.
        if (from == 22) shut(22);
        break;
    case 41: boxes = {{2, 0, 0x41, 0x613F}}; break;  // f49_0000
    case 47:
        // f50_0202
        if (from == 50) gameFlag_[0] = gameFlag_[1] = 0, boxes = {{2, 0, 0x15, 0x6115}, {2, 0, 0x5F7, 0x616F}};
        break;
    case 50:
        // f50_108b: f24_1ee3 alone (screen 2's play area in colour 2, all
        // redrawn before it's seen), no box.
        break;
    case 54:
        // f51_09ce: its magnets put in place (once made: roomObjects).
        if (roomObj_[2] >= 0) putBodyAt(table_.objects[static_cast<size_t>(roomObj_[2])].body, 506, 400, 80);
        if (roomObj_[4] >= 0) putBody(table_.objects[static_cast<size_t>(roomObj_[4])].body, 505, 401);
        if (from == 47) boxes = {{2, 0, 0x16, 0x6116}};
        break;
    case 55: boxes = {{2, 0, 0x20, 0x611F}}; break;
    case 59: boxes = {{1, 1, 0x38, 0x6137}}; break;
    case 67: boxes = {{2, 0, 0x43, 0x6141}, {2, 0, 0x44, 0x6142}}; break;
    case 70: if (from != 70) boxes = {{2, 0, 0x46, 0x6144}}; break;
    case 92: boxes = {{2, 0, 0x2B, 0x612A}}; break;
    case 96: shut(66), boxes = {{2, 0, 0x40, 0x613E}}; break;
    default: break;
    }
    if (!boxes.empty()) {
        // (Event 9 has faded the old room out: the display is black.)
        std::fill(ctx_.screens[1].pixels.begin(), ctx_.screens[1].pixels.end(), uint8_t{2});
        std::fill(ctx_.screens[2].pixels.begin(), ctx_.screens[2].pixels.end(), uint8_t{2});
        dialogNoRedraw_ = true;
        for (const auto& [face, style, text, sound] : boxes) say(face, style, text, sound);
        dialogNoRedraw_ = false;
    }
    // After the greeting (room 47's and 54's builders): holes shut by the
    // game's flags ([8E50] the hole to 33 in 47, 100 and 101 in 54;
    // [8E52] 23 in 47; [8E54] 48 in 54).
    if (room == 47) {
        if (gameFlag_[0]) shut(33);
        if (gameFlag_[1]) shut(23);
    } else if (room == 54) {
        if (gameFlag_[2]) shut(48);
        if (gameFlag_[0]) shut(100), shut(101);
    }
}

}  // namespace edison
