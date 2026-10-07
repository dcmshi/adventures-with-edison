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
enum { kFA2, kFA4, kFA6, kFC0, kFC4, kFBE };

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
    return dialogRun(d);
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
    return d.text;
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
        if (to == 0xC46) bonusBalls_ = 1, bonus(1500), completionShare_[3] = 0, to = 10;
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
    case 18:  // f44_0f60: the magnet ([FC4], room 18's tick: not ported)
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
            // Try again: the hole to 1001 told to start over (its method
            // 3, f27_0a5a: room 18's objects aren't ported), the ball out.
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
                // Start the screen again: no shots, the counters off, two of
                // its objects back in place (+FB4, +FB6: room 55's own, not
                // ported).
                if (completionBonus_ < 0) bonus(0);
                shots_ = 0, viewDirty_ = true;
                roomVar_[kFA2] = roomVar_[kFA4] = 0;
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
            bonusBalls_ = 1, bonus(2000), completionShare_[2] = 0x20, completionShare_[3] = 0, to = 14;
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
    case 92:  // f59_07df
        if (to == 0xC46) bonusBalls_ = 1, bonus(1500), completionShare_[2] = 0x2A, completionShare_[3] = 0, to = 93;
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
        // A key or a click.
        clearInput();
        int x, y;
        while (ctx_.platform.takeClick(&x, &y)) {}
        for (;;) {
            ctx_.pump();
            if (ctx_.platform.takeClick(&x, &y) || ctx_.platform.takeKey() != 0) break;
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
        for (int k = 0; k < 2; ++k) {
            if (roomObj_[k] < 0) continue;
            Ball& g = table_.objects[static_cast<size_t>(roomObj_[k])].body;
            for (int j = 0; j < 3; ++j) g.v[j] = 0, g.rem[j] = 0, g.kick[j] = 0;
            g.kickTicks = 0;
            if (g.cx * g.cx + g.cy * g.cy > 15 * 15) sound(0x6026);
            g.cx = 0, g.cy = 0, g.cz = heightUnder(0, 0) + g.r;
        }
        sound(0x601A);
        viewDirty_ = true;
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
            for (int k = 0; k < 3; ++k) b.v[k] = 0, b.rem[k] = 0, b.kick[k] = 0;
            b.kickTicks = 0;
            if (b.cx * b.cx + b.cy * b.cy > 15 * 15) sound(0x6026);
            b.cx = 0, b.cy = 0, b.cz = heightUnder(0, 0) + b.r;
            viewDirty_ = true;
        }
        if (roomVar_[kFC0] >= 4)
            if (Object* h = holeTo(66)) h->closed = false, viewDirty_ = true;
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
        // the completion's shares (f27_0859: 2 at 20h, 3 at 0) and event 9
        // to room 14.
        bonusBalls_ = 1, completionBonus_ = 2000, completionShare_[2] = 0x20, completionShare_[3] = 0;
        exitNextTick_ = 14;
        return;
    }
    // (f43_0136 and its copies: one point; rooms 37 and 74, f48_0879 and
    // f55_0722, two; f41_1548, room 5: four.)
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
    if (room == 18) {
        // f44_0a97: a loose magnet (f05_1749: 10, N) at (478, 252) (+FB8).
        roomObj_[2] = add(478, 252, 4, {10, 1});
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

void Science::roomTick() {
    // The room's +14 (f27_2434 in the base room: the objects' ticks, which
    // tickRoom runs after this).
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

Science::Object* Science::holeTo(int room) {
    // f27_09dc: the room's object (a hole) whose +C is `room`.
    for (Object& o : table_.objects)
        if (o.type == 8 && o.args[0] == room) return &o;
    return nullptr;
}

void Science::roomArrival(int room) {
    // The builders' own ends (segments 41-60), after the room's pictures:
    // a greeting box on a black screen (f24_1ee3: screen 2's play area in
    // colour 2; [275C] is set while event 9 builds a room, so its closing
    // redraws nothing), some only when coming from a given room (the
    // player's +90) or not from the same one; doors met on arrival (the
    // ball coming out of the one it came through); holes shut by the
    // game's flags.
    const int from = previousRoom_;
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
    case 3: if (from == 55) boxes = {{1, 1, 0x36, 0x6135}}; break;
    case 6: case 7: boxes = {{1, 1, 0x33, 0x6132}}; break;
    case 9: roomVar_[3] = 0, boxes = {{2, 0, 0x28, 0x6127}}; break;
    case 10: if (from != 10) boxes = {{2, 0, 0x2C, 0x612B}}; break;
    case 13: if (from == 7) boxes = {{1, 1, 0x37, 0x6136}}; break;
    case 16: boxes = {{2, 0, 0x2E, 0x612D}}; break;
    case 18: if (from != 18) boxes = {{2, 0, 0x31, 0x6130}}; break;
    case 32: if (from != 32) boxes = {{2, 0, 0x3F, 0x613D}}; break;
    case 34:
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
            // In through the door from 43: the ball out of it, 2 shots.
            if (Object* h = holeTo(43)) spitBall(*h, 2), closeHole(*h);
            shots_ = 2, roomVar_[3] = 1;
        } else {
            roomVar_[3] = 0;
            shut(1000);
            boxes = {{2, 0, 3, 0x6103}, {2, 0, 0x5E8, 0x6160}};
        }
        break;
    case 36: if (from != 36) boxes = {{1, 1, 0xF, 0x610F}}; break;
    case 41: boxes = {{2, 0, 0x41, 0x613F}}; break;
    case 47:
        if (from == 50) gameFlag_[0] = gameFlag_[1] = 0, boxes = {{2, 0, 0x15, 0x6115}, {2, 0, 0x5F7, 0x616F}};
        break;
    case 54: if (from == 47) boxes = {{2, 0, 0x16, 0x6116}}; break;
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
