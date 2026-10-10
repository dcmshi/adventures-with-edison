// WINMAIN's segment 30 (the Music Library: eight composers, their lives
// and pieces, on a timeline), segment 21 (text from the .hi files in a box)
// and segment 12 (the pieces' player, in ADLIB4.DLL).

#include <algorithm>
#include "formats/paths.h"
#include <fstream>

#include "rockbach/rockbach.h"

#include "audio/artech_fm_driver.h"

namespace edison {
namespace {

// A .hi file as the original reads it with fgets(80): its lines.
std::vector<std::string> readLines(const std::string& path) {
    std::vector<std::string> lines;
    std::ifstream in(findPath(path), std::ios::binary);
    for (std::string line; std::getline(in, line);) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(line.substr(0, 0x4F));
    }
    return lines;
}

}  // namespace

// --- the player (segment 12) ----------------------------------------------------

void RockBach::pieceReset() {
    // f12_0000: the first piece, instrument 3, tempo C0.
    if (!options_.music) return;
    piece_ = Piece{};
    pieceTempo(0xC0);
}

void RockBach::pieceChoose(int composer, int n) {
    // f12_007a: DS:1858 (8 composers x 5 words) points at the piece's
    // record: a count of sounds, the first, and its own tempo.
    if (n >= 5) n = 0;
    piece_.record = dataWord(static_cast<uint16_t>(0x1858 + composer * 10 + n * 2));
}

void RockBach::pieceInstrument(int i) {
    // f12_00b6: the instrument's patch (DS:17D8) on each of the piece's
    // channels.
    if (!options_.music) return;
    piece_.instrument = i;
    const int count = dataWord(piece_.record);
    const uint8_t patch = data_[0x17D8 + i];
    ctx_.platform.withFm([count, patch](ArtechFmDriver& d) {
        for (int ch = 0; ch < 9 && ch < count; ++ch) d.installPatch(static_cast<uint8_t>(ch), patch);
    });
}

void RockBach::pieceStart() {
    // f12_0118: the piece's sounds (none for DS:17DC), then the tempo.
    if (!options_.music || piece_.record == 0x17DC) return;
    const int count = dataWord(piece_.record), first = data_[piece_.record + 2];
    ctx_.platform.withFm([count, first](ArtechFmDriver& d) {
        for (int i = 0; i < count; ++i) d.sendSound(static_cast<uint16_t>(first + i));
    });
    pieceTempo(piece_.tempo);
}

void RockBach::pieceStop() {
    // f12_018c: sound 0; then 0.1 s.
    if (!options_.music) return;
    ctx_.platform.sendFm(0);
    ctx_.countdown[3] = 1;
    while (ctx_.countdown[3] != 0) ctx_.pump();
}

void RockBach::pieceTempo(int t) {
    // f12_0270: t (BC-FF) scaled onto the piece's own tempo (to FF): "A6
    // tempo" through sound F.
    if (!options_.music) return;
    piece_.tempo = t;
    const int base = data_[piece_.record + 3];
    const int tempo = (t - 0xBC) * (0xFF - base) / 0x43 + base;
    const std::vector<uint8_t> bytes = {0x09, 0x00, 0xA6, static_cast<uint8_t>(tempo), 0x88};
    ctx_.platform.withFm([&bytes](ArtechFmDriver& d) {
        // f12_02f6: the bytes over the sound (f12_0330 / f12_038c: its address).
        const uint16_t to = d.soundAddress(0xF);
        for (size_t k = 0; k < bytes.size(); ++k) d.poke(static_cast<uint16_t>(to + k), bytes[k]);
        d.sendSound(0xF);
    });
}

// --- text in a box (segment 21) ----------------------------------------------------

bool RockBach::textBox(const std::vector<std::string>& lines, const TextBox& box, bool viaScreen2) {
    // f21_0000: block `box.block` of the file (blocks end with a line
    // starting '!'; "!E" ends the file), in the box: its background (a
    // bitmap, or a fill), a line every 12 pixels. When the box is full it
    // waits for a key or click (if it may) and goes on on a clean box;
    // otherwise its next line is the last.
    size_t at = 0;
    std::string line;
    auto next = [&] {
        line = at < lines.size() ? lines[at++] : std::string("!E");
        return line;
    };
    for (int count = -1; count != box.block && !(line.size() > 1 && line[1] == 'E');) {
        do next();
        while (line.empty() || line[0] != '!');
        ++count;
    }
    if (line.size() > 1 && line[0] == '!' && line[1] == 'E') return false;
    const int rows = box.h / 12;
    if (viaScreen2) select(2);
    auto background = [&] {
        if (box.bitmap) drawOpaque(box.x, box.y, box.bitmap);
        else fill(box.x, box.y, box.w, box.h, box.colour);
    };
    auto show = [&] {
        if (viaScreen2) copyArea(2, 1, box.x, box.y, box.w, box.h);
    };
    background();
    const int tx = box.tx + 2;
    Screen* s = &ctx_.screens[current()];
    int n = 0;
    for (next(); line.empty() || line[0] != '!'; next()) {
        font_->draw(*s, tx, box.ty + 12 * n, line, box.ink);
        if (++n != rows - 1) continue;
        const bool more = at < lines.size() && !lines[at].empty() && lines[at][0] != '!';
        if (more && box.paging) {
            font_->draw(*s, tx, box.ty + 12 * n, dataString(0x2168), 7);  // "Press A Key or Mouse Button to Continue"
            copyArea(2, 1, box.x, box.y, box.w, box.h);
            background();
            n = 0;
            clearInput();
            while (!anyInput()) ctx_.pump();
            clearInput();
        } else {
            next();
            show();
            if (line.empty() || line[0] != '!') font_->draw(*s, tx, box.ty + 12 * n, line, box.ink);
            break;
        }
    }
    if (viaScreen2) {
        show();
        select(1);
    }
    return true;
}

// --- the Music Library (segment 30) ------------------------------------------------

void RockBach::libraryWidgets() {
    // f30_0000: 30 widgets.
    auto w = [](int x0, int y0, int x1, int y1, uint16_t flags, uint16_t down, uint16_t up, uint16_t ovUp = 0x2000,
                uint16_t ovDown = 0x2000, int group = 0) {
        Widget r{x0, y0, x1, y1, flags};
        r.group = group;
        r.bitmapDown = down;
        r.bitmapUp = up;
        r.overlayUp = ovUp;
        r.overlayDown = ovDown;
        return r;
    };
    std::vector<Widget>& l = libraryWidgets_;
    l.clear();
    for (int i = 0; i < 8; ++i)  // 0-7 the composers
        l.push_back(w(i < 4 ? 20 : 92, 34 + 54 * (i % 4), (i < 4 ? 20 : 92) + 69, 85 + 54 * (i % 4), i ? 0x140 : 0x142,
                      0x23CF, 0x23D0, static_cast<uint16_t>(0x23C7 + i), static_cast<uint16_t>(0x23D1 + i), 1));
    l.push_back(w(566, 334, 623, 379, 0x100, 0x200D, 0x200E));              // 8 exit
    l.push_back(w(208, 226, 267, 255, 0x140, 0x2404, 0x2405, 0x2000, 0x2000, 2));  // 9 go
    l.push_back(w(354, 226, 433, 255, 0x142, 0x2406, 0x2407, 0x2000, 0x2000, 2));  // 10 stop
    for (int i = 0; i < 6; ++i) {  // 11-16 the pieces (hot spots over the list)
        Widget p = w(482, 30 + 42 * i, 617, 67 + 42 * i, 0x204, 0, 0, 0, 0);
        p.faceDown = 0x35;
        p.faceUp = 0xB0;
        l.push_back(p);
    }
    for (int i = 0; i < 4; ++i) l.push_back(w(0, 1, 0, 1, 0x204, 0, 0, 0, 0));  // 17-20
    Widget done = w(400, 156, 443, 183, 0x320, 0x23DA, 0x23D9);               // 21
    done.label = dataString(0x2AE2);
    l.push_back(done);
    l.push_back(w(286, 226, 335, 255, 0x100, 0x2402, 0x2403));              // 22 info
    Widget tempo = w(252, 12, 383, 30, 0x108, 0x23DB, 0x23DB, 0x23DC, 0x23DC);  // 23 tempo
    tempo.slider = &librarySlider_;
    l.push_back(tempo);
    for (int i = 0; i < 4; ++i)  // 24-27 the instruments
        l.push_back(w(i < 2 ? 458 : 506, i % 2 ? 366 : 334, (i < 2 ? 458 : 506) + 41, (i % 2 ? 366 : 334) + 25,
                      i ? 0x140 : 0x142, static_cast<uint16_t>(0x23DD + 2 * i), static_cast<uint16_t>(0x23DE + 2 * i),
                      0x2000, 0x2000, 13));
    l.push_back(w(192, 0, 235, 35, 0x104, 0x23A1, 0x23A1, 0x240F, 0x2410));  // 28 slower
    l.push_back(w(398, 0, 443, 35, 0x104, 0x23A2, 0x23A2, 0x2411, 0x2412));  // 29 faster
    l.push_back(w(302, 322, 421, 391, 0x004, 0x2000, 0x2000));             // 30 the inkwell
    for (Widget& x : l) x.hotkey = 1;
}

void RockBach::showComposer(int c, bool viaScreen2) {
    // f30_0f24: the portrait (after a glimpse of another), the name and
    // dates, the life (DS:2B08 lists the composer's pages of LIBINFO.HI),
    // the pieces (SONG.HI), and the composer's time on the timeline.
    static const uint16_t kFacts[8][3] = {{0x2B3B, 0x2B54, 0x2B6F}, {0x2B87, 0x2BA2, 0x2BC0}, {0x2BDE, 0x2BF9, 0x2C11},
                                          {0x2C2F, 0x2C51, 0x2C6D}, {0x2C81, 0x2C99, 0x2CB6}, {0x2CD5, 0x2CF8, 0x2D17},
                                          {0x2D2C, 0x2D44, 0x2D60}, {0x2D73, 0x2D83, 0x2D91}};
    static const uint16_t kLabels[3] = {0x2D9C, 0x2DA8, 0x2DB4};  // "Name:" "Born:" "Died:"
    libraryStopped();
    for (int i = 1; i <= 6; ++i) setColours({ctx_.displayPalette[0]}, i);  // f30_0c2a
    for (int i = 11; i <= 16; ++i) libraryWidgets_[i].flags |= Widget::kHidden;  // f30_0d0c
    library_.piece = -1;
    library_.page[c] = 9;
    if (viaScreen2) select(2);
    if (viaScreen2) {
        fill(0xBC, 0x2C, 0x46, 0x34, 0xAE);
        drawLogo(0xC0, 0x30, static_cast<uint16_t>(0x23E6 + c));
        copyArea(2, 1, 0xBC, 0x2C, 0x4A, 0x38);
        ctx_.countdown[1] = 6;
        while (ctx_.countdown[1] != 0) ctx_.pump();
    }
    fill(0xBC, 0x2C, 0x46, 0x34, 0xAE);
    drawLogo(0xC0, 0x30, static_cast<uint16_t>(0x23C7 + c));
    if (viaScreen2) copyArea(2, 1, 0xBC, 0x2C, 0x4A, 0x38);
    fill(0x104, 0x2A, 0xC4, 0x38, 0xAE);
    Screen& s = ctx_.screens[current()];
    for (int i = 0; i < 3; ++i) {
        font_->draw(s, 0x106, 0x2F + 14 * i, dataString(kLabels[i]), 0);
        font_->draw(s, 0x12E, 0x2F + 14 * i, dataString(kFacts[c][i]), 0);
    }
    if (viaScreen2) {
        copyArea(2, 1, 0x104, 0x2A, 0xC4, 0x38);
        select(1);
    }
    nextInfoPage(c);
    textBox(library_.info, TextBox{library_.infoPage, false, 0, 0xAE, 0xBA, 0x66, 0x10E, 0x76, 0xBA, 0x66, 0}, viaScreen2);
    pieceList(c, viaScreen2);
    for (int i = 0; i < data_[0x2B28 + c]; ++i) libraryWidgets_[11 + i].flags &= ~Widget::kHidden;  // f30_0cc6
    if (viaScreen2) select(1);
    drawOpaque(0xC, 0x13E, 0x23EE);
    // The timeline's marker (DS:2AE8: x and width).
    const int x = dataWord(static_cast<uint16_t>(0x2AE8 + 4 * c)), w = dataWord(static_cast<uint16_t>(0x2AEA + 4 * c));
    restoreArea(library_.marker);
    library_.marker = saveArea(x, 0x122, w, 5);
    fill(x, 0x122, w, 5, 0x25);
}

void RockBach::nextInfoPage(int c) {
    // f30_0b98: the composer's next page of LIBINFO.HI (up to 4, DS:2B08).
    int& p = library_.page[c];
    if (++p >= 4) p = 0;
    if (p > 0 && data_[0x2B08 + c * 4 + p] == 0) p = 0;
    library_.infoPage = data_[0x2B08 + c * 4 + p];
}

void RockBach::pieceList(int c, bool viaScreen2) {
    // f30_0d46: the composer's block of SONG.HI on the right, three lines
    // a piece, piece n in colour n (1-6) so that colour 7 can pick one out.
    if (viaScreen2) select(2);
    drawOpaque(0x1D5, 6, 0x23EF);
    const std::vector<std::string>& lines = library_.list;
    size_t at = 0;
    std::string line;
    auto next = [&] { line = at < lines.size() ? lines[at++] : std::string("!"); };
    int count = -1;
    do {
        while (line.empty() || line[0] != '!') next();
        ++count;
        next();
    } while (count != c && count < 8);
    if (count == 8) line = "!";
    Screen& s = ctx_.screens[current()];
    for (int n = 0, group = 1; line.empty() || line[0] != '!';) {
        font_->draw(s, 0x1E4, 0x1A + 12 * n + 6 * (group - 1), line, group);
        next();
        if (++n % 3 == 0) ++group;
    }
    if (viaScreen2) {
        copyArea(2, 1, 0x1D5, 6, 0xA0, 0xFC);
        select(1);
    }
}

void RockBach::libraryStopped() {
    // f30_0c7c: GO up, STOP down, the piece stopped.
    std::vector<Widget>& w = libraryWidgets_;
    w[9].flags &= ~Widget::kPressed;
    w[10].flags |= Widget::kPressed;
    library_.playing = false;
    drawWidget(w[9], false);
    drawWidget(w[10], true);
    pieceStop();
}

void RockBach::choosePiece(int widget, int c, bool viaScreen2) {
    // f30_169e: the piece picked out in colour 7, its words at the bottom
    // (SONGINFO.HI, counted over the composers before), its record and
    // tempo.
    const int n = widget - 11;
    library_.piece = n;
    for (int i = 1; i <= 6; ++i) setColours({ctx_.displayPalette[0]}, i);
    libraryStopped();
    setColours({ctx_.displayPalette[7]}, widget - 10);
    int block = n;  // f30_0e9a: counted over the composers before, in the box 23EE
    for (int k = 0; k < c; ++k) block += data_[0x2B28 + k];
    textBox(library_.words, TextBox{block, false, 0x23EE, 0, 0xC, 0x13E, 0x11A, 0x4E, 0xC, 0x13E, 0}, viaScreen2);
    if (n >= 0 && n < 5) pieceChoose(c, n);
    const int t = data_[0x2DBA + c * 5 + n];
    pieceTempo(t + 0x7A);
    librarySlider_.value = t;
    placeSlider(libraryWidgets_[23], true);
}

int RockBach::library() {
    // f30_1780.
    std::vector<Widget>& w = libraryWidgets_;
    library_ = LibraryState{};
    library_.page.fill(9);
    libraryWidgets();
    select(2);
    blackout();
    backdrop(0x100D);
    show(2);
    librarySlider_ = Slider{0, 0x46, 0x96, 0xF, 0x13, 6, 4, 0x84};
    initWidgets(w, {0xFB, 0xFC, 0xFD, 0xFE});
    library_.marker = saveArea(dataWord(0x2AE8), 0x122, dataWord(0x2AEA), 5);
    library_.info = readLines(options_.cdDir + "/LIBINFO.HI");
    library_.words = readLines(options_.cdDir + "/SONGINFO.HI");
    library_.list = readLines(options_.cdDir + "/SONG.HI");
    const bool missing = library_.info.empty() || library_.words.empty() || library_.list.empty();
    if (missing) logLine("Rock and Bach: could not open LIBINFO.HI, SONGINFO.HI or SONG.HI");
    fill(0xBC, 0x2C, 0x46, 0x34, 0xAE);
    fill(0x104, 0x2A, 0xC4, 0x38, 0xAE);
    fill(0xBA, 0x66, 0x10E, 0x76, 0xAE);
    int composer = 0;  // [bp-36]
    if (!missing) {
        showComposer(0, false);
        choosePiece(11, composer, false);
    }
    drawOpaque(0x12E, 0x142, 0x2401);
    copyArea(2, 1, 0, 0, Screen::kWidth, Screen::kHeight);
    select(1);
    clearInput();
    int ink = -1, hold = 0;  // [bp-2E], [bp-2C]: the inkwell's frames
    for (bool done = missing; !done;) {
        const int r = pollWidgets();
        if (ink >= 0 && ctx_.countdown[3] == 0) {
            // The quill writes (frames 23F0 + n, 5 a second), stopping a
            // while (14-23 frames) at the ninth.
            drawOpaque(0x12E, 0x142, static_cast<uint16_t>(0x23F0 + ink));
            if (++ink == 9 && hold < rbRand() % 10 + 14) --ink, ++hold;
            if (ink >= 18) hold = 0, ink = -1;
            ctx_.countdown[3] = 2;
        }
        if (r == 8) done = true;
        if (r >= 0 && r <= 7 && r != composer) {
            showComposer(r, true);
            composer = r;
            library_.piece = -1;
            choosePiece(11, composer, true);
        }
        if (r == 9 && !library_.playing && library_.piece >= 0 && library_.piece < data_[0x2B28 + composer]) {
            // GO: the piece with its own instrument (DS:2E68).
            library_.playing = true;
            pieceStart();
            for (int i = 24; i <= 27; ++i) w[i].flags &= ~Widget::kPressed;
            w[data_[0x2E68 + composer * 5 + library_.piece]].flags |= Widget::kPressed;
            for (int i = 24; i <= 27; ++i) drawWidget(w[i], w[i].flags & Widget::kPressed);
        }
        if (r == 10 && library_.playing) {
            library_.playing = false;
            pieceStop();
        }
        if (r >= 11 && r <= 16) choosePiece(r, composer, true);
        if (r == 22) {
            // INFO: the composer's next page, a key or click between boxes.
            nextInfoPage(composer);
            textBox(library_.info, TextBox{library_.infoPage, true, 0, 0xAE, 0xBA, 0x66, 0x10E, 0x76, 0xBA, 0x66, 0},
                    true);
        }
        if (r == 23) pieceTempo(librarySlider_.value + 0x7A);
        if (r >= 24 && r <= 27) pieceInstrument(r - 24);
        if (r == 28 || r == 29) {
            librarySlider_.value = std::clamp(librarySlider_.value + (r == 28 ? -8 : 8), 0, 0x85);
            placeSlider(w[23], true);
            pieceTempo(librarySlider_.value + 0x7A);
        }
        if (r == 30) ink = hold = 0;
        if (library_.playing && options_.music) {
            // The piece over (its first channel quiet). (Only once the driver
            // has started what was sent: SENDSND just queues, and the port's
            // loop comes round again before the driver's next tick, which the
            // original's didn't.)
            uint8_t left = 1;
            ctx_.platform.withFm([&left](ArtechFmDriver& d) {
                if (!d.pending()) left = d.channelStatus(0);
            });
            if (left == 0) {
                w[9].flags &= ~Widget::kPressed;
                w[10].flags |= Widget::kPressed;
                library_.playing = false;
                drawWidget(w[9], false);
                drawWidget(w[10], true);
            }
        }
    }
    freeArea(library_.marker);
    return 0;
}

}  // namespace edison
