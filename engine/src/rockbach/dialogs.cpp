// WINMAIN's segments 22 and 23: the file dialogs (LOAD: CD or hard drive,
// then a list of files; a name box for SAVE; message boxes) and their
// scroll bar. They're drawn on screen 2 and copied to the display, and
// poll their own widget lists.

#include <algorithm>
#include "formats/paths.h"
#include <cctype>
#include <filesystem>

#include "rockbach/rockbach.h"

namespace edison {
namespace {

std::string upper(std::string s) {
    for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

}  // namespace

// The dialog widgets' face: a 1 x 1 blank; the pictures are the overlays.
RockBach::Widget RockBach::dialogButton(int x, int y, uint16_t pressed, uint16_t up, int w, int h) {
    Widget b{x, y, x + w + 3, y + h + 3, 0x104};
    b.bitmapDown = b.bitmapUp = 0x2309;
    b.overlayDown = pressed;
    b.overlayUp = up;
    b.mode = 'c';
    b.hotkey = 0;
    return b;
}

void RockBach::dialogColours() {
    // f27_0054: the 16 EGA colours' nearest entries in screen 2's palette
    // (the table is B, G, R, compared byte for byte with the palette's
    // B, G, R: seg 63's entries are RGBQUAD order, f43_0186). The bytes
    // are signed chars (cbw), so 0x80-0xFF count as negative: as the
    // original, which makes EGA blue a bright green on some backdrops.
    auto s = [](uint8_t v) { return static_cast<long>(static_cast<int8_t>(v)); };
    for (int i = 0; i < 16; ++i) {
        const uint8_t* t = &data_[0x2630 + 3 * i];
        int best = 0;
        long bestD = -1;
        for (int k = 0; k < 256; ++k) {
            const Rgb& p = ctx_.screens[2].palette[k];
            const long db = s(t[0]) - s(p.b), dg = s(t[1]) - s(p.g), dr = s(t[2]) - s(p.r);
            const long d = db * db + dg * dg + dr * dr;
            if (bestD < 0 || d < bestD) bestD = d, best = k;
        }
        dialogColour_[i] = static_cast<uint8_t>(best);
    }
}

void RockBach::bevelBox(int x, int y, int w, int h, bool pressed) {
    // f23_0ab8 (deep): a box with three-pixel edges.
    const uint8_t face = pressed ? dialogColour_[2] : dialogColour_[7];
    const uint8_t top = pressed ? dialogColour_[8] : dialogColour_[5], bottom = pressed ? dialogColour_[6] : dialogColour_[9];
    const uint8_t right = pressed ? dialogColour_[9] : dialogColour_[6], left = pressed ? dialogColour_[5] : dialogColour_[8];
    // Each edge's three lines in turn (top, bottom, right, left), so the
    // sides cut across the corners; the left one reaches a row further.
    fill(x, y, w, h, face);
    for (int k = 0; k < 3; ++k) line(x, y + k, x + w - 1, y + k, top);
    for (int k = 0; k < 3; ++k) line(x, y + h - 1 - k, x + w - 1, y + h - 1 - k, bottom);
    for (int k = 0; k < 3; ++k) line(x + w - 1 - k, y + 1 + k, x + w - 1 - k, y + h - 2 - k, right);
    for (int k = 0; k < 3; ++k) line(x + k, y + 1 + k, x + k, y + h - 1 - k, left);
}

std::vector<std::string> RockBach::listFiles(const std::string& dir, const std::string& ext) {
    // f22_0000 / f22_00ba: the names (without their extensions) of dir's
    // *ext files, sorted.
    std::vector<std::string> names;
    std::error_code ec;
    for (const auto& e : std::filesystem::directory_iterator(findPath(dir), ec)) {
        if (!e.is_regular_file()) continue;
        const std::string file = e.path().filename().string();
        const auto dot = file.find('.');
        if (dot == std::string::npos || upper(file.substr(dot)) != upper(ext)) continue;
        names.push_back(file.substr(0, dot));
        if (names.size() >= 0x1FF) break;
    }
    std::sort(names.begin(), names.end());  // qsort with f22_0094 (strcmp)
    return names;
}

bool RockBach::fileList(int x, int y, const std::string& dir, bool cd, const std::string& ext, std::string* out,
                        int kind, const std::string& title, bool saveBackground) {
    // f22_1864: 12 slots (3 rows of 4) of the files' names under an icon
    // (waveform 22FC for sounds, video 22FD), a scroll bar, CANCEL. A letter
    // jumps to the first name from it. 1 chosen (out: the path), 0 cancelled.
    const int previous = current();
    std::vector<Widget>* oldWidgets = widgets_;
    clearInput();
    const Bitmap& panel = ctx_.bitmap(0x22FE);
    const int pw = panel.width, ph = panel.height;
    select(1);
    const int saved = saveBackground ? saveArea(x, y, pw, ph) : 0;
    const std::vector<std::string> names = listFiles(dir, ext);
    const int count = static_cast<int>(names.size()), slots = std::min(12, count);
    const uint16_t icon = kind ? 0x22FC : 0x22FD;
    const Bitmap& ib = ctx_.bitmap(icon);
    std::vector<Widget> w;  // f22_0664
    w.push_back(dialogButton(x + 0xFB, y + 0x2E, 0x22FA, 0x22FB, ctx_.bitmap(0x22FA).width, ctx_.bitmap(0x22FB).height));
    for (int n = 0; n < 12; ++n) {
        Widget s = dialogButton(x + 0x20 + (n % 4) * (ib.width + 8), y + 0x66 + (n / 4) * (ib.height + 0xF), icon, icon,
                                ib.width, ib.height);
        if (n >= slots) s.flags |= Widget::kHidden;
        w.push_back(s);
    }
    widgets_ = &w;
    // The scroll bar (f22_087c), right of the slots: f23_0000 clears the
    // bars, f23_007a adds this one (its arrows and track), f23_0546 puts
    // the thumb at top * (track - thumb) / (count - 12).
    const int bx = w[4].x1 + 10, by = w[4].y0, bh = w[12].y1 - w[4].y0;
    const int trackY = by + 0xC, trackH = bh - 2 * 0xC, thumbH = 0xC, page = 0xC;
    int top = 0;
    auto thumbY = [&] {
        const int range = count - page;
        return range > 0 ? trackY + std::min(top, range) * (trackH - thumbH) / range : trackY;
    };
    auto drawBar = [&](bool upPressed, bool downPressed) {  // f23_02c0
        select(1);
        drawLogo(bx - 2, by, upPressed ? 0x230B : 0x230A);
        drawLogo(bx - 2, by + bh - 0xC, downPressed ? 0x230D : 0x230C);
        select(2);
        fill(bx, trackY, 0x10, trackH, 0x40);
        bevelBox(bx, thumbY(), 0x10, thumbH, false);
        copyArea(2, 1, bx, trackY, 0x10, trackH);
        select(1);
    };
    auto draw = [&] {  // f22_0360
        const int tw = font_->width(title);
        select(2);
        dialogColours();
        drawLogo(x, y, 0x22FE);
        font_->draw(ctx_.screens[2], x + (pw - tw) / 2, y + 0xC, title, dialogColour_[6]);
        font_->draw(ctx_.screens[2], x + (pw - tw) / 2 - 1, y + 0xB, title, dialogColour_[1]);
        drawLogo(w[0].x0 + 2, w[0].y0 + 2, 0x22FA);  // f22_049e
        for (int i = 0; i < slots && top + i < count; ++i) {
            const Widget& s = w[1 + i];
            drawOpaque(s.x0 + 2, s.y0 + 2, icon);
            const std::string name = upper(names[top + i]);
            font_->draw(ctx_.screens[2], s.x0 + (ib.width - font_->width(name)) / 2, s.y0 + ib.height - font_->height(),
                        name, 0xFF);
        }
        drawLogo(x + 0x32, y + 0x20, cd ? 0x22F8 : 0x22F9);
        copyKeyed(2, 1, x, y, pw, ph);  // f37_0320: colour 0 (the icons' shadows) shows what was there
        drawBar(false, false);
        select(1);
    };
    draw();
    bool chosen = true;
    for (bool done = false; !done;) {
        if (ctx_.platform.escapeHeld()) break;  // (leaves "chosen" with the path unchanged, as the original)
        const int r = pollWidgets();
        if (r >= 0 && r <= slots) {
            done = true;
            if (r == 0) chosen = false;
            else *out = dir + names[top + r - 1] + ext;
        }
        if (lastKey_ > 0 && lastKey_ < 0x80 && std::isalpha(lastKey_) && count > 0) {
            // f22_02c0: the first name from that letter (else the last name's letter).
            const char c = static_cast<char>(std::toupper(lastKey_));
            int i = 0;
            while (i < count && static_cast<unsigned char>(upper(names[i])[0]) < static_cast<unsigned char>(c)) ++i;
            if (i == count) i = count - 1;
            top = std::clamp(i, 0, std::max(0, count - page));
            draw();
        }
        if (lastClick_.on && lastClick_.x >= bx - 2 && lastClick_.x < bx + 0x10 && lastClick_.y >= by && lastClick_.y < by + bh) {
            // f23_0768: the arrows step a row (4 names) while held; the thumb drags.
            // (f23_0184: the bar clicked, f23_0222: which part; f23_06e8: the
            // step, kept within 0 to count - 12.)
            const bool up = lastClick_.y < by + 0xC, down = lastClick_.y >= by + bh - 0xC;
            const int range = std::max(0, count - page);
            for (bool held = true; held;) {
                const int was = top;
                int mx, my;
                ctx_.platform.mouse(&mx, &my, &held);
                if (up) top = std::max(0, top - 4);
                else if (down) top = std::min(range, top + 4);
                else if (range > 0) top = std::clamp((my - trackY - thumbH / 2) * range / (trackH - thumbH), 0, range);
                if (top != was) draw();
                drawBar(up && held, down && held);
                if (!up && !down) {
                    ctx_.pump();
                } else {
                    waitCountdown(1);
                }
            }
        }
    }
    if (saveBackground) restoreArea(saved);
    select(previous);
    clearInput();
    widgets_ = oldWidgets;
    return chosen;
}

bool RockBach::loadDialog(int x, int y, const std::string& ext, std::string* out, int kind, const std::string& title) {
    // f22_1454: CD ROM (the CD's \RB\EFFECTS\), HARD DRIVE (the game's
    // folder) or CANCEL; then that one's list. The choice is kept for the
    // next LOAD of the same kind, which goes straight to its list.
    const int previous = current();
    int choice = kind == 1 ? loadSource_[1] : loadSource_[0];
    select(1);
    clearInput();
    const Bitmap& panel = ctx_.bitmap(0x22FE);
    const int pw = panel.width, ph = panel.height, tw = font_->width(title);
    std::vector<uint8_t> saved(static_cast<size_t>(pw) * ph);  // f37_1bb6: the display
    for (int r = 0; r < ph; ++r)
        for (int c = 0; c < pw; ++c)
            if (x + c < Screen::kWidth && y + r < Screen::kHeight)
                saved[static_cast<size_t>(r) * pw + c] = ctx_.screens[1].pixels[static_cast<size_t>(y + r) * Screen::kWidth + x + c];
    dialogColours();
    const Bitmap& cdb = ctx_.bitmap(0x22F8);
    // (The two sources' rectangles take x for their y: as in the original,
    // whose pictures are drawn there too.)
    const int rects[3][4] = {{x + 0xFC, y + 0x2E, ctx_.bitmap(0x22FA).width + 3, ctx_.bitmap(0x22FA).height + 3},
                             {x + 0x32, x, cdb.width, cdb.height},
                             {x + 0x32 + cdb.width + 0x10, x, cdb.width, cdb.height}};
    bool picked = false, redraw = true;
    for (bool done = false; !done && !ctx_.platform.escapeHeld();) {
        if (redraw) {
            select(2);
            drawLogo(x, y, 0x22FE);
            font_->draw(ctx_.screens[2], x + (pw - tw) / 2, y + 0x14, title, dialogColour_[6]);
            font_->draw(ctx_.screens[2], x + (pw - tw) / 2 - 1, y + 0x13, title, dialogColour_[1]);
            static const uint16_t kPictures[3] = {0x22FA, 0x22F8, 0x22F9};
            for (int i = 0; i < 3; ++i) ctx_.screens.drawSprite(2, ctx_.bitmap(kPictures[i]), rects[i][0], rects[i][1]);
            copyKeyed(2, 1, x, y, pw, ph);  // f37_0320
            select(1);
            redraw = false;
        }
        ctx_.pump();
        int cx, cy;
        if (ctx_.platform.takeClick(&cx, &cy))
            for (int i = 0; i < 3; ++i)
                if (cx > rects[i][0] && cy > rects[i][1] && cx <= rects[i][0] + rects[i][2] && cy <= rects[i][1] + rects[i][3])
                    choice = i;
        if (choice == 1 || choice == 2) {
            const std::string dir = choice == 1 ? cdRoot_ + "/RB/EFFECTS/" : options_.saveDir + "/";
            picked = fileList(x, y, dir, choice == 1, ext, out, kind, title, false);
            if (picked) done = true;
            else redraw = true;
        } else if (choice == 0) {
            done = true;
        }
        if (done && choice > 0) loadSource_[kind == 1 ? 1 : 0] = choice;
        choice = -1;
    }
    select(1);
    for (int r = 0; r < ph; ++r)  // f37_1664
        for (int c = 0; c < pw; ++c)
            if (x + c < Screen::kWidth && y + r < Screen::kHeight)
                ctx_.screens[1].pixels[static_cast<size_t>(y + r) * Screen::kWidth + x + c] = saved[static_cast<size_t>(r) * pw + c];
    select(previous);
    return picked;
}

bool RockBach::nameDialog(int x, int y, std::string* name, const std::string& ext, std::string* out) {
    // f22_099e: up to 8 letters and digits, a caret blinking every 0.3 s,
    // CANCEL and O.K. (or Enter). An empty name asks again.
    const int previous = current();
    std::vector<Widget>* oldWidgets = widgets_;
    clearInput();
    std::string buf = name->substr(0, 8);
    const int ex = x + 0x63, ey = y + 0x14, ew = 0x5A, eh = 0x18;
    std::vector<Widget> w;  // f22_130e
    for (int i = 0; i < 2; ++i) {
        const Bitmap& b = ctx_.bitmap(static_cast<uint16_t>(0x2300 + i));
        w.push_back(dialogButton(x + 0x22 + 0x5A * i, y + 0x9A, static_cast<uint16_t>(0x2300 + i),
                                 static_cast<uint16_t>(0x2302 + i), b.width, b.height));
    }
    widgets_ = &w;
    const Bitmap& panel = ctx_.bitmap(0x22FF);
    const int saved = saveArea(x, y, panel.width, panel.height);
    dialogColours();
    drawLogo(x, y, 0x22FF);
    for (const Widget& b : w) drawLogo(b.x0 + 2, b.y0 + 2, b.overlayUp);
    auto showName = [&] {  // f22_1164
        fill(ex, ey, ew, eh, 0);
        text(ex + 2, ey + (eh - font_->height()) / 2, buf, 0xFF);
    };
    showName();
    bool blink = false, result = false;
    ctx_.countdown[1] = 0;
    for (bool done = false; !done && !ctx_.platform.escapeHeld();) {
        const int r = pollWidgets();
        bool ok = r == 1;
        if (r == 0) break;
        if (ctx_.countdown[1] == 0) {
            text(ex + font_->width(buf) + 2, ey + (eh - font_->height()) / 2, "|", blink ? dialogColour_[9] : dialogColour_[4]);
            blink = !blink;
            ctx_.countdown[1] = 3;
        }
        // f22_11ce: a key.
        const int key = lastKey_;
        if (key == Platform::kBackspace || key == Platform::kLeft) {
            if (!buf.empty()) buf.pop_back(), showName();
        } else if (key == Platform::kEnter) {
            ok = true;
        } else if (key > 0 && key < 0x80 && std::isalnum(key)) {
            if (buf.size() >= 8) buf.pop_back();
            buf += static_cast<char>(key);
            showName();
        }
        if (ok) {
            if (buf.empty() || buf[0] == '.') {
                clearInput();
                continue;
            }
            *name = buf;
            *out = options_.saveDir + "/" + buf + ext;
            result = done = true;
        }
    }
    restoreArea(saved);
    select(previous);
    clearInput();
    widgets_ = oldWidgets;
    return result;
}

int RockBach::messageBox(int x, int y, const std::vector<std::string>& lines, const std::vector<int>& buttons) {
    // f22_0dd2: the panel (240A), the lines centred, the buttons (0 CANCEL,
    // 1 YES, 2 O.K.) spread along the bottom; the position of the one
    // pressed.
    const int previous = current();
    std::vector<Widget>* oldWidgets = widgets_;
    const Bitmap& panel = ctx_.bitmap(0x240A);
    select(1);
    const int saved = saveArea(x, y, panel.width, panel.height);
    drawLogo(x, y, 0x240A);
    const int ix = x + 0x10, iy = y + 0x10, iw = panel.width - 0x20, ih = panel.height - 0x20;
    const int n = static_cast<int>(buttons.size());
    const int bw = n ? ctx_.bitmap(static_cast<uint16_t>(0x240B + buttons[0])).width : 0;
    const int gap = (iw - n * bw) / (n + 1);
    std::vector<Widget> w;
    for (int i = 0, bx = ix + gap; i < n; ++i, bx += gap + 0x50) {
        const uint16_t id = static_cast<uint16_t>(0x240B + buttons[i]);
        w.push_back(dialogButton(bx, iy + ih - 0x28, id, id, ctx_.bitmap(id).width, ctx_.bitmap(id).height));
    }
    widgets_ = &w;
    for (const Widget& b : w) drawLogo(b.x0 + 2, b.y0 + 2, b.overlayUp);
    int ly = iy + font_->height();
    for (const std::string& l : lines) {
        const int lx = ix + ((iw - font_->width(l)) >> 1);
        text(lx, ly, l, 0x20);
        text(lx + 1, ly - 1, l, 0);
        ly += font_->height();
    }
    int r = -1;
    while (r < 0) r = pollWidgets();
    restoreArea(saved);
    select(previous);
    widgets_ = oldWidgets;
    clearInput();
    return r;
}

}  // namespace edison
