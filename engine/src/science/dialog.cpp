// WMAIN.EXE segment 24: the dialog boxes over the table (room 1's EXIT
// question and warp codes; rooms' messages). See docs/SCIENCE.md.

#include <algorithm>
#include <cstdlib>
#include <cstring>

#include "science/science.h"

namespace edison {

namespace {

// [1F1E]: the face's frames as it talks (f24_1d43 steps through them;
// from a 1 it jumps to 1 or 3 at random).
constexpr int kMouth[7] = {1, 0, 1, 2, 3, 2, 1};
constexpr int kTalkFrame = 2;  // [1F24] (kMouth[3])

// f24_1b15's ctype test (Borland's _ctype + 1: 2 digits, 4 capitals, 8
// small letters) or a space.
bool typeable(int c) {
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == ' ';
}

}  // namespace

Science::Rect Science::dialogFrame(const Dialog& d) const {
    // f24_07e1: the framed style's frame (22 out from the picture, 328 x
    // 236), else the picture.
    if (d.style == 0) return {d.rect.x - 22, d.rect.y - 22, 0x148, 0xEC};
    return d.rect;
}

Science::Rect Science::dialogLine(const Dialog& d, int i) const {
    // f24_08a9: button i, 28 apart from +2C.
    return {d.rect.x + 0x17, d.linesTop + i * 0x1C, 0xDF, 0x17};
}

void Science::dialogButton(const Rect& r, const std::string& s, bool down) {
    // f24_0cc7: the button (1398 down, 1396 up), its text 4 right and 5
    // down in colour F (f14_191d).
    panelSprite(r.x, r.y, down ? 0x1398 : 0x1396);
    textAt(r.x + 4, r.y + 5, s, 0x0F);
}

void Science::dialogFace(Dialog& d, int frame) {
    // f24_0d57: sprite 13A3 + face (1 at least) x 4 + the frame.
    panelSprite(d.faceRect.x, d.faceRect.y, static_cast<uint16_t>(0x13A3 + std::max(d.face, 1) * 4 + frame));
}

void Science::dialogOpen(Dialog& d) {
    // f24_0e8c, on screen 2 (the clip the whole screen).
    clearPolygonClip();
    d.closed = false;
    select(2);
    // The picture: a random one of 1359-135D (DS:1E7C, ten entries), or
    // 1393 for style 1 (DS:1EA4); centred on the point.
    const int pick = static_cast<int>(static_cast<long>(borlandRand()) * 10 / 0x8000);
    uint16_t picture = d.style == 0 ? static_cast<uint16_t>(0x1359 + pick % 5) : 0x1393;
    // (Testing: SCI_DIALOGPIC=k picks 1359 + k, as the original's rand may have.)
    if (const char* k = std::getenv("SCI_DIALOGPIC"); k && d.style == 0) picture = static_cast<uint16_t>(0x1359 + std::atoi(k) % 5);
    if (!d.strings.empty() && !d.strings[0].empty()) picture = 0x135E;
    const Bitmap& pic = ctx_.bitmap(picture);
    d.rect = {d.centreX - pic.width / 2, d.centreY - pic.height / 2, pic.width, pic.height};
    panelSprite(d.rect.x, d.rect.y, picture);
    // f24_0a34: the stand under the frame (1394, centred under its foot;
    // drawn unless screen 2's pixel there reads 2, which GetPixel's RGB never
    // does) and for the framed style the frame: top 138F and left 1392 at
    // its corner, bottom 1390 and right 1391 against its far edges.
    const Rect f = dialogFrame(d);
    const int cx = f.x + (f.w >> 1), bottom = f.y + f.h - 1, right = f.x + f.w - 1;
    const Bitmap& stand = ctx_.bitmap(0x1394);
    d.stand = {cx - stand.width / 2, bottom, stand.width, stand.height};
    objectSprite(cx, bottom + stand.height / 2, 0x1394);
    if (d.style == 0) {
        panelSprite(f.x, f.y, 0x138F);
        panelSprite(f.x, f.y, 0x1392);
        panelSprite(f.x, bottom - ctx_.bitmap(0x1390).height + 1, 0x1390);
        panelSprite(right - ctx_.bitmap(0x1391).width + 1, f.y, 0x1391);
    }
    // f24_01be's picture in the box (f24_06bf: low nibble 1 centred, 2
    // left, else right; high 10h centred, 20h top, else bottom).
    if (d.picture) {
        const Bitmap& p = ctx_.bitmap(d.picture);
        const int h = d.align & 0x0F, v = d.align & 0xF0;
        const int px = h == 1 ? (d.rect.w - p.width) / 2 + d.rect.x : h == 2 ? d.rect.x : d.rect.x + d.rect.w - p.width;
        const int py = v == 0x10 ? (d.rect.h - p.height) / 2 + d.rect.y : v == 0x20 ? d.rect.y : d.rect.y + d.rect.h - p.height;
        panelSprite(px, py, d.picture);
    }
    // The buttons' rows: up from the picture's foot, 28 each and 10 more
    // (not for the framed style).
    const int foot = d.rect.y + d.rect.h - 1;
    d.linesTop = foot - d.lines * 0x1C - 10 + (d.style == 0 ? 10 : 0);
    // The message (f24_095e's rectangle: 246 wide, down to the buttons),
    // wrapped by segment 23 from its first estimate, (w x w) / w here (the
    // base class's extent counts characters); a line only if its corner is
    // in the rectangle, colour F (f22_0325).
    const Rect m{d.rect.x + (d.style == 0 ? 0x0D : 0x17), d.rect.y + (d.style == 0 ? 8 : 0x1C), 0xF6, 0};
    const int mh = d.linesTop - m.y;
    {
        const std::string text = textResource(static_cast<uint16_t>(0x7000 + d.message));
        int y = m.y;
        for (const std::string& line : wrapText(text, m.w, (m.w * m.w & 0xFFFF) / m.w)) {
            if (y >= m.y && y < m.y + mh) textAt(m.x, y, line, 0x0F);
            y += font_->height();
        }
    }
    if (d.lines != 0) {
        if (d.single) {
            // One field: typed (empty: DS:1F30), else what was typed or
            // "OK..." (DS:1F3D).
            const std::string s = d.typed ? std::string() : d.text.empty() ? dataString(0x1F3D) : d.text;
            dialogButton(dialogLine(d, 0), s, false);
        } else if (!d.strings.empty() && !d.strings[0].empty()) {
            // A list (f24_057d): for each line the balls over the first
            // (f24_175a, drawn again each time), then the line 4 right and
            // 5 down in colour F, or as a button (1396, colour 18) when
            // it's the highlighted one.
            for (int i = 0; i < d.lines; ++i) {
                dialogBalls(d);
                if (i >= 5) continue;
                int colour = 0x0F;
                const Rect r = dialogLine(d, i);
                if (d.highlight == i && d.highlight < 5) {
                    colour = 0x18;
                    panelSprite(r.x, r.y, 0x1396);
                }
                if (static_cast<size_t>(i) < d.strings.size()) textAt(r.x + 4, r.y + 5, d.strings[static_cast<size_t>(i)], colour);
            }
        } else {
            // Each button with its line centred (segment 23, alignment 1),
            // the first line only, 4 lower (f22_0325, colour F).
            for (int i = 0; i < d.lines; ++i) {
                const Rect r = dialogLine(d, i);
                panelSprite(r.x, r.y, 0x1396);
                const std::string text = textResource(static_cast<uint16_t>(0x7000 + d.firstLine + i));
                const std::vector<std::string> lines = wrapText(text, r.w, (r.w * r.w & 0xFFFF) / r.w);
                if (lines.empty()) continue;
                const int w = font_->width(lines[0]);
                textAt(r.x + (r.w >> 1) - w / 2, r.y + 4, lines[0], 0x0F);
            }
        }
    }
    // Edison's face at the picture's foot (its frame 2's size), screen 2
    // under it kept (+38), then frame 2.
    const Bitmap& fb = ctx_.bitmap(static_cast<uint16_t>(0x13A3 + std::max(d.face, 1) * 4 + 2));
    int fy = foot - fb.height + (d.style != 0 ? -4 : d.face == 2 ? 2 : 8);
    int fx = (d.rect.w - fb.width) * 5 / 8 + d.rect.x;
    if (d.face == 2) fx += 0x32 + (d.style == 0 ? 4 : 0);
    else if (d.face == 1 && d.style == 0) fx += 0x1A;
    else fx += 10;
    d.faceRect = {fx, fy, fb.width, fb.height};
    d.underFace.clear();
    const Screen& two = ctx_.screens[2];
    for (int y = fy; y < fy + fb.height; ++y)
        for (int x = fx; x < fx + fb.width; ++x)
            d.underFace.push_back(x >= 0 && y >= 0 && x < Screen::kWidth && y < Screen::kHeight ? two.pixels[static_cast<size_t>(y) * Screen::kWidth + x] : 0);
    if (d.face) dialogFace(d, kTalkFrame);
    copyArea(2, 1, f.x, f.y, f.w, f.h);
}

void Science::dialogWait(int ticks) {
    // f32_07aa: a busy wait on the 50 Hz counter (no messages handled:
    // clicks and keys stay queued).
    const uint64_t end = ctx_.platform.milliseconds() + static_cast<uint64_t>(ticks) * 20;
    while (ctx_.platform.milliseconds() < end) ctx_.pump();
}

void Science::dialogTick(Dialog& d) {
    // f24_1d43: 3 ticks; every sixth call the face's next frame (screen 2
    // under it put back first).
    dialogWait(3);
    if (++faceTicks_ % 6 != 0 || d.face == 0) return;
    select(2);
    Screen& two = ctx_.screens[2];
    size_t k = 0;
    for (int y = d.faceRect.y; y < d.faceRect.y + d.faceRect.h; ++y)
        for (int x = d.faceRect.x; x < d.faceRect.x + d.faceRect.w; ++x, ++k)
            if (x >= 0 && y >= 0 && x < Screen::kWidth && y < Screen::kHeight) two.pixels[static_cast<size_t>(y) * Screen::kWidth + x] = d.underFace[k];
    if (d.mouth == 4 && d.face == 1) dialogFace(d, kTalkFrame);
    dialogFace(d, kMouth[d.mouth]);
    copyArea(2, 1, d.faceRect.x, d.faceRect.y, d.faceRect.w, d.faceRect.h);
    d.mouthShown = d.mouth;
    if (kMouth[d.mouth] == 1) d.mouth = static_cast<long>(borlandRand()) * 3 / 0x8000 == 0 ? 1 : 3;
    else if (++d.mouth > 6) d.mouth = 0;
}

void Science::dialogPress(Dialog& d, int i) {
    // f24_0d89: sound 6025, button i down with "ZAP!" (DS:1F38); the face
    // again over it if they meet; to the display.
    sound(0x6025);
    select(2);
    const Rect r = dialogLine(d, i);
    dialogButton(r, dataString(0x1F38), true);
    const Rect both = intersect(r, d.faceRect);
    if (both.w > 0 && both.h > 0 && d.face != 0) {
        if (d.mouthShown == 4) dialogFace(d, kTalkFrame);
        dialogFace(d, kMouth[d.mouthShown]);
    }
    copyArea(2, 1, r.x, r.y, r.w, r.h);
}

int Science::dialogRun(Dialog& d) {
    // f24_193e. The loops handle one Windows message a pass (f36_0000's
    // PeekMessage) and wait 3 ticks without handling any (f24_1d43), so a
    // press is seen on the pass after it's handled and a release on the
    // pass after that; the buttons are tried one a pass, in turn, so a
    // click only counts when it's on the one being tried then ([6EC4] is
    // the button held, [6EC0] / [6EC2] where it went down).
    d.mouth = d.mouthShown = 0;
    clearInput();
    int x, y;
    while (ctx_.platform.takeClick(&x, &y)) {}
    bool held = false, pressed = false;  // [6EC4], [6EC5]
    int key = 0;                         // [9558] / [955C]
    int mx = 0, my = 0;
    auto message = [&] {
        // f36_0000: one message.
        bool down;
        ctx_.platform.mouse(&x, &y, &down);
        if (held && !down) {
            held = false;
            return;
        }
        int cx, cy;
        if (ctx_.platform.takeClick(&cx, &cy)) {
            held = true, pressed = true, mx = cx, my = cy;
            return;
        }
        if (const int k = ctx_.platform.takeKey()) key = k;
    };
    if (!d.typed) {
        if (d.lines == 0) {
            while (!key && !pressed) {
                message();
                dialogTick(d);
            }
        } else {
            int i = 0;
            bool done = false;
            for (;;) {
                if (key) break;
                if (std::getenv("SCI_DEBUG")) logLine("dialog pass " + std::to_string(i) + (held ? " held" : ""));
                if (held) {
                    if (inside(dialogLine(d, i), mx, my)) {
                        dialogPress(d, i);
                        dialogWait(0x1E);
                        d.result = i;
                        done = true;
                        break;
                    }
                    if (d.single) {
                        dialogPress(d, 0);
                        dialogWait(0x1E);
                        d.result = 0;
                        done = true;
                        break;
                    }
                }
                if (++i >= d.lines) i = 0;
                message();
                dialogTick(d);
            }
            if (!done) {
                // A key: y (or Y with several buttons) is the second, any
                // other the first.
                d.result = key == 'y' || (key == 'Y' && !d.single) ? 1 : 0;
                dialogPress(d, d.result);
                dialogWait(0x1E);
            }
        }
    } else {
        // f24_1b15: typing into the field, up to 64 characters (letters,
        // digits, spaces); backspace or left takes the last away; Enter
        // ends. The field and a caret (8 wide, the font's height less 4,
        // after the text) blinking every 8 passes (colours F and 56), the
        // face again over the field; no wait between passes.
        select(2);
        d.text.clear();
        for (unsigned pass = 1;; ++pass) {
            key = 0;
            message();
            if (key == Platform::kEnter) break;
            if (key == Platform::kBackspace || key == Platform::kLeft) {
                if (!d.text.empty()) d.text.pop_back();
            } else if (key > 0 && key < 0x100 && typeable(key) && d.text.size() < 0x40) {
                d.text += static_cast<char>(key);
            }
            select(2);
            const Rect r = dialogLine(d, 0);
            dialogButton(r, d.text, false);
            fill(r.x + 4 + font_->width(d.text), r.y + 5, 8, font_->height() - 4, (pass & 0xF) < 8 ? 0x0F : 0x56);
            const Rect both = intersect(r, d.faceRect);
            if (both.w > 0 && both.h > 0 && d.face != 0) dialogFace(d, kTalkFrame);
            copyArea(2, 1, r.x, r.y, r.w, r.h);
            // (The original's passes run as fast as the machine; here one
            // a millisecond.)
            const uint64_t until = ctx_.platform.milliseconds() + 1;
            while (ctx_.platform.milliseconds() < until) ctx_.pump();
        }
    }
    clearInput();
    while (ctx_.platform.takeClick(&x, &y)) {}
    dialogClose(d);
    return d.result;
}

void Science::dialogClose(Dialog& d) {
    // f24_1f26: event 5 to the player (its method 4, f31_27de) for the
    // frame and for the stand: the room's redraw there (method 3) and
    // every control's. (Screen 2 under the face is then put back, under
    // what was redrawn: nothing shows it.)
    if (d.closed) return;
    d.closed = true;
    if (dialogNoRedraw_) return;  // [275C]: while event 9 builds a room
    for (const Rect& r : {dialogFrame(d), d.stand}) {
        redrawTable(r);
        drawPanel();
        drawColumn(false);
        drawColumn(true);
    }
}

Science::Rect Science::dialogBalls(Dialog& d) {
    // f24_175a: +1C8 balls in random rolling frames (101C-1021), 30
    // apart, 25 above the first line; its rectangle so moved.
    Rect r = dialogLine(d, 0);
    r.y -= 0x19;
    for (int i = 0, x = r.x; i < d.balls; ++i, x += 0x1E)
        panelSprite(x, r.y, static_cast<uint16_t>(0x101C + static_cast<long>(borlandRand()) * 6 / 0x8000));
    return r;
}

void Science::dialogLineAgain(Dialog& d, const std::string& s) {
    // f24_1829: for a list, the balls again (to the display), and the
    // highlighted line as a button with s in colours 16-18 in turn
    // ([1F2C]).
    select(2);
    if (d.strings.empty() || d.strings[0].empty()) return;
    const Rect b = dialogBalls(d);
    copyArea(2, 1, b.x, b.y, b.w, b.h);
    if (d.highlight >= 5) return;
    const Rect r = dialogLine(d, d.highlight);
    panelSprite(r.x, r.y, 0x1396);
    textAt(r.x + 4, r.y + 5, s, static_cast<int>(flashCount_++ % 3 + 0x16));
    copyArea(2, 1, r.x, r.y, r.w, r.h);
}

}  // namespace edison
