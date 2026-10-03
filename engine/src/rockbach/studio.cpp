// WINMAIN's Studio: segment 35 (its top level), segment 4 (the front room:
// load, save, play, make, delete a video), segment 14 (the band maker) and
// segment 15 (the song maker). A video is the record at DS:6754, saved as
// is in a .vid file.

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <random>

#include "rockbach/rockbach.h"

#include "audio/artech_fm_driver.h"

namespace edison {
namespace {

constexpr int kSignSlot = 7;   // g04_080e (3 a second)
constexpr int kSoloSlot = 7;   // g14_10de (3 a second)
constexpr uint16_t kSlotUnlit = 0x2142, kSlotLit = 0x21E8;

// A widget table as the original's initialisers lay it out.
struct Row {
    int x0, y0, x1, y1;
    uint16_t flags;
    uint8_t faceDown, faceUp;
    uint16_t bitmapDown, bitmapUp;
    int hotkey;
    uint16_t overlayUp, overlayDown;
    char mode;
    int group;
};

template <class W>
std::vector<W> table(std::initializer_list<Row> rows) {
    std::vector<W> list;
    for (const Row& r : rows) {
        W w{r.x0, r.y0, r.x1, r.y1, r.flags};
        w.faceDown = r.faceDown;
        w.faceUp = r.faceUp;
        w.bitmapDown = r.bitmapDown;
        w.bitmapUp = r.bitmapUp;
        w.hotkey = r.hotkey;
        w.overlayUp = r.overlayUp;
        w.overlayDown = r.overlayDown;
        w.mode = r.mode;
        w.group = r.group;
        list.push_back(w);
    }
    return list;
}

// Track t's picture in the song maker (and on its slots), style s's.
uint16_t trackPicture(int t) { return static_cast<uint16_t>(t < 8 ? 0x2131 + t : 0x2297 + t); }
uint16_t stylePicture(int s) { return static_cast<uint16_t>(s < 9 ? 0x2139 + s : 0x2215 + s); }

}  // namespace

// --- the video ------------------------------------------------------------------

std::string RockBach::vstring(uint16_t at) const {
    std::string s;
    for (size_t i = at - 0x6754u; i < video_.size() && video_[i] && s.size() < 20; ++i) s += static_cast<char>(video_[i]);
    return s;
}

void RockBach::setVstring(uint16_t at, const std::string& s) {
    for (size_t i = 0; i < 20; ++i) vbyte(static_cast<uint16_t>(at + i)) = i < s.size() && i < 19 ? static_cast<uint8_t>(s[i]) : 0;
}

void RockBach::videoBlank(bool keepProducer) {
    // f33_0144: no band, no song, no scenes; the names 19 spaces (the
    // producer's kept if asked).
    vbyte(kVersion) = 1;
    vbyte(kVideoNamed) = vbyte(kSongNamed) = vbyte(kBandNamed) = 0;
    setVword(kTempo, 0xF0);
    for (int m = 0; m < 4; ++m) {
        vbyte(static_cast<uint16_t>(kMembers + m)) = 0xFF;
        setVword(static_cast<uint16_t>(kCamPlaces + 4 * m), 0);
        setVword(static_cast<uint16_t>(kCamPlaces + 2 + 4 * m), 0);
    }
    for (int i = 0; i < 16; ++i) {
        setVword(static_cast<uint16_t>(kSong + 4 * i), -1);
        setVword(static_cast<uint16_t>(kSong + 2 + 4 * i), -1);
        vbyte(static_cast<uint16_t>(kBgTurns + i)) = 1;
        for (int k = 0; k < 4; ++k) setVword(static_cast<uint16_t>(kBgColours + 8 * i + 2 * k), -1);
        setVword(static_cast<uint16_t>(kBackground + 2 * i), -1);
        vbyte(static_cast<uint16_t>(kFxTurns + i)) = 1;
        for (int k = 0; k < 4; ++k) setVword(static_cast<uint16_t>(kFxColours + 8 * i + 2 * k), -1);
        setVword(static_cast<uint16_t>(kEffect + 2 * i), -1);
        vbyte(static_cast<uint16_t>(kCamPlays + i)) = 1;
        setVword(static_cast<uint16_t>(kCamMotion + 2 * i), 0);
        setVword(static_cast<uint16_t>(kCamera + 2 * i), -1);
        vbyte(static_cast<uint16_t>(kCamColours + i)) = 0;
    }
    for (int i = 0; i < 19; ++i) {
        vbyte(static_cast<uint16_t>(kVideoName + i)) = vbyte(static_cast<uint16_t>(kSongName + i)) =
            vbyte(static_cast<uint16_t>(kBandName + i)) = ' ';
        if (!keepProducer) vbyte(static_cast<uint16_t>(kProducer + i)) = ' ';
    }
    vbyte(static_cast<uint16_t>(kVideoName + 19)) = vbyte(static_cast<uint16_t>(kSongName + 19)) = vbyte(static_cast<uint16_t>(kBandName + 19)) = 0;
    if (!keepProducer) vbyte(static_cast<uint16_t>(kProducer + 19)) = 0;
}

void RockBach::videoNew() {
    // f04_107c: everything FF, then the flags, empty names, the player as
    // the producer.
    video_.fill(0xFF);
    vbyte(kVersion) = 1;
    setVword(kTempo, 0xF0);
    for (int i = 0; i < 16; ++i) {
        vbyte(static_cast<uint16_t>(kBgTurns + i)) = vbyte(static_cast<uint16_t>(kFxTurns + i)) =
            vbyte(static_cast<uint16_t>(kCamPlays + i)) = 1;
        vbyte(static_cast<uint16_t>(kCamColours + i)) = vbyte(static_cast<uint16_t>(kCamPlaces + i)) = 0;
        setVword(static_cast<uint16_t>(kCamMotion + 2 * i), 0);
    }
    setVstring(kVideoName, "");
    setVstring(kSongName, "");
    setVstring(kBandName, "");
    setVstring(kProducer, name_);
    vbyte(kVideoNamed) = vbyte(kSongNamed) = vbyte(kBandNamed) = 0;
}

bool RockBach::videoLoad() {
    // f04_0b46: a video from the CD or the game's folder; it becomes the
    // one in the Studio, named after its file.
    std::string path;
    if (!loadDialog(0x73, 0x2B, ".vid", &path, 0, dataString(0x70C))) return false;
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    in.read(reinterpret_cast<char*>(video_.data()), static_cast<std::streamsize>(video_.size()));
    videoOpen_ = true;
    const auto slash = path.find_last_of("/\\");
    videoName_ = path.substr(slash == std::string::npos ? 0 : slash + 1);
    videoName_ = videoName_.substr(0, videoName_.find('.'));
    return true;
}

bool RockBach::videoSave() {
    // f04_0cd2.
    std::string out;
    if (!nameDialog(0xAE, 0x2E, &videoName_, ".vid", &out)) return false;
    std::ofstream f(out, std::ios::binary);
    if (f) f.write(reinterpret_cast<const char*>(video_.data()), static_cast<std::streamsize>(video_.size()));
    else logLine("Rock and Bach: can't create " + out);
    videoChanged_ = false;
    return true;
}

void RockBach::videoDelete() {
    // f04_0bf2: one of the game's folder's videos, after "PERMANENTLY
    // DELETE THE / VIDEO: name" (CANCEL or YES).
    std::string victim;
    if (!fileList(0x73, 0x2B, options_.saveDir + "/", false, ".vid", &victim, 0, dataString(0x728), true)) return;
    const auto slash = victim.find_last_of("/\\");
    std::string shown = victim.substr(slash == std::string::npos ? 0 : slash + 1);
    shown = shown.substr(0, shown.find('.'));
    if (messageBox(0xA0, 0x50, {dataString(0x5CA), "VIDEO: " + shown}, {0, 1}) == 1) {
        std::error_code ec;
        std::filesystem::remove(victim, ec);
    }
}

// --- the front room (segment 4) -----------------------------------------------------

void RockBach::studioBackdrop(uint16_t id) {
    // A Studio backdrop on screen 2, with Edison's colours (DS:8CC0, the
    // player's look) over its E1-ED.
    backdrop(id);
    std::copy(savedLook_.begin(), savedLook_.end(), ctx_.screens[2].palette.begin() + 0xE1);
}

void RockBach::studioWidgets() {
    // f04_0000: EXIT, then LOAD, SAVE, PLAY, NEW, EDIT, DELETE; Song, Video
    // and Done for "What do you want to do?" (hidden); a spot by the door.
    studioWidgets_ = table<Widget>({
        {566, 16, 637, 63, 0x104, 0x00, 0x00, 0x21BC, 0x21BB, 1, 0x2000, 0x2000, 0, 0},  // 0
        {22, 26, 99, 49, 0x104, 0xA3, 0xA6, 0x2309, 0x2309, 1, 0x230E, 0x2314, 0, 0},  // 1
        {22, 51, 99, 74, 0x104, 0xA3, 0xA6, 0x2309, 0x2309, 1, 0x230F, 0x2315, 0, 0},  // 2
        {22, 76, 99, 99, 0x104, 0xA3, 0xA6, 0x2309, 0x2309, 1, 0x2310, 0x2316, 0, 0},  // 3
        {22, 101, 99, 124, 0x104, 0xA3, 0xA6, 0x2309, 0x2309, 1, 0x2311, 0x2317, 0, 0},  // 4
        {22, 126, 99, 149, 0x104, 0xA3, 0xA6, 0x2309, 0x2309, 1, 0x2312, 0x2312, 0, 0},  // 5
        {22, 151, 99, 174, 0x104, 0xA3, 0xA6, 0x2309, 0x2309, 1, 0x2313, 0x2313, 0, 0},  // 6
        {132, 84, 199, 131, 0x300, 0xA3, 0xA6, 0x2203, 0x21F3, 1, 0x2000, 0x2000, 0, 0},  // 7
        {132, 132, 199, 179, 0x300, 0xA3, 0xA6, 0x220F, 0x21FF, 1, 0x2000, 0x2000, 0, 0},  // 8
        {132, 180, 199, 227, 0x300, 0xA3, 0xA6, 0x2206, 0x21F6, 1, 0x2000, 0x2000, 0, 0},  // 9
        {494, 18, 559, 67, 0x004, 0xA3, 0xA6, 0x220B, 0x21FB, 1, 0x2000, 0x2000, 0, 0},  // 10
        {566, 16, 566, 63, 0x304, 0x00, 0x00, 0x21BC, 0x21BB, 1, 0x2000, 0x2000, 0, 0},  // 11
        {132, 84, 199, 131, 0x300, 0xA3, 0xA6, 0x22A4, 0x22A3, 1, 0x2000, 0x2000, 0, 0},  // 12
        {132, 132, 199, 179, 0x300, 0xA3, 0xA6, 0x22A6, 0x22A5, 1, 0x2000, 0x2000, 0, 0},  // 13
    });
}

void RockBach::studioSign(bool onDisplay) {
    // f04_0824: the flashing sign (218E, then one of 218A-218D at random)
    // on screen 2, and onto the display when asked.
    static std::mt19937 rng{std::random_device{}()};
    if (onDisplay) select(2);
    restoreArea(signArea_, true);
    signArea_ = saveArea(0x22, 0xF4, 0x5C, 0x96);
    ctx_.screens.drawSprite(current(), ctx_.bitmap(0x218E), 0x22, 0xF4);
    ctx_.screens.drawSprite(current(), ctx_.bitmap(static_cast<uint16_t>(0x218A + rng() % 4)), 0x22, 0xF4);
    if (onDisplay) {
        copyArea(2, 1, 0x22, 0xF4, 0x5C, 0x96);
        select(1);
    }
}

int RockBach::studioWhatNext() {
    // f04_0580: "What do you want to do?" in a bubble, with Song, Video and
    // Done (7, 8, 9).
    std::vector<Widget>& w = studioWidgets_;
    const std::string question = dataString(0x619);
    const std::string labels[3] = {dataString(0x636), dataString(0x641), dataString(0x64C)};
    int width = font_->width(question) + 0x96;
    for (const std::string& l : labels) width = std::max(width, font_->width(l) + 0xDA);
    constexpr int kX = 0x73, kY = 0x37, kH = 0xAE;
    const int saved = saveArea(kX, kY, width + 0x1E, kH + 0x1E);
    select(2);
    copyArea(1, 2, kX, kY, width + 0x1E, kH + 0x1E);
    bubble(kX, kY, width, kH, 2);
    font_->draw(ctx_.screens[2], kX + 0x46, 0x40, question, 0);
    for (int i = 0; i < 14; ++i) w[i].flags |= Widget::kHidden;
    for (int i = 7; i <= 9; ++i) {
        w[i].flags &= ~Widget::kHidden;
        drawWidget(w[i], w[i].flags & Widget::kPressed);
        font_->draw(ctx_.screens[2], kX + 0x5F, kY - 4 + (i - 6) * 0x30, labels[i - 7], 0);
    }
    select(1);
    copyArea(2, 1, kX, kY, width + 0x1E, kH + 0x1E);
    int r = -1;
    while (r != 7 && r != 8 && r != 9) r = pollWidgets();
    for (int i = 0; i <= 10; ++i) {
        if (i < 7 || i == 10) w[i].flags &= ~Widget::kHidden;
        else w[i].flags |= Widget::kHidden;
    }
    restoreArea(saved);
    return r;
}

int RockBach::studioRoom(bool edit) {
    // f04_112e: the front room. EXIT (asking about an unsaved video), LOAD
    // (and play it), SAVE, PLAY, NEW (the band and song next), EDIT
    // (straight to the band and song for a new video, else "What do you
    // want to do?"), DELETE. `edit` presses EDIT at once.
    std::vector<Widget>& w = studioWidgets_;
    int result = -1;
    studioWidgets();
    studioBackdrop(0x1004);
    select(2);
    signArea_ = saveArea(0x22, 0xF4, 0x5C, 0x96);
    studioSign(false);
    initWidgets(w, {0xC0, 0xD8, 0xDB, 0xDC});
    show(2);
    select(1);
    clearInput();
    bool blink = false;  // [8718]
    ctx_.timer.setPeriodic(kSignSlot, 3, [&blink] { blink = true; });
    while (result < 0) {
        int r = pollWidgets();
        if (edit) r = 5;
        if (blink) {
            studioSign(true);
            blink = false;
        }
        if (r == 0) {
            result = 0;
            if (videoChanged_) {
                if (videoName_.empty()) videoName_ = dataString(0x779);
                const int answer =
                    messageBox(0xA0, 0x50, {dataString(0x586), "VIDEO: " + videoName_, dataString(0x5B0)}, {0, 3, 2});
                if (answer == 0) result = -1;
                else if (answer == 2 && !videoSave()) result = -1;
            }
        } else if (r == 1) {
            if (videoLoad()) result = 6;
        } else if (r == 2 && videoOpen_) {
            videoSave();
        } else if (r == 3 && videoOpen_) {
            result = 6;
        } else if (r == 4 || r == 5) {
            videoChanged_ = true;
            int next = 7;
            if (r == 4 || !videoOpen_) {
                bandMade_ = false;
                videoOpen_ = true;
                videoNew();
            } else {
                next = studioWhatNext();
            }
            if (next == 7) result = 2;
            else if (next == 8) result = 5;
            else edit = false;
        } else if (r == 6) {
            videoDelete();
        }
    }
    ctx_.timer.setPeriodic(kSignSlot, 0, nullptr);
    freeArea(signArea_);
    return result;
}

// --- Edison's questions (segment 15) ----------------------------------------------------

void RockBach::edisonSays(int* area, const std::string& text, bool ask, int which) {
    // f15_1d7e: he walks in at the left (21DC-21E0, 0.3 s each) on screen 2,
    // the last frame with his bubble (21E1) and the text.
    select(2);
    copyArea(1, 2, 0, 100, 0x17C, 0x6E);
    for (int i = 0; i <= 4; ++i) {
        restoreArea(*area);
        if (i < 4) {
            *area = saveArea(0, 100, 0x3C, 0x6E);
        } else {
            *area = saveArea(0, 100, 0x17C, 0x6E);
            drawLogo(0x37, 0x64, 0x21E1);
            font_->draw(ctx_.screens[2], 0x50, 0x73, text, 0);
        }
        drawLogo(0, 0x64, static_cast<uint16_t>(0x21DC + i));
        copyArea(2, 1, 0, 100, i < 4 ? 0x3C : 0x17C, 0x6E);
        waitCountdown(3);
    }
    select(1);
    if (!ask) {
        // 10 s, or a click or key, or 400 pixels of mouse moves (f33_0380).
        clearInput();
        int mx, my, moved = 0;
        bool down;
        ctx_.platform.mouse(&mx, &my, &down);
        ctx_.countdown[1] = 100;  // [4F80]
        while (ctx_.countdown[1] > 0 && !anyInput() && moved <= 400) {
            ctx_.pump();
            int nx, ny;
            ctx_.platform.mouse(&nx, &ny, &down);
            moved += std::abs(nx - mx) + std::abs(ny - my);
            mx = nx;
            my = ny;
        }
    } else {
        static const uint16_t kFields[3] = {kSongName, kBandName, kVideoName};  // the song, the band, the video
        if (which >= 0 && which < 3) askName(kFields[which]);
    }
    select(2);
    restoreArea(*area);
    *area = saveArea(0, 0, 2, 2);
    copyArea(2, 1, 0, 100, 0x17C, 0x6E);
    select(1);
}

void RockBach::askName(uint16_t field) {
    // f15_1cf2: typed at (50, 82) until Enter, up to 18 letters, upper case.
    std::string s;
    do {
        s.clear();
    } while (!editLine(s, 0x13, 0x50, 0x82, 0x13 * 12, 2));
    for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    setVstring(field, s);
}

// --- the band maker (segment 14) --------------------------------------------------------

void RockBach::bandMakerWidgets() {
    // f14_0000: EXIT; four groups of nine players (the solo, drums, chords
    // and bass... as the roles 3, 0, 1 and 2); the role's chosen player
    // and name; the band's name.
    studioWidgets_ = table<Widget>({
        {490, 344, 623, 389, 0x100, 0x00, 0x00, 0x216F, 0x2170, 1, 0x2000, 0x2000, 0, 0},  // 0
        {24, 234, 93, 285, 0x140, 0x00, 0x00, 0x2004, 0x2003, 1, 0x200F, 0x2033, 0, 1},  // 1
        {94, 234, 163, 285, 0x140, 0x00, 0x00, 0x2004, 0x2003, 1, 0x2010, 0x2034, 0, 1},  // 2
        {164, 234, 233, 285, 0x140, 0x00, 0x00, 0x2004, 0x2003, 1, 0x2011, 0x2035, 0, 1},  // 3
        {24, 288, 94, 339, 0x140, 0x00, 0x00, 0x2004, 0x2003, 1, 0x2012, 0x2036, 0, 1},  // 4
        {94, 288, 163, 339, 0x140, 0x00, 0x00, 0x2004, 0x2003, 1, 0x2013, 0x2037, 0, 1},  // 5
        {164, 288, 233, 339, 0x140, 0x00, 0x00, 0x2004, 0x2003, 1, 0x2014, 0x2038, 0, 1},  // 6
        {24, 342, 93, 393, 0x140, 0x00, 0x00, 0x2004, 0x2003, 1, 0x2015, 0x2039, 0, 1},  // 7
        {94, 342, 163, 393, 0x140, 0x00, 0x00, 0x2004, 0x2003, 1, 0x2016, 0x203A, 0, 1},  // 8
        {164, 342, 233, 393, 0x140, 0x00, 0x00, 0x2004, 0x2003, 1, 0x2017, 0x203B, 0, 1},  // 9
        {254, 234, 323, 285, 0x140, 0x00, 0x00, 0x2008, 0x2007, 1, 0x2018, 0x203C, 0, 2},  // 10
        {324, 234, 393, 285, 0x140, 0x00, 0x00, 0x2008, 0x2007, 1, 0x2019, 0x203D, 0, 2},  // 11
        {394, 234, 463, 285, 0x140, 0x00, 0x00, 0x2008, 0x2007, 1, 0x201A, 0x203E, 0, 2},  // 12
        {254, 288, 323, 339, 0x140, 0x00, 0x00, 0x2008, 0x2007, 1, 0x201B, 0x203F, 0, 2},  // 13
        {324, 288, 393, 339, 0x140, 0x00, 0x00, 0x2008, 0x2007, 1, 0x201C, 0x2040, 0, 2},  // 14
        {394, 288, 463, 339, 0x140, 0x00, 0x00, 0x2008, 0x2007, 1, 0x201D, 0x2041, 0, 2},  // 15
        {254, 342, 323, 393, 0x140, 0x00, 0x00, 0x2008, 0x2007, 1, 0x201E, 0x2042, 0, 2},  // 16
        {324, 342, 393, 393, 0x140, 0x00, 0x00, 0x2008, 0x2007, 1, 0x201F, 0x2043, 0, 2},  // 17
        {394, 342, 463, 393, 0x140, 0x00, 0x00, 0x2008, 0x2007, 1, 0x2020, 0x2044, 0, 2},  // 18
        {254, 36, 323, 87, 0x140, 0x00, 0x00, 0x2002, 0x2001, 1, 0x2021, 0x2045, 0, 3},  // 19
        {324, 36, 393, 87, 0x140, 0x00, 0x00, 0x2002, 0x2001, 1, 0x2022, 0x2046, 0, 3},  // 20
        {394, 36, 463, 87, 0x140, 0x00, 0x00, 0x2002, 0x2001, 1, 0x2023, 0x2047, 0, 3},  // 21
        {254, 90, 323, 141, 0x140, 0x00, 0x00, 0x2002, 0x2001, 1, 0x2024, 0x2048, 0, 3},  // 22
        {324, 90, 393, 141, 0x140, 0x00, 0x00, 0x2002, 0x2001, 1, 0x2025, 0x2049, 0, 3},  // 23
        {394, 90, 463, 141, 0x140, 0x00, 0x00, 0x2002, 0x2001, 1, 0x2026, 0x204A, 0, 3},  // 24
        {254, 144, 323, 195, 0x140, 0x00, 0x00, 0x2002, 0x2001, 1, 0x2027, 0x204B, 0, 3},  // 25
        {324, 144, 393, 195, 0x140, 0x00, 0x00, 0x2002, 0x2001, 1, 0x2028, 0x204C, 0, 3},  // 26
        {394, 144, 463, 195, 0x140, 0x00, 0x00, 0x2002, 0x2001, 1, 0x2029, 0x204D, 0, 3},  // 27
        {24, 36, 93, 87, 0x140, 0x00, 0x00, 0x2006, 0x2005, 1, 0x202A, 0x204E, 0, 4},  // 28
        {94, 36, 163, 87, 0x140, 0x00, 0x00, 0x2006, 0x2005, 1, 0x202B, 0x204F, 0, 4},  // 29
        {164, 36, 233, 87, 0x140, 0x00, 0x00, 0x2006, 0x2005, 1, 0x202C, 0x2050, 0, 4},  // 30
        {24, 90, 93, 141, 0x140, 0x00, 0x00, 0x2006, 0x2005, 1, 0x202D, 0x2051, 0, 4},  // 31
        {94, 90, 163, 141, 0x140, 0x00, 0x00, 0x2006, 0x2005, 1, 0x202E, 0x2052, 0, 4},  // 32
        {164, 90, 233, 141, 0x140, 0x00, 0x00, 0x2006, 0x2005, 1, 0x202F, 0x2053, 0, 4},  // 33
        {24, 144, 93, 195, 0x140, 0x00, 0x00, 0x2006, 0x2005, 1, 0x2030, 0x2054, 0, 4},  // 34
        {94, 144, 163, 195, 0x140, 0x00, 0x00, 0x2006, 0x2005, 1, 0x2031, 0x2055, 0, 4},  // 35
        {164, 144, 233, 195, 0x140, 0x00, 0x00, 0x2006, 0x2005, 1, 0x2032, 0x2056, 0, 4},  // 36
        {550, 236, 619, 287, 0x300, 0x00, 0x00, 0x2006, 0x2005, 1, 0x2000, 0x2000, 0, 0},  // 37
        {490, 196, 555, 235, 0x104, 0x00, 0x00, 0x2003, 0x2003, 1, 0x2000, 0x2000, 0, 0},  // 38
        {490, 266, 555, 305, 0x104, 0x00, 0x00, 0x2007, 0x2007, 1, 0x2000, 0x2000, 0, 0},  // 39
        {490, 126, 555, 165, 0x104, 0x00, 0x00, 0x2001, 0x2001, 1, 0x2000, 0x2000, 0, 0},  // 40
        {490, 56, 555, 95, 0x104, 0x00, 0x00, 0x2005, 0x2005, 1, 0x2000, 0x2000, 0, 0},  // 41
        {490, 248, 559, 265, 0x0A4, 0xAF, 0xAF, 0x0000, 0x0000, 1, 0x0000, 0x0000, 0, 0},  // 42
        {490, 318, 559, 335, 0x0A4, 0x9E, 0x9E, 0x0000, 0x0000, 1, 0x0000, 0x0000, 0, 0},  // 43
        {490, 178, 559, 195, 0x0A4, 0xA0, 0xA0, 0x0000, 0x0000, 1, 0x0000, 0x0000, 0, 0},  // 44
        {490, 108, 559, 125, 0x0A4, 0xDC, 0xDC, 0x0000, 0x0000, 1, 0x0000, 0x0000, 0, 0},  // 45
        {480, 36, 631, 53, 0x0A0, 0x02, 0x01, 0x0000, 0x0000, 1, 0x0000, 0x0000, 0, 0},  // 46
    });
    static const uint16_t kRoles[4] = {0x1AD8, 0x1AE3, 0x1AEE, 0x1AF9};  // Drums, Chord, Bass, Solo
    for (int g = 0; g < 4; ++g) studioWidgets_[42 + g].label = dataString(kRoles[g]);
}

int RockBach::bandMaker() {
    // f14_10f4: a player for each role; the solo's picture plays its riff;
    // the band's name. Leaving fills any empty role (the first of its
    // group) and asks for a name the first time.
    static const int kSolo[28] = {0, 1, 0, 1, 11, 1, 2, 3, 4, 5, 6, 7, 6, 7, 8, 9, 8, 9, 8, 9, 10, 2, 10, 2, 11, 1, 0, 1};
    std::vector<Widget>& w = studioWidgets_;
    auto member = [this](int g) { return static_cast<int8_t>(vbyte(static_cast<uint16_t>(kMembers + g))); };
    auto face = [this](int m) { return static_cast<uint16_t>(0x200E + data_[0x4E + m]); };
    auto memberName = [this](int m) { return dataString(static_cast<uint16_t>(0x194C + m * 11)); };
    blackout();
    bandMakerWidgets();
    int prompt = saveArea(0, 0, 2, 2);
    studioBackdrop(0x1002);
    show(2);
    select(2);
    for (int g = 0; g < 4; ++g) {
        const int m = member(g);
        if (m < 0) continue;
        w[m + 1].flags |= Widget::kPressed;
        w[38 + g].overlayUp = w[38 + g].overlayDown = face(m);
        w[42 + g].label = memberName(m);
    }
    w[46].label = vstring(kBandName);
    initWidgets(w, {0xFB, 0xFC, 0xFD, 0xFE});
    select(1);
    copyArea(2, 1, 0, 0, Screen::kWidth, Screen::kHeight);
    bool solo = false, due = false;  // [1B43]
    int frame = 0, r = -1;
    for (bool done = false; !done;) {
        if (solo && due) {
            if (frame < 28) {
                drawOpaque(0x1EC, 0x3A, static_cast<uint16_t>(0x22A7 + kSolo[frame++]));
            } else {
                solo = false;
                frame = 0;
                ctx_.timer.setPeriodic(kSoloSlot, 0, nullptr);
                drawWidget(w[41], w[41].flags & Widget::kPressed);
            }
            due = false;
        }
        r = pollWidgets();
        if (r == 0) {
            bool empty = false;
            for (int g = 0; g < 4; ++g) empty |= member(g) < 0;
            if (empty)
                for (int g = 0; g < 4; ++g) vbyte(static_cast<uint16_t>(kMembers + g)) = static_cast<uint8_t>(g * 9);
            done = true;
            if (!vbyte(kBandNamed)) {
                vbyte(kBandNamed) = 1;
                sound(0x6023);
                edisonSays(&prompt, dataString(0x1B5B), true, 1);
                if (vstring(kBandName).empty()) setVstring(kBandName, dataString(0x1BE0));
            }
        }
        if (r >= 1 && r <= 36) {
            const int g = (r - 1) / 9;
            vbyte(static_cast<uint16_t>(kMembers + g)) = static_cast<uint8_t>(r - 1);
            w[38 + g].overlayUp = w[38 + g].overlayDown = face(r - 1);
            drawWidget(w[38 + g], w[38 + g].flags & Widget::kPressed);
            w[42 + g].label = memberName(r - 1);
            drawWidget(w[42 + g], w[42 + g].flags & Widget::kPressed);
        }
        if (r == 41) {
            due = false;
            solo = true;
            frame = 0;
            ctx_.timer.setPeriodic(kSoloSlot, 3, [&due] { due = true; });
        }
        if (r == 46) {
            vbyte(kBandNamed) = 1;
            sound(0x6023);
            edisonSays(&prompt, dataString(0x1B5B), true, 1);
            if (vstring(kBandName).empty()) {
                vbyte(kBandNamed) = 0;
                edisonSays(&prompt, dataString(0x1B8F), false, 1);
            }
            w[46].label = vstring(kBandName);
            drawWidget(w[46], w[46].flags & Widget::kPressed);
        }
    }
    ctx_.timer.setPeriodic(kSoloSlot, 0, nullptr);
    freeArea(prompt);
    return r;
}

// --- the song maker (segment 15) --------------------------------------------------------

void RockBach::songMakerWidgets() {
    // f15_0000: EXIT; tracks 0-7 (1-8) and 8-11 (74-77); styles 0-7 (9-16)
    // and 8-15 (63-70); the 16 slots' tracks (17-32) and styles (46-61);
    // GO, STOP; the band button (37); eight songs to start from (38-45);
    // the tempo (62, and 72, 73 a step down or up); the song's name (71).
    studioWidgets_ = table<Widget>({
        {540, 368, 619, 397, 0x100, 0x00, 0x00, 0x21B9, 0x21BA, 1, 0x2000, 0x2000, 0, 0},  // 0
        {499, 23, 562, 44, 0x100, 0x00, 0x00, 0x2142, 0x2142, 1, 0x2131, 0x2131, 'c', 0},  // 1
        {499, 49, 562, 70, 0x100, 0x00, 0x00, 0x2142, 0x2142, 1, 0x2132, 0x2132, 'c', 0},  // 2
        {499, 75, 562, 96, 0x100, 0x00, 0x00, 0x2142, 0x2142, 1, 0x2133, 0x2133, 'c', 0},  // 3
        {499, 101, 562, 122, 0x100, 0x00, 0x00, 0x2142, 0x2142, 1, 0x2134, 0x2134, 'c', 0},  // 4
        {499, 127, 562, 148, 0x100, 0x00, 0x00, 0x2142, 0x2142, 1, 0x2135, 0x2135, 'c', 0},  // 5
        {499, 153, 562, 174, 0x100, 0x00, 0x00, 0x2142, 0x2142, 1, 0x2136, 0x2136, 'c', 0},  // 6
        {567, 23, 630, 44, 0x100, 0x00, 0x00, 0x2142, 0x2142, 1, 0x2137, 0x2137, 'c', 0},  // 7
        {567, 49, 630, 70, 0x100, 0x00, 0x00, 0x2142, 0x2142, 1, 0x2138, 0x2138, 'c', 0},  // 8
        {507, 204, 532, 223, 0x100, 0x00, 0x00, 0x2143, 0x2143, 1, 0x2139, 0x2139, 'c', 0},  // 9
        {537, 204, 562, 223, 0x100, 0x00, 0x00, 0x2143, 0x2143, 1, 0x213A, 0x213A, 'c', 0},  // 10
        {567, 204, 592, 223, 0x100, 0x00, 0x00, 0x2143, 0x2143, 1, 0x213B, 0x213B, 'c', 0},  // 11
        {597, 204, 622, 223, 0x100, 0x00, 0x00, 0x2143, 0x2143, 1, 0x213C, 0x213C, 'c', 0},  // 12
        {507, 228, 532, 247, 0x100, 0x00, 0x00, 0x2143, 0x2143, 1, 0x213D, 0x213D, 'c', 0},  // 13
        {537, 228, 562, 247, 0x100, 0x00, 0x00, 0x2143, 0x2143, 1, 0x213E, 0x213E, 'c', 0},  // 14
        {567, 228, 592, 247, 0x100, 0x00, 0x00, 0x2143, 0x2143, 1, 0x213F, 0x213F, 'c', 0},  // 15
        {597, 228, 622, 247, 0x100, 0x00, 0x00, 0x2143, 0x2143, 1, 0x2140, 0x2140, 'c', 0},  // 16
        {184, 69, 245, 99, 0x144, 0x00, 0x00, 0x2142, 0x2142, 1, 0x2000, 0x2000, 'c', 3},  // 17
        {246, 69, 307, 99, 0x144, 0x00, 0x00, 0x2142, 0x2142, 1, 0x2000, 0x2000, 'c', 3},  // 18
        {308, 69, 369, 99, 0x144, 0x00, 0x00, 0x2142, 0x2142, 1, 0x2000, 0x2000, 'c', 3},  // 19
        {370, 69, 431, 99, 0x144, 0x00, 0x00, 0x2142, 0x2142, 1, 0x2000, 0x2000, 'c', 3},  // 20
        {184, 123, 245, 153, 0x144, 0x00, 0x00, 0x2142, 0x2142, 1, 0x2000, 0x2000, 'c', 3},  // 21
        {246, 123, 307, 153, 0x144, 0x00, 0x00, 0x2142, 0x2142, 1, 0x2000, 0x2000, 'c', 3},  // 22
        {308, 123, 369, 153, 0x144, 0x00, 0x00, 0x2142, 0x2142, 1, 0x2000, 0x2000, 'c', 3},  // 23
        {370, 123, 431, 153, 0x144, 0x00, 0x00, 0x2142, 0x2142, 1, 0x2000, 0x2000, 'c', 3},  // 24
        {184, 177, 245, 207, 0x144, 0x00, 0x00, 0x2142, 0x2142, 1, 0x2000, 0x2000, 'c', 3},  // 25
        {246, 177, 307, 207, 0x144, 0x00, 0x00, 0x2142, 0x2142, 1, 0x2000, 0x2000, 'c', 3},  // 26
        {308, 177, 369, 207, 0x144, 0x00, 0x00, 0x2142, 0x2142, 1, 0x2000, 0x2000, 'c', 3},  // 27
        {370, 177, 431, 207, 0x144, 0x00, 0x00, 0x2142, 0x2142, 1, 0x2000, 0x2000, 'c', 3},  // 28
        {184, 231, 245, 261, 0x144, 0x00, 0x00, 0x2142, 0x2142, 1, 0x2000, 0x2000, 'c', 3},  // 29
        {246, 231, 307, 261, 0x144, 0x00, 0x00, 0x2142, 0x2142, 1, 0x2000, 0x2000, 'c', 3},  // 30
        {308, 231, 369, 261, 0x144, 0x00, 0x00, 0x2142, 0x2142, 1, 0x2000, 0x2000, 'c', 3},  // 31
        {370, 231, 431, 261, 0x144, 0x00, 0x00, 0x2142, 0x2142, 1, 0x2000, 0x2000, 'c', 3},  // 32
        {548, 304, 607, 333, 0x140, 0x00, 0x00, 0x21BF, 0x21C0, 1, 0x2000, 0x2000, 0, 4},  // 33
        {538, 336, 617, 365, 0x142, 0x00, 0x00, 0x21BD, 0x21BE, 1, 0x2000, 0x2000, 0, 4},  // 34
        {20, 300, 89, 351, 0x700, 0x00, 0x00, 0x2145, 0x2144, 1, 0x2000, 0x2000, 0, 0},  // 35
        {4, 288, 163, 317, 0x300, 0x00, 0x00, 0x2147, 0x2146, 1, 0x2000, 0x2000, 0, 0},  // 36
        {4, 288, 163, 317, 0x100, 0x00, 0x00, 0x221C, 0x221D, 1, 0x2000, 0x2000, 0, 0},  // 37
        {16, 38, 131, 57, 0x140, 0x41, 0x4F, 0x21AF, 0x21B0, 1, 0x21B1, 0x21B1, 0, 6},  // 38
        {16, 60, 131, 79, 0x140, 0x41, 0x4F, 0x21AF, 0x21B0, 1, 0x21B2, 0x21B2, 0, 6},  // 39
        {16, 82, 131, 101, 0x140, 0x41, 0x4F, 0x21AF, 0x21B0, 1, 0x21B3, 0x21B3, 0, 6},  // 40
        {16, 104, 131, 123, 0x140, 0x41, 0x4F, 0x21AF, 0x21B0, 1, 0x21B4, 0x21B4, 0, 6},  // 41
        {16, 126, 131, 145, 0x140, 0x41, 0x4F, 0x21AF, 0x21B0, 1, 0x21B5, 0x21B5, 0, 6},  // 42
        {16, 148, 131, 167, 0x140, 0x41, 0x4F, 0x21AF, 0x21B0, 1, 0x21B6, 0x21B6, 0, 6},  // 43
        {16, 170, 131, 189, 0x140, 0x41, 0x4F, 0x21AF, 0x21B0, 1, 0x21B7, 0x21B7, 0, 6},  // 44
        {16, 192, 131, 211, 0x140, 0x41, 0x4F, 0x21AF, 0x21B0, 1, 0x21B8, 0x21B8, 0, 6},  // 45
        {186, 51, 209, 68, 0x144, 0x00, 0x00, 0x2143, 0x2143, 1, 0x2143, 0x2143, 'c', 9},  // 46
        {248, 51, 271, 68, 0x144, 0x00, 0x00, 0x2143, 0x2143, 1, 0x2143, 0x2143, 'c', 9},  // 47
        {310, 51, 333, 68, 0x144, 0x00, 0x00, 0x2143, 0x2143, 1, 0x2143, 0x2143, 'c', 9},  // 48
        {372, 51, 395, 68, 0x144, 0x00, 0x00, 0x2143, 0x2143, 1, 0x2143, 0x2143, 'c', 9},  // 49
        {186, 105, 209, 122, 0x144, 0x00, 0x00, 0x2143, 0x2143, 1, 0x2143, 0x2143, 'c', 9},  // 50
        {248, 105, 271, 122, 0x144, 0x00, 0x00, 0x2143, 0x2143, 1, 0x2143, 0x2143, 'c', 9},  // 51
        {310, 105, 333, 122, 0x144, 0x00, 0x00, 0x2143, 0x2143, 1, 0x2143, 0x2143, 'c', 9},  // 52
        {372, 105, 395, 122, 0x144, 0x00, 0x00, 0x2143, 0x2143, 1, 0x2143, 0x2143, 'c', 9},  // 53
        {186, 159, 209, 176, 0x144, 0x00, 0x00, 0x2143, 0x2143, 1, 0x2143, 0x2143, 'c', 9},  // 54
        {248, 159, 271, 176, 0x144, 0x00, 0x00, 0x2143, 0x2143, 1, 0x2143, 0x2143, 'c', 9},  // 55
        {310, 159, 333, 176, 0x144, 0x00, 0x00, 0x2143, 0x2143, 1, 0x2143, 0x2143, 'c', 9},  // 56
        {372, 159, 395, 176, 0x144, 0x00, 0x00, 0x2143, 0x2143, 1, 0x2143, 0x2143, 'c', 9},  // 57
        {186, 213, 209, 230, 0x144, 0x00, 0x00, 0x2143, 0x2143, 1, 0x2143, 0x2143, 'c', 9},  // 58
        {248, 213, 271, 230, 0x144, 0x00, 0x00, 0x2143, 0x2143, 1, 0x2143, 0x2143, 'c', 9},  // 59
        {310, 213, 333, 230, 0x144, 0x00, 0x00, 0x2143, 0x2143, 1, 0x2143, 0x2143, 'c', 9},  // 60
        {372, 213, 395, 230, 0x144, 0x00, 0x00, 0x2143, 0x2143, 1, 0x2143, 0x2143, 'c', 9},  // 61
        {41, 248, 108, 261, 0x108, 0x00, 0x00, 0x2216, 0x2216, 0, 0x2217, 0x2217, 0, 0},  // 62
        {507, 252, 532, 271, 0x100, 0x00, 0x00, 0x2143, 0x2143, 1, 0x2141, 0x2141, 'c', 0},  // 63
        {537, 252, 562, 271, 0x100, 0x00, 0x00, 0x2143, 0x2143, 1, 0x221E, 0x221E, 'c', 0},  // 64
        {567, 252, 592, 271, 0x100, 0x00, 0x00, 0x2143, 0x2143, 1, 0x221F, 0x221F, 'c', 0},  // 65
        {597, 252, 622, 271, 0x100, 0x00, 0x00, 0x2143, 0x2143, 1, 0x2220, 0x2220, 'c', 0},  // 66
        {507, 276, 532, 295, 0x100, 0x00, 0x00, 0x2143, 0x2143, 1, 0x2221, 0x2221, 'c', 0},  // 67
        {537, 276, 562, 295, 0x100, 0x00, 0x00, 0x2143, 0x2143, 1, 0x2222, 0x2222, 'c', 0},  // 68
        {567, 276, 592, 295, 0x100, 0x00, 0x00, 0x2143, 0x2143, 1, 0x2223, 0x2223, 'c', 0},  // 69
        {597, 276, 622, 295, 0x100, 0x00, 0x00, 0x2143, 0x2143, 1, 0x2224, 0x2224, 'c', 0},  // 70
        {152, 24, 449, 45, 0x0A0, 0xB0, 0xA0, 0x0000, 0x0000, 1, 0x0000, 0x0000, 0, 0},  // 71
        {0, 237, 33, 266, 0x100, 0xB0, 0xA0, 0x2296, 0x2297, 1, 0x2000, 0x2000, 0, 0},  // 72
        {114, 237, 147, 266, 0x100, 0xB0, 0xA0, 0x2298, 0x2299, 1, 0x2000, 0x2000, 0, 0},  // 73
        {567, 75, 630, 96, 0x100, 0x00, 0x00, 0x2142, 0x2142, 1, 0x229F, 0x229F, 'c', 0},  // 74
        {567, 101, 630, 122, 0x100, 0x00, 0x00, 0x2142, 0x2142, 1, 0x22A0, 0x22A0, 'c', 0},  // 75
        {567, 127, 630, 148, 0x100, 0x00, 0x00, 0x2142, 0x2142, 1, 0x22A1, 0x22A1, 'c', 0},  // 76
        {567, 153, 630, 174, 0x100, 0x00, 0x00, 0x2142, 0x2142, 1, 0x22A2, 0x22A2, 'c', 0},  // 77
    });
    studioWidgets_[62].slider = &studioSliders_[0];
    studioWidgets_[71].label = vstring(kSongName);
}

int RockBach::songMaker() {
    // f15_1f84: a track or a style picked up (its picture follows the
    // pointer) goes into the slot clicked next; a click on a slot with
    // nothing carried plays the song from there, the slot playing lit.
    // GO plays it all, STOP stops. Leaving fills an unfinished song with
    // track 0 in style 0 (and asks for a name the first time); the band
    // button goes back to the band maker (-99).
    std::vector<Widget>& w = studioWidgets_;
    auto slotTrack = [this](int i) { return vword(static_cast<uint16_t>(kSong + 4 * i)); };
    auto slotStyle = [this](int i) { return vword(static_cast<uint16_t>(kSong + 2 + 4 * i)); };
    auto showSlot = [&](int i) {
        const int t = slotTrack(i), s = slotStyle(i);
        if (t >= 0 && t < 12) w[17 + i].overlayUp = w[17 + i].overlayDown = trackPicture(t);
        if (s >= 0 && s < 16) w[46 + i].overlayUp = w[46 + i].overlayDown = stylePicture(s);
    };
    auto event = [this]() {
        uint8_t e = 0;
        ctx_.platform.withFm([&e](ArtechFmDriver& d) { e = d.peek(d.getVar()); });
        return static_cast<int>(static_cast<int8_t>(e));
    };
    blackout();
    musicReset();  // (f20_0286: every slot track 7)
    studioSliders_[0] = Slider{0, vword(kTempo) - 0x7A, 0xA7, 0xE, 0xA, 9, 5, 0x44};
    tempo_ = static_cast<uint8_t>(vword(kTempo));
    songMakerWidgets();
    for (int i = 0; i < 16; ++i) showSlot(i);
    select(2);
    studioBackdrop(0x1003);
    show(2);
    // The band: a stand for each (2007), the player on it, the names.
    static const int kX[4] = {0x6A, 0xC6, 0x122, 0x17E};
    for (int g = 0; g < 4; ++g) drawOpaque(kX[g], 0x15A, 0x2007);
    for (int g = 0; g < 4; ++g) {
        const int m = static_cast<int8_t>(vbyte(static_cast<uint16_t>(kMembers + g)));
        if (m < 0) continue;
        drawLogo(kX[g], 0x15A, g == 3 && m == 0x23 ? 0x21E7 : static_cast<uint16_t>(0x200E + data_[0x4E + m]));
    }
    if (static_cast<int8_t>(vbyte(kMembers)) >= 0) {
        const std::string band = vstring(kBandName);
        font_->draw(ctx_.screens[2], 0x14D - font_->width(band) / 2, 0x132, band, 0);
        for (int g = 0; g < 4; ++g) {
            const int m = static_cast<int8_t>(vbyte(static_cast<uint16_t>(kMembers + g)));
            font_->draw(ctx_.screens[2], kX[g] + 2, 0x146, dataString(static_cast<uint16_t>(0x194C + m * 11)), 0);
        }
    }
    initWidgets(w, {0xA9, 0xA3, 0x92, 0x8D});
    copyArea(2, 1, 0, 0, Screen::kWidth, Screen::kHeight);
    select(1);
    struct Carry {
        bool on = false;      // [bp-22]
        uint16_t picture = 0xFFFF;  // [bp-1C]
        int ox = 0, oy = 0, w = 0, h = 0;
        int area = 0;         // [bp-18]
        int x = 0, y = 0;     // the pointer it was drawn at
    } carry;
    carry.area = saveArea(0, 0, 2, 2);
    int prompt = saveArea(0, 0, 2, 2);  // [bp-16]
    int track = -1, style = -1;         // [bp-8], [bp-12]: picked up
    bool playing = false;               // [bp-2A]
    int lit = 0x10;                     // [bp-26]
    int result = 0x7FFF;                // [bp-14]
    clearInput();
    auto hideCarry = [&] {
        if (carry.on) restoreArea(carry.area), carry.area = saveArea(0, 0, 2, 2);
    };
    auto drawCarry = [&](int mx, int my) {
        mx = std::clamp(mx, carry.ox, Screen::kWidth - carry.ox);
        my = std::clamp(my, carry.oy, Screen::kHeight - carry.oy);
        restoreArea(carry.area);
        carry.area = saveArea(mx - carry.ox, my - carry.oy, carry.w, carry.h);
        drawLogo(mx - carry.ox, my - carry.oy, carry.picture);
        carry.x = mx;
        carry.y = my;
    };
    auto drop = [&] {
        restoreArea(carry.area);
        carry.area = saveArea(0, 0, 2, 2);
        carry.on = false;
    };
    auto unlightAll = [&] {
        for (int i = 17; i <= 32; ++i) {
            w[i].bitmapDown = w[i].bitmapUp = kSlotUnlit;
            drawWidget(w[i], w[i].flags & Widget::kPressed);
        }
    };
    auto stopped = [&] {
        // GO up, STOP down.
        w[33].flags &= ~Widget::kPressed;
        w[34].flags |= Widget::kPressed;
        drawWidget(w[33], false);
        drawWidget(w[34], true);
    };
    auto pickUp = [&](int r, int ox, int oy, int cw, int ch) {
        hideCarry();
        carry = Carry{true, w[r].overlayDown, ox, oy, cw, ch, carry.area};
        int mx, my;
        bool down;
        ctx_.platform.mouse(&mx, &my, &down);
        drawCarry(mx, my);
    };
    while (result == 0x7FFF) {
        const int r = pollWidgets();
        const bool anyClick = r >= 0 || lastClick_.on || lastKey_ != 0;
        if (playing && lit != event()) {
            const int e = event();
            const bool was = carry.on;
            if (was) restoreArea(carry.area), carry.area = saveArea(0, 0, 2, 2);
            if (e < 16) {
                lit = e;
                w[lit + 17].bitmapDown = w[lit + 17].bitmapUp = kSlotLit;
                drawWidget(w[lit + 17], w[lit + 17].flags & Widget::kPressed);
                if (lit > 0) {
                    w[lit + 16].bitmapDown = w[lit + 16].bitmapUp = kSlotUnlit;
                    drawWidget(w[lit + 16], w[lit + 16].flags & Widget::kPressed);
                }
            } else {
                lit = e;
                musicStop();
                playing = false;
                w[lit + 16].bitmapDown = w[lit + 16].bitmapUp = kSlotUnlit;
                drawWidget(w[lit + 16], w[lit + 16].flags & Widget::kPressed);
                stopped();
            }
            if (was) drawCarry(carry.x, carry.y);
        }
        int mx, my;
        bool down;
        ctx_.platform.mouse(&mx, &my, &down);
        if (carry.on && carry.picture != 0xFFFF && (mx != carry.x || my != carry.y)) drawCarry(mx, my);
        if (carry.on && anyClick) drop();
        if (r == 0) {
            bool unfinished = false, noBand = false;
            for (int i = 0; i < 16; ++i) unfinished |= slotTrack(i) < 0 || slotStyle(i) < 0;
            if (unfinished) {
                for (int i = 0; i < 16; ++i) {
                    setVword(static_cast<uint16_t>(kSong + 4 * i), 0);
                    setVword(static_cast<uint16_t>(kSong + 2 + 4 * i), 0);
                }
            }
            for (int g = 0; g < 4; ++g) noBand |= static_cast<int8_t>(vbyte(static_cast<uint16_t>(kMembers + g))) < 0;
            if (noBand) {
                edisonSays(&prompt, dataString(0x1E46), false, 0);
            } else {
                result = vbyte(kVersion);
                if (!vbyte(kSongNamed)) {
                    vbyte(kSongNamed) = 1;
                    sound(0x6024);
                    edisonSays(&prompt, dataString(0x1E7D), true, 0);
                    if (vstring(kSongName).empty()) setVstring(kSongName, dataString(0x1ECE));
                }
            }
            drawWidget(w[0], w[0].flags & Widget::kPressed);
        }
        if (r >= 17 && r <= 32) {
            if (track >= 0) {
                w[r].overlayUp = w[r].overlayDown = trackPicture(track);
                drawWidget(w[r], w[r].flags & Widget::kPressed);
                setVword(static_cast<uint16_t>(kSong + 4 * (r - 17)), track);
            } else {
                unlightAll();
                if (playing) musicStop();
                playing = true;
                lit = r - 18;
                int song[16][2];
                for (int i = 0; i < 16; ++i) song[i][0] = slotTrack(i), song[i][1] = slotStyle(i);
                setSong(song);
                musicPlay(r - 17);
                w[33].flags |= Widget::kPressed;
                w[34].flags &= ~Widget::kPressed;
                drawWidget(w[33], true);
                drawWidget(w[34], false);
            }
            track = -1;
            carry.picture = 0xFFFF;
        }
        if (r >= 46 && r <= 61) {
            if (style >= 0) {
                w[r].overlayUp = w[r].overlayDown = stylePicture(style);
                drawWidget(w[r], w[r].flags & Widget::kPressed);
                setVword(static_cast<uint16_t>(kSong + 2 + 4 * (r - 46)), style);
            }
            style = -1;
            carry.picture = 0xFFFF;
        }
        if (r == 33 && !playing) {
            unlightAll();
            playing = true;
            lit = 15;
            int song[16][2];
            for (int i = 0; i < 16; ++i) song[i][0] = slotTrack(i), song[i][1] = slotStyle(i);
            setSong(song);
            musicPlay(0);
            ctx_.platform.withFm([](ArtechFmDriver& d) { d.poke(d.getVar(), 0); });
            drawWidget(w[r], w[r].flags & Widget::kPressed);
        }
        if (r == 34) {
            unlightAll();
            playing = false;
            const int e = std::clamp(event(), 0, 16);  // (16, after the end: GO, as in the original)
            w[e + 17].bitmapDown = w[e + 17].bitmapUp = kSlotUnlit;
            musicStop();
            drawWidget(w[e + 17], w[e + 17].flags & Widget::kPressed);
            drawWidget(w[r], w[r].flags & Widget::kPressed);
        }
        if (r == 37) {
            drawWidget(w[r], w[r].flags & Widget::kPressed);
            musicStop();
            result = -99;
        }
        if (r >= 38 && r <= 45) {
            // One of the eight songs (DS:1BE6, 0x40 bytes each).
            for (int i = 0; i < 16; ++i) {
                const uint16_t from = static_cast<uint16_t>(0x1BE6 + (r - 38) * 0x40 + 4 * i);
                setVword(static_cast<uint16_t>(kSong + 4 * i), static_cast<int16_t>(dataWord(from)));
                setVword(static_cast<uint16_t>(kSong + 2 + 4 * i), static_cast<int16_t>(dataWord(static_cast<uint16_t>(from + 2))));
                showSlot(i);
                drawWidget(w[17 + i], w[17 + i].flags & Widget::kPressed);
                drawWidget(w[46 + i], w[46 + i].flags & Widget::kPressed);
            }
            for (int i : {r, r - 1, r + 1}) drawWidget(w[i], w[i].flags & Widget::kPressed);
        }
        if ((r >= 1 && r <= 8) || (r >= 74 && r <= 77)) {
            track = r <= 8 ? r - 1 : r - 0x42;
            style = -1;
            pickUp(r, 0x1F, 0xF, 0x3E, 0x1F);
        }
        if ((r >= 9 && r <= 16) || (r >= 63 && r <= 70)) {
            style = r <= 16 ? r - 9 : r - 0x37;
            track = -1;
            pickUp(r, 0xB, 8, 0x16, 0x10);
        }
        if (r == 62) {
            tempo_ = static_cast<uint8_t>(studioSliders_[0].value + 0x7A);
            setVword(kTempo, studioSliders_[0].value + 0x7A);
        }
        if (r == 71) {
            vbyte(kSongNamed) = 1;
            sound(0x6024);
            edisonSays(&prompt, dataString(0x1E7D), true, 0);
            if (vstring(kSongName).empty()) setVstring(kSongName, dataString(0x1ED3));
            w[71].label = vstring(kSongName);
            drawWidget(w[r], w[r].flags & Widget::kPressed);
        }
        if (r == 72 || r == 73) {
            Slider& t = studioSliders_[0];
            t.value = r == 72 ? std::max(t.value - 8, 0) : std::min(t.value + 8, 0x85);
            placeSlider(w[62], true);
            tempo_ = static_cast<uint8_t>(t.value + 0x7A);
            setVword(kTempo, t.value + 0x7A);
        }
        if (r == 62 || r == 72 || r == 73) {
            unlightAll();
            playing = false;
            const int e = std::clamp(event(), 0, 16);  // (16, after the end: GO, as in the original)
            w[e + 17].bitmapDown = w[e + 17].bitmapUp = kSlotUnlit;
            musicStop();
            drawWidget(w[e + 17], w[e + 17].flags & Widget::kPressed);
            stopped();
        }
    }
    musicStop();
    freeArea(carry.area);
    freeArea(prompt);
    return result;
}

// --- the video makers' widgets (segments 16, 17 and 26) ------------------------------------

void RockBach::backgroundMakerWidgets() {
    // f16_0000: the Background Mode.
    studioWidgets_ = table<Widget>({
        {496, 4, 631, 55, 0x100, 0x00, 0x00, 0x2171, 0x2172, 1, 0x2000, 0x2000, 0, 0},  // 0
        {8, 88, 121, 137, 0x142, 0xA3, 0xA6, 0x21AC, 0x21AD, 1, 0x215C, 0x215C, 0, 4},  // 1
        {8, 152, 121, 201, 0x140, 0xA3, 0xA6, 0x21AC, 0x21AD, 1, 0x215E, 0x215E, 0, 4},  // 2
        {8, 24, 121, 73, 0x140, 0xA3, 0xA6, 0x21AC, 0x21AD, 1, 0x2160, 0x2160, 0, 4},  // 3
        {66, 244, 117, 267, 0x142, 0xA3, 0xA6, 0x2161, 0x2162, 1, 0x2000, 0x2000, 0, 5},  // 4
        {12, 244, 63, 267, 0x140, 0xA3, 0xA6, 0x2163, 0x2164, 1, 0x2000, 0x2000, 0, 5},  // 5
        {492, 64, 557, 111, 0x104, 0xA3, 0xA6, 0x2003, 0x2003, 1, 0x2000, 0x2000, 0, 0},  // 6
        {492, 136, 557, 183, 0x104, 0xA3, 0xA6, 0x2007, 0x2007, 1, 0x2000, 0x2000, 0, 0},  // 7
        {564, 64, 629, 135, 0x104, 0xA3, 0xA6, 0x2001, 0x2001, 1, 0x2000, 0x2000, 0, 0},  // 8
        {564, 136, 629, 207, 0x104, 0xA3, 0xA6, 0x2005, 0x2005, 1, 0x2000, 0x2000, 0, 0},  // 9
        {574, 218, 625, 241, 0x140, 0xA3, 0xA6, 0x2173, 0x2174, 1, 0x2000, 0x2000, 0, 6},  // 10
        {574, 248, 625, 271, 0x142, 0xA3, 0xA6, 0x2175, 0x2176, 1, 0x2000, 0x2000, 0, 6},  // 11
        {150, 34, 219, 85, 0x104, 0xA3, 0xA6, 0x21AE, 0x21AE, 1, 0x2000, 0x2000, 'c', 0},  // 12
        {238, 34, 307, 85, 0x104, 0xA3, 0xA6, 0x21AE, 0x21AE, 1, 0x2000, 0x2000, 'c', 0},  // 13
        {326, 34, 395, 85, 0x104, 0xA3, 0xA6, 0x21AE, 0x21AE, 1, 0x2000, 0x2000, 'c', 0},  // 14
        {414, 34, 483, 85, 0x104, 0xA3, 0xA6, 0x21AE, 0x21AE, 1, 0x2000, 0x2000, 'c', 0},  // 15
        {150, 94, 219, 145, 0x104, 0xA3, 0xA6, 0x21AE, 0x21AE, 1, 0x2000, 0x2000, 'c', 0},  // 16
        {238, 94, 307, 145, 0x104, 0xA3, 0xA6, 0x21AE, 0x21AE, 1, 0x2000, 0x2000, 'c', 0},  // 17
        {326, 94, 395, 145, 0x104, 0xA3, 0xA6, 0x21AE, 0x21AE, 1, 0x2000, 0x2000, 'c', 0},  // 18
        {414, 94, 483, 145, 0x104, 0xA3, 0xA6, 0x21AE, 0x21AE, 1, 0x2000, 0x2000, 'c', 0},  // 19
        {150, 154, 219, 205, 0x104, 0xA3, 0xA6, 0x21AE, 0x21AE, 1, 0x2000, 0x2000, 'c', 0},  // 20
        {238, 154, 307, 205, 0x104, 0xA3, 0xA6, 0x21AE, 0x21AE, 1, 0x2000, 0x2000, 'c', 0},  // 21
        {326, 154, 395, 205, 0x104, 0xA3, 0xA6, 0x21AE, 0x21AE, 1, 0x2000, 0x2000, 'c', 0},  // 22
        {414, 154, 483, 205, 0x104, 0xA3, 0xA6, 0x21AE, 0x21AE, 1, 0x2000, 0x2000, 'c', 0},  // 23
        {150, 214, 219, 265, 0x104, 0xA3, 0xA6, 0x21AE, 0x21AE, 1, 0x2000, 0x2000, 'c', 0},  // 24
        {238, 214, 307, 265, 0x104, 0xA3, 0xA6, 0x21AE, 0x21AE, 1, 0x2000, 0x2000, 'c', 0},  // 25
        {326, 214, 395, 265, 0x104, 0xA3, 0xA6, 0x21AE, 0x21AE, 1, 0x2000, 0x2000, 'c', 0},  // 26
        {414, 214, 483, 265, 0x104, 0xA3, 0xA6, 0x21AE, 0x21AE, 1, 0x2000, 0x2000, 'c', 0},  // 27
        {134, 34, 149, 85, 0x500, 0xA3, 0xA6, 0x2192, 0x2193, 1, 0x2000, 0x2000, 0, 0},  // 28
        {222, 34, 237, 85, 0x500, 0xA3, 0xA6, 0x2192, 0x2193, 1, 0x2000, 0x2000, 0, 0},  // 29
        {310, 34, 325, 85, 0x500, 0xA3, 0xA6, 0x2192, 0x2193, 1, 0x2000, 0x2000, 0, 0},  // 30
        {398, 34, 413, 85, 0x500, 0xA3, 0xA6, 0x2192, 0x2193, 1, 0x2000, 0x2000, 0, 0},  // 31
        {134, 94, 149, 145, 0x500, 0xA3, 0xA6, 0x2192, 0x2193, 1, 0x2000, 0x2000, 0, 0},  // 32
        {222, 94, 237, 145, 0x500, 0xA3, 0xA6, 0x2192, 0x2193, 1, 0x2000, 0x2000, 0, 0},  // 33
        {310, 94, 325, 145, 0x500, 0xA3, 0xA6, 0x2192, 0x2193, 1, 0x2000, 0x2000, 0, 0},  // 34
        {398, 94, 413, 145, 0x500, 0xA3, 0xA6, 0x2192, 0x2193, 1, 0x2000, 0x2000, 0, 0},  // 35
        {134, 154, 149, 205, 0x500, 0xA3, 0xA6, 0x2192, 0x2193, 1, 0x2000, 0x2000, 0, 0},  // 36
        {222, 154, 237, 205, 0x500, 0xA3, 0xA6, 0x2192, 0x2193, 1, 0x2000, 0x2000, 0, 0},  // 37
        {310, 154, 325, 205, 0x500, 0xA3, 0xA6, 0x2192, 0x2193, 1, 0x2000, 0x2000, 0, 0},  // 38
        {398, 154, 413, 205, 0x500, 0xA3, 0xA6, 0x2192, 0x2193, 1, 0x2000, 0x2000, 0, 0},  // 39
        {134, 214, 149, 265, 0x500, 0xA3, 0xA6, 0x2192, 0x2193, 1, 0x2000, 0x2000, 0, 0},  // 40
        {222, 214, 237, 265, 0x500, 0xA3, 0xA6, 0x2192, 0x2193, 1, 0x2000, 0x2000, 0, 0},  // 41
        {310, 214, 325, 265, 0x500, 0xA3, 0xA6, 0x2192, 0x2193, 1, 0x2000, 0x2000, 0, 0},  // 42
        {398, 214, 413, 265, 0x500, 0xA3, 0xA6, 0x2192, 0x2193, 1, 0x2000, 0x2000, 0, 0},  // 43
        {48, 298, 117, 349, 0x100, 0xA3, 0xA6, 0x2155, 0x214F, 1, 0x2000, 0x2000, 0, 0},  // 44
        {146, 298, 215, 349, 0x100, 0xA3, 0xA6, 0x2156, 0x2150, 1, 0x2000, 0x2000, 0, 0},  // 45
        {244, 298, 313, 349, 0x100, 0xA3, 0xA6, 0x2157, 0x2151, 1, 0x2000, 0x2000, 0, 0},  // 46
        {342, 298, 411, 349, 0x100, 0xA3, 0xA6, 0x2158, 0x2152, 1, 0x2000, 0x2000, 0, 0},  // 47
        {440, 298, 509, 349, 0x100, 0xA3, 0xA6, 0x2159, 0x2153, 1, 0x2000, 0x2000, 0, 0},  // 48
        {538, 298, 607, 349, 0x100, 0xA3, 0xA6, 0x215A, 0x2154, 1, 0x2000, 0x2000, 0, 0},  // 49
        {32, 298, 47, 349, 0x502, 0xA3, 0xA6, 0x2192, 0x2193, 1, 0x2000, 0x2000, 0, 0},  // 50
        {130, 298, 145, 349, 0x502, 0xA3, 0xA6, 0x2192, 0x2193, 1, 0x2000, 0x2000, 0, 0},  // 51
        {228, 298, 243, 349, 0x502, 0xA3, 0xA6, 0x2192, 0x2193, 1, 0x2000, 0x2000, 0, 0},  // 52
        {326, 298, 341, 349, 0x502, 0xA3, 0xA6, 0x2192, 0x2193, 1, 0x2000, 0x2000, 0, 0},  // 53
        {424, 298, 439, 349, 0x502, 0xA3, 0xA6, 0x2192, 0x2193, 1, 0x2000, 0x2000, 0, 0},  // 54
        {522, 298, 537, 349, 0x502, 0xA3, 0xA6, 0x2192, 0x2193, 1, 0x2000, 0x2000, 0, 0},  // 55
        {28, 352, 49, 371, 0x080, 0xF0, 0xF0, 0x2000, 0x2000, 1, 0x2000, 0x2000, 0, 0},  // 56
        {50, 352, 71, 371, 0x080, 0xF1, 0xF1, 0x2000, 0x2000, 1, 0x2000, 0x2000, 0, 0},  // 57
        {72, 352, 93, 371, 0x080, 0xF2, 0xF2, 0x2000, 0x2000, 1, 0x2000, 0x2000, 0, 0},  // 58
        {94, 352, 115, 371, 0x080, 0xF3, 0xA9, 0x2000, 0x2000, 1, 0x2000, 0x2000, 0, 0},  // 59
        {126, 352, 147, 371, 0x080, 0xF0, 0xF0, 0x2000, 0x2000, 1, 0x2000, 0x2000, 0, 0},  // 60
        {148, 352, 169, 371, 0x080, 0xF1, 0xF1, 0x2000, 0x2000, 1, 0x2000, 0x2000, 0, 0},  // 61
        {170, 352, 191, 371, 0x080, 0xF2, 0xF2, 0x2000, 0x2000, 1, 0x2000, 0x2000, 0, 0},  // 62
        {192, 352, 213, 371, 0x080, 0xF3, 0xA9, 0x2000, 0x2000, 1, 0x2000, 0x2000, 0, 0},  // 63
        {224, 352, 245, 371, 0x080, 0xF0, 0xA6, 0x2000, 0x2000, 1, 0x2000, 0x2000, 0, 0},  // 64
        {246, 352, 267, 371, 0x080, 0xA3, 0xA6, 0x2000, 0x2000, 1, 0x2000, 0x2000, 0, 0},  // 65
        {268, 352, 289, 371, 0x080, 0xA3, 0xA6, 0x2000, 0x2000, 1, 0x2000, 0x2000, 0, 0},  // 66
        {290, 352, 311, 371, 0x080, 0xA3, 0xA6, 0x2000, 0x2000, 1, 0x2000, 0x2000, 0, 0},  // 67
        {322, 352, 343, 371, 0x080, 0xF0, 0xA6, 0x2000, 0x2000, 1, 0x2000, 0x2000, 0, 0},  // 68
        {344, 352, 365, 371, 0x080, 0xA3, 0xA6, 0x2000, 0x2000, 1, 0x2000, 0x2000, 0, 0},  // 69
        {366, 352, 387, 371, 0x080, 0xA3, 0xA6, 0x2000, 0x2000, 1, 0x2000, 0x2000, 0, 0},  // 70
        {388, 352, 409, 371, 0x080, 0xA3, 0xA6, 0x2000, 0x2000, 1, 0x2000, 0x2000, 0, 0},  // 71
        {420, 352, 441, 371, 0x080, 0xF0, 0xA6, 0x2000, 0x2000, 1, 0x2000, 0x2000, 0, 0},  // 72
        {442, 352, 463, 371, 0x080, 0xA3, 0xA6, 0x2000, 0x2000, 1, 0x2000, 0x2000, 0, 0},  // 73
        {464, 352, 485, 371, 0x080, 0xA3, 0xA6, 0x2000, 0x2000, 1, 0x2000, 0x2000, 0, 0},  // 74
        {486, 352, 507, 371, 0x080, 0xA3, 0xA6, 0x2000, 0x2000, 1, 0x2000, 0x2000, 0, 0},  // 75
        {518, 352, 539, 371, 0x080, 0xF0, 0xA6, 0x2000, 0x2000, 1, 0x2000, 0x2000, 0, 0},  // 76
        {540, 352, 561, 371, 0x080, 0xA3, 0xA6, 0x2000, 0x2000, 1, 0x2000, 0x2000, 0, 0},  // 77
        {562, 352, 583, 371, 0x080, 0xA3, 0xA6, 0x2000, 0x2000, 1, 0x2000, 0x2000, 0, 0},  // 78
        {584, 352, 605, 371, 0x080, 0xA3, 0xA6, 0x2000, 0x2000, 1, 0x2000, 0x2000, 0, 0},  // 79
        {492, 116, 561, 135, 0x0A4, 0xAF, 0xAF, 0x0000, 0x0000, 1, 0x0000, 0x0000, 0, 0},  // 80
        {492, 188, 561, 207, 0x0A4, 0x9E, 0x9E, 0x0000, 0x0000, 1, 0x0000, 0x0000, 0, 0},  // 81
        {564, 116, 633, 135, 0x0A4, 0xA0, 0xA0, 0x0000, 0x0000, 1, 0x0000, 0x0000, 0, 0},  // 82
        {564, 188, 633, 207, 0x0A4, 0xDC, 0xDC, 0x0000, 0x0000, 1, 0x0000, 0x0000, 0, 0},  // 83
        {8, 2, 183, 21, 0x0A4, 0xDC, 0xDC, 0x0000, 0x0000, 1, 0x0000, 0x0000, 0, 0},  // 84
        {60, 372, 81, 391, 0x080, 0xF0, 0xA6, 0x2000, 0x2000, 1, 0x2000, 0x2000, 0, 0},  // 85
        {158, 372, 179, 391, 0x080, 0xF0, 0xA6, 0x2000, 0x2000, 1, 0x2000, 0x2000, 0, 0},  // 86
        {256, 372, 277, 391, 0x080, 0xF0, 0xA6, 0x2000, 0x2000, 1, 0x2000, 0x2000, 0, 0},  // 87
        {354, 372, 375, 391, 0x080, 0xF0, 0xA6, 0x2000, 0x2000, 1, 0x2000, 0x2000, 0, 0},  // 88
        {452, 372, 473, 391, 0x080, 0xF0, 0xA6, 0x2000, 0x2000, 1, 0x2000, 0x2000, 0, 0},  // 89
        {550, 372, 571, 391, 0x080, 0xF0, 0xA6, 0x2000, 0x2000, 1, 0x2000, 0x2000, 0, 0},  // 90
        {190, 2, 431, 21, 0x0A0, 0xD9, 0xDC, 0x0000, 0x0000, 1, 0x0000, 0x0000, 0, 0},  // 91
    });
}

void RockBach::effectMakerWidgets() {
    // f17_0000: the Special Effect Mode.
    studioWidgets_ = table<Widget>({
        {496, 4, 631, 55, 0x100, 0x00, 0x00, 0x2171, 0x2172, 1, 0x2000, 0x2000, 0, 0},  // 0
        {8, 88, 121, 137, 0x140, 0xA3, 0xA6, 0x21AC, 0x21AD, 1, 0x215C, 0x215C, 0, 4},  // 1
        {8, 152, 121, 201, 0x142, 0xA3, 0xA6, 0x21AC, 0x21AD, 1, 0x215E, 0x215E, 0, 4},  // 2
        {8, 24, 121, 73, 0x140, 0xA3, 0xA6, 0x21AC, 0x21AD, 1, 0x2160, 0x2160, 0, 4},  // 3
        {66, 244, 117, 267, 0x142, 0xA3, 0xA6, 0x2161, 0x2162, 1, 0x2000, 0x2000, 0, 5},  // 4
        {12, 244, 63, 267, 0x140, 0xA3, 0xA6, 0x2163, 0x2164, 1, 0x2000, 0x2000, 0, 5},  // 5
        {492, 64, 557, 111, 0x104, 0xA3, 0xA6, 0x2003, 0x2003, 1, 0x2000, 0x2000, 0, 0},  // 6
        {492, 136, 557, 183, 0x104, 0xA3, 0xA6, 0x2007, 0x2007, 1, 0x2000, 0x2000, 0, 0},  // 7
        {564, 64, 629, 111, 0x104, 0xA3, 0xA6, 0x2001, 0x2001, 1, 0x2000, 0x2000, 0, 0},  // 8
        {564, 136, 629, 183, 0x104, 0xA3, 0xA6, 0x2005, 0x2005, 1, 0x2000, 0x2000, 0, 0},  // 9
        {574, 218, 625, 241, 0x140, 0xA3, 0xA6, 0x2173, 0x2174, 1, 0x2000, 0x2000, 0, 6},  // 10
        {574, 248, 625, 271, 0x142, 0xA3, 0xA6, 0x2175, 0x2176, 1, 0x2000, 0x2000, 0, 6},  // 11
        {150, 34, 219, 85, 0x104, 0xA3, 0xA6, 0x21AE, 0x21AE, 1, 0x2000, 0x2000, 0, 0},  // 12
        {238, 34, 307, 85, 0x104, 0xA3, 0xA6, 0x21AE, 0x21AE, 1, 0x2000, 0x2000, 0, 0},  // 13
        {326, 34, 395, 85, 0x104, 0xA3, 0xA6, 0x21AE, 0x21AE, 1, 0x2000, 0x2000, 0, 0},  // 14
        {414, 34, 483, 85, 0x104, 0xA3, 0xA6, 0x21AE, 0x21AE, 1, 0x2000, 0x2000, 0, 0},  // 15
        {150, 94, 219, 145, 0x104, 0xA3, 0xA6, 0x21AE, 0x21AE, 1, 0x2000, 0x2000, 0, 0},  // 16
        {238, 94, 307, 145, 0x104, 0xA3, 0xA6, 0x21AE, 0x21AE, 1, 0x2000, 0x2000, 0, 0},  // 17
        {326, 94, 395, 145, 0x104, 0xA3, 0xA6, 0x21AE, 0x21AE, 1, 0x2000, 0x2000, 0, 0},  // 18
        {414, 94, 483, 145, 0x104, 0xA3, 0xA6, 0x21AE, 0x21AE, 1, 0x2000, 0x2000, 0, 0},  // 19
        {150, 154, 219, 205, 0x104, 0xA3, 0xA6, 0x21AE, 0x21AE, 1, 0x2000, 0x2000, 0, 0},  // 20
        {238, 154, 307, 205, 0x104, 0xA3, 0xA6, 0x21AE, 0x21AE, 1, 0x2000, 0x2000, 0, 0},  // 21
        {326, 154, 395, 205, 0x104, 0xA3, 0xA6, 0x21AE, 0x21AE, 1, 0x2000, 0x2000, 0, 0},  // 22
        {414, 154, 483, 205, 0x104, 0xA3, 0xA6, 0x21AE, 0x21AE, 1, 0x2000, 0x2000, 0, 0},  // 23
        {150, 214, 219, 265, 0x104, 0xA3, 0xA6, 0x21AE, 0x21AE, 1, 0x2000, 0x2000, 0, 0},  // 24
        {238, 214, 307, 265, 0x104, 0xA3, 0xA6, 0x21AE, 0x21AE, 1, 0x2000, 0x2000, 0, 0},  // 25
        {326, 214, 395, 265, 0x104, 0xA3, 0xA6, 0x21AE, 0x21AE, 1, 0x2000, 0x2000, 0, 0},  // 26
        {414, 214, 483, 265, 0x104, 0xA3, 0xA6, 0x21AE, 0x21AE, 1, 0x2000, 0x2000, 0, 0},  // 27
        {134, 34, 149, 85, 0x500, 0xA3, 0xA6, 0x2192, 0x2193, 1, 0x2000, 0x2000, 0, 0},  // 28
        {222, 34, 237, 85, 0x500, 0xA3, 0xA6, 0x2192, 0x2193, 1, 0x2000, 0x2000, 0, 0},  // 29
        {310, 34, 325, 85, 0x500, 0xA3, 0xA6, 0x2192, 0x2193, 1, 0x2000, 0x2000, 0, 0},  // 30
        {398, 34, 413, 85, 0x500, 0xA3, 0xA6, 0x2192, 0x2193, 1, 0x2000, 0x2000, 0, 0},  // 31
        {134, 94, 149, 145, 0x500, 0xA3, 0xA6, 0x2192, 0x2193, 1, 0x2000, 0x2000, 0, 0},  // 32
        {222, 94, 237, 145, 0x500, 0xA3, 0xA6, 0x2192, 0x2193, 1, 0x2000, 0x2000, 0, 0},  // 33
        {310, 94, 325, 145, 0x500, 0xA3, 0xA6, 0x2192, 0x2193, 1, 0x2000, 0x2000, 0, 0},  // 34
        {398, 94, 413, 145, 0x500, 0xA3, 0xA6, 0x2192, 0x2193, 1, 0x2000, 0x2000, 0, 0},  // 35
        {134, 154, 149, 205, 0x500, 0xA3, 0xA6, 0x2192, 0x2193, 1, 0x2000, 0x2000, 0, 0},  // 36
        {222, 154, 237, 205, 0x500, 0xA3, 0xA6, 0x2192, 0x2193, 1, 0x2000, 0x2000, 0, 0},  // 37
        {310, 154, 325, 205, 0x500, 0xA3, 0xA6, 0x2192, 0x2193, 1, 0x2000, 0x2000, 0, 0},  // 38
        {398, 154, 413, 205, 0x500, 0xA3, 0xA6, 0x2192, 0x2193, 1, 0x2000, 0x2000, 0, 0},  // 39
        {134, 214, 149, 265, 0x500, 0xA3, 0xA6, 0x2192, 0x2193, 1, 0x2000, 0x2000, 0, 0},  // 40
        {222, 214, 237, 265, 0x500, 0xA3, 0xA6, 0x2192, 0x2193, 1, 0x2000, 0x2000, 0, 0},  // 41
        {310, 214, 325, 265, 0x500, 0xA3, 0xA6, 0x2192, 0x2193, 1, 0x2000, 0x2000, 0, 0},  // 42
        {398, 214, 413, 265, 0x500, 0xA3, 0xA6, 0x2192, 0x2193, 1, 0x2000, 0x2000, 0, 0},  // 43
        {48, 298, 117, 349, 0x100, 0xA3, 0xA6, 0x219E, 0x2198, 1, 0x2000, 0x2000, 0, 0},  // 44
        {146, 298, 215, 349, 0x100, 0xA3, 0xA6, 0x219F, 0x2199, 1, 0x2000, 0x2000, 0, 0},  // 45
        {244, 298, 313, 349, 0x100, 0xA3, 0xA6, 0x21A0, 0x219A, 1, 0x2000, 0x2000, 0, 0},  // 46
        {342, 298, 411, 349, 0x100, 0xA3, 0xA6, 0x21A1, 0x219B, 1, 0x2000, 0x2000, 0, 0},  // 47
        {440, 298, 509, 349, 0x100, 0xA3, 0xA6, 0x21A2, 0x219C, 1, 0x2000, 0x2000, 0, 0},  // 48
        {538, 298, 607, 349, 0x100, 0xA3, 0xA6, 0x21A3, 0x219D, 1, 0x2000, 0x2000, 0, 0},  // 49
        {32, 298, 47, 349, 0x502, 0xA3, 0xA6, 0x2192, 0x2193, 1, 0x2000, 0x2000, 0, 0},  // 50
        {130, 298, 145, 349, 0x502, 0xA3, 0xA6, 0x2192, 0x2193, 1, 0x2000, 0x2000, 0, 0},  // 51
        {228, 298, 243, 349, 0x502, 0xA3, 0xA6, 0x2192, 0x2193, 1, 0x2000, 0x2000, 0, 0},  // 52
        {326, 298, 341, 349, 0x502, 0xA3, 0xA6, 0x2192, 0x2193, 1, 0x2000, 0x2000, 0, 0},  // 53
        {424, 298, 439, 349, 0x502, 0xA3, 0xA6, 0x2192, 0x2193, 1, 0x2000, 0x2000, 0, 0},  // 54
        {522, 298, 537, 349, 0x502, 0xA3, 0xA6, 0x2192, 0x2193, 1, 0x2000, 0x2000, 0, 0},  // 55
        {28, 352, 49, 371, 0x080, 0xF0, 0xF0, 0x2000, 0x2000, 1, 0x2000, 0x2000, 0, 0},  // 56
        {50, 352, 71, 371, 0x080, 0xF1, 0xF1, 0x2000, 0x2000, 1, 0x2000, 0x2000, 0, 0},  // 57
        {72, 352, 93, 371, 0x080, 0xF2, 0xF2, 0x2000, 0x2000, 1, 0x2000, 0x2000, 0, 0},  // 58
        {94, 352, 115, 371, 0x080, 0xF3, 0xA9, 0x2000, 0x2000, 1, 0x2000, 0x2000, 0, 0},  // 59
        {126, 352, 147, 371, 0x080, 0xF0, 0xF0, 0x2000, 0x2000, 1, 0x2000, 0x2000, 0, 0},  // 60
        {148, 352, 169, 371, 0x080, 0xF1, 0xF1, 0x2000, 0x2000, 1, 0x2000, 0x2000, 0, 0},  // 61
        {170, 352, 191, 371, 0x080, 0xF2, 0xF2, 0x2000, 0x2000, 1, 0x2000, 0x2000, 0, 0},  // 62
        {192, 352, 213, 371, 0x080, 0xF3, 0xA9, 0x2000, 0x2000, 1, 0x2000, 0x2000, 0, 0},  // 63
        {224, 352, 245, 371, 0x080, 0xF0, 0xA6, 0x2000, 0x2000, 1, 0x2000, 0x2000, 0, 0},  // 64
        {246, 352, 267, 371, 0x080, 0xA3, 0xA6, 0x2000, 0x2000, 1, 0x2000, 0x2000, 0, 0},  // 65
        {268, 352, 289, 371, 0x080, 0xA3, 0xA6, 0x2000, 0x2000, 1, 0x2000, 0x2000, 0, 0},  // 66
        {290, 352, 311, 371, 0x080, 0xA3, 0xA6, 0x2000, 0x2000, 1, 0x2000, 0x2000, 0, 0},  // 67
        {322, 352, 343, 371, 0x080, 0xF0, 0xA6, 0x2000, 0x2000, 1, 0x2000, 0x2000, 0, 0},  // 68
        {344, 352, 365, 371, 0x080, 0xA3, 0xA6, 0x2000, 0x2000, 1, 0x2000, 0x2000, 0, 0},  // 69
        {366, 352, 387, 371, 0x080, 0xA3, 0xA6, 0x2000, 0x2000, 1, 0x2000, 0x2000, 0, 0},  // 70
        {388, 352, 409, 371, 0x080, 0xA3, 0xA6, 0x2000, 0x2000, 1, 0x2000, 0x2000, 0, 0},  // 71
        {420, 352, 441, 371, 0x080, 0xF0, 0xA6, 0x2000, 0x2000, 1, 0x2000, 0x2000, 0, 0},  // 72
        {442, 352, 463, 371, 0x080, 0xA3, 0xA6, 0x2000, 0x2000, 1, 0x2000, 0x2000, 0, 0},  // 73
        {464, 352, 485, 371, 0x080, 0xA3, 0xA6, 0x2000, 0x2000, 1, 0x2000, 0x2000, 0, 0},  // 74
        {486, 352, 507, 371, 0x080, 0xA3, 0xA6, 0x2000, 0x2000, 1, 0x2000, 0x2000, 0, 0},  // 75
        {518, 352, 539, 371, 0x080, 0xF0, 0xA6, 0x2000, 0x2000, 1, 0x2000, 0x2000, 0, 0},  // 76
        {540, 352, 561, 371, 0x080, 0xA3, 0xA6, 0x2000, 0x2000, 1, 0x2000, 0x2000, 0, 0},  // 77
        {562, 352, 583, 371, 0x080, 0xA3, 0xA6, 0x2000, 0x2000, 1, 0x2000, 0x2000, 0, 0},  // 78
        {584, 352, 605, 371, 0x080, 0xA3, 0xA6, 0x2000, 0x2000, 1, 0x2000, 0x2000, 0, 0},  // 79
        {492, 116, 561, 135, 0x0A4, 0xAF, 0xAF, 0x0000, 0x0000, 1, 0x0000, 0x0000, 0, 0},  // 80
        {492, 188, 561, 207, 0x0A4, 0x9E, 0x9E, 0x0000, 0x0000, 1, 0x0000, 0x0000, 0, 0},  // 81
        {564, 116, 633, 135, 0x0A4, 0xA0, 0xA0, 0x0000, 0x0000, 1, 0x0000, 0x0000, 0, 0},  // 82
        {564, 188, 633, 207, 0x0A4, 0xDC, 0xDC, 0x0000, 0x0000, 1, 0x0000, 0x0000, 0, 0},  // 83
        {8, 2, 183, 21, 0x0A4, 0xDC, 0xDC, 0x0000, 0x0000, 1, 0x0000, 0x0000, 0, 0},  // 84
        {60, 372, 81, 391, 0x080, 0xF0, 0xA6, 0x2000, 0x2000, 1, 0x2000, 0x2000, 0, 0},  // 85
        {158, 372, 179, 391, 0x080, 0xF0, 0xA6, 0x2000, 0x2000, 1, 0x2000, 0x2000, 0, 0},  // 86
        {256, 372, 277, 391, 0x080, 0xF0, 0xA6, 0x2000, 0x2000, 1, 0x2000, 0x2000, 0, 0},  // 87
        {354, 372, 375, 391, 0x080, 0xF0, 0xA6, 0x2000, 0x2000, 1, 0x2000, 0x2000, 0, 0},  // 88
        {452, 372, 473, 391, 0x080, 0xF0, 0xA6, 0x2000, 0x2000, 1, 0x2000, 0x2000, 0, 0},  // 89
        {550, 372, 571, 391, 0x080, 0xF0, 0xA6, 0x2000, 0x2000, 1, 0x2000, 0x2000, 0, 0},  // 90
        {190, 2, 431, 21, 0x0A0, 0xD9, 0xDC, 0x0000, 0x0000, 1, 0x0000, 0x0000, 0, 0},  // 91
    });
}

void RockBach::cameraMakerWidgets() {
    // f26_0000: the Camera View Mode.
    studioWidgets_ = table<Widget>({
        {496, 4, 631, 55, 0x100, 0x00, 0x00, 0x2171, 0x2172, 1, 0x2000, 0x2000, 0, 0},  // 0
        {8, 88, 121, 137, 0x140, 0xA3, 0xA6, 0x21AC, 0x21AD, 1, 0x215C, 0x215C, 0, 4},  // 1
        {8, 152, 121, 201, 0x140, 0xA3, 0xA6, 0x21AC, 0x21AD, 1, 0x215E, 0x215E, 0, 4},  // 2
        {8, 24, 121, 73, 0x142, 0xA3, 0xA6, 0x21AC, 0x21AD, 1, 0x2160, 0x2160, 0, 4},  // 3
        {66, 244, 117, 267, 0x142, 0xA3, 0xA6, 0x2161, 0x2162, 1, 0x2000, 0x2000, 0, 5},  // 4
        {12, 244, 63, 267, 0x140, 0xA3, 0xA6, 0x2163, 0x2164, 1, 0x2000, 0x2000, 0, 5},  // 5
        {492, 64, 557, 111, 0x104, 0xA3, 0xA6, 0x2003, 0x2003, 1, 0x2000, 0x2000, 0, 0},  // 6
        {492, 136, 557, 183, 0x104, 0xA3, 0xA6, 0x2007, 0x2007, 1, 0x2000, 0x2000, 0, 0},  // 7
        {564, 64, 629, 111, 0x104, 0xA3, 0xA6, 0x2001, 0x2001, 1, 0x2000, 0x2000, 0, 0},  // 8
        {564, 136, 629, 183, 0x104, 0xA3, 0xA6, 0x2005, 0x2005, 1, 0x2000, 0x2000, 0, 0},  // 9
        {574, 218, 625, 241, 0x140, 0xA3, 0xA6, 0x2173, 0x2174, 1, 0x2000, 0x2000, 0, 6},  // 10
        {574, 248, 625, 271, 0x142, 0xA3, 0xA6, 0x2175, 0x2176, 1, 0x2000, 0x2000, 0, 6},  // 11
        {150, 34, 219, 85, 0x104, 0xA3, 0xA6, 0x21AE, 0x21AE, 1, 0x2000, 0x2000, 'b', 0},  // 12
        {238, 34, 307, 85, 0x104, 0xA3, 0xA6, 0x21AE, 0x21AE, 1, 0x2000, 0x2000, 'b', 0},  // 13
        {326, 34, 395, 85, 0x104, 0xA3, 0xA6, 0x21AE, 0x21AE, 1, 0x2000, 0x2000, 'b', 0},  // 14
        {414, 34, 483, 85, 0x104, 0xA3, 0xA6, 0x21AE, 0x21AE, 1, 0x2000, 0x2000, 'b', 0},  // 15
        {150, 94, 219, 145, 0x104, 0xA3, 0xA6, 0x21AE, 0x21AE, 1, 0x2000, 0x2000, 'b', 0},  // 16
        {238, 94, 307, 145, 0x104, 0xA3, 0xA6, 0x21AE, 0x21AE, 1, 0x2000, 0x2000, 'b', 0},  // 17
        {326, 94, 395, 145, 0x104, 0xA3, 0xA6, 0x21AE, 0x21AE, 1, 0x2000, 0x2000, 'b', 0},  // 18
        {414, 94, 483, 145, 0x104, 0xA3, 0xA6, 0x21AE, 0x21AE, 1, 0x2000, 0x2000, 'b', 0},  // 19
        {150, 154, 219, 205, 0x104, 0xA3, 0xA6, 0x21AE, 0x21AE, 1, 0x2000, 0x2000, 'b', 0},  // 20
        {238, 154, 307, 205, 0x104, 0xA3, 0xA6, 0x21AE, 0x21AE, 1, 0x2000, 0x2000, 'b', 0},  // 21
        {326, 154, 395, 205, 0x104, 0xA3, 0xA6, 0x21AE, 0x21AE, 1, 0x2000, 0x2000, 'b', 0},  // 22
        {414, 154, 483, 205, 0x104, 0xA3, 0xA6, 0x21AE, 0x21AE, 1, 0x2000, 0x2000, 'b', 0},  // 23
        {150, 214, 219, 265, 0x104, 0xA3, 0xA6, 0x21AE, 0x21AE, 1, 0x2000, 0x2000, 'b', 0},  // 24
        {238, 214, 307, 265, 0x104, 0xA3, 0xA6, 0x21AE, 0x21AE, 1, 0x2000, 0x2000, 'b', 0},  // 25
        {326, 214, 395, 265, 0x104, 0xA3, 0xA6, 0x21AE, 0x21AE, 1, 0x2000, 0x2000, 'b', 0},  // 26
        {414, 214, 483, 265, 0x104, 0xA3, 0xA6, 0x21AE, 0x21AE, 1, 0x2000, 0x2000, 'b', 0},  // 27
        {134, 34, 149, 59, 0x500, 0xA3, 0xA6, 0x2194, 0x2195, 1, 0x2000, 0x2000, 0, 0},  // 28
        {222, 34, 237, 59, 0x500, 0xA3, 0xA6, 0x2194, 0x2195, 1, 0x2000, 0x2000, 0, 0},  // 29
        {310, 34, 325, 59, 0x500, 0xA3, 0xA6, 0x2194, 0x2195, 1, 0x2000, 0x2000, 0, 0},  // 30
        {398, 34, 413, 59, 0x500, 0xA3, 0xA6, 0x2194, 0x2195, 1, 0x2000, 0x2000, 0, 0},  // 31
        {134, 94, 149, 119, 0x500, 0xA3, 0xA6, 0x2194, 0x2195, 1, 0x2000, 0x2000, 0, 0},  // 32
        {222, 94, 237, 119, 0x500, 0xA3, 0xA6, 0x2194, 0x2195, 1, 0x2000, 0x2000, 0, 0},  // 33
        {310, 94, 325, 119, 0x500, 0xA3, 0xA6, 0x2194, 0x2195, 1, 0x2000, 0x2000, 0, 0},  // 34
        {398, 94, 413, 119, 0x500, 0xA3, 0xA6, 0x2194, 0x2195, 1, 0x2000, 0x2000, 0, 0},  // 35
        {134, 154, 149, 179, 0x500, 0xA3, 0xA6, 0x2194, 0x2195, 1, 0x2000, 0x2000, 0, 0},  // 36
        {222, 154, 237, 179, 0x500, 0xA3, 0xA6, 0x2194, 0x2195, 1, 0x2000, 0x2000, 0, 0},  // 37
        {310, 154, 325, 179, 0x500, 0xA3, 0xA6, 0x2194, 0x2195, 1, 0x2000, 0x2000, 0, 0},  // 38
        {398, 154, 413, 179, 0x500, 0xA3, 0xA6, 0x2194, 0x2195, 1, 0x2000, 0x2000, 0, 0},  // 39
        {134, 214, 149, 239, 0x500, 0xA3, 0xA6, 0x2194, 0x2195, 1, 0x2000, 0x2000, 0, 0},  // 40
        {222, 214, 237, 239, 0x500, 0xA3, 0xA6, 0x2194, 0x2195, 1, 0x2000, 0x2000, 0, 0},  // 41
        {310, 214, 325, 239, 0x500, 0xA3, 0xA6, 0x2194, 0x2195, 1, 0x2000, 0x2000, 0, 0},  // 42
        {398, 214, 413, 239, 0x500, 0xA3, 0xA6, 0x2194, 0x2195, 1, 0x2000, 0x2000, 0, 0},  // 43
        {48, 298, 117, 349, 0x100, 0xA3, 0xA6, 0x21A8, 0x21A9, 1, 0x21A6, 0x21A6, 0, 0},  // 44
        {146, 298, 215, 349, 0x100, 0xA3, 0xA6, 0x2004, 0x2003, 1, 0x2000, 0x2000, 0, 0},  // 45
        {244, 298, 313, 349, 0x100, 0xA3, 0xA6, 0x2008, 0x2007, 1, 0x2000, 0x2000, 0, 0},  // 46
        {342, 298, 411, 349, 0x100, 0xA3, 0xA6, 0x2002, 0x2001, 1, 0x2000, 0x2000, 0, 0},  // 47
        {440, 298, 509, 349, 0x100, 0xA3, 0xA6, 0x2006, 0x2005, 1, 0x2000, 0x2000, 0, 0},  // 48
        {538, 298, 607, 349, 0x100, 0xA3, 0xA6, 0x21AA, 0x21AB, 1, 0x21A7, 0x21A7, 0, 0},  // 49
        {32, 298, 47, 323, 0x502, 0xA3, 0xA6, 0x2194, 0x2195, 1, 0x2000, 0x2000, 0, 0},  // 50
        {130, 298, 145, 323, 0x502, 0xA3, 0xA6, 0x2194, 0x2195, 1, 0x2000, 0x2000, 0, 0},  // 51
        {228, 298, 243, 323, 0x502, 0xA3, 0xA6, 0x2194, 0x2195, 1, 0x2000, 0x2000, 0, 0},  // 52
        {326, 298, 341, 323, 0x502, 0xA3, 0xA6, 0x2194, 0x2195, 1, 0x2000, 0x2000, 0, 0},  // 53
        {424, 298, 439, 323, 0x502, 0xA3, 0xA6, 0x2194, 0x2195, 1, 0x2000, 0x2000, 0, 0},  // 54
        {522, 298, 537, 323, 0x502, 0xA3, 0xA6, 0x2194, 0x2195, 1, 0x2000, 0x2000, 0, 0},  // 55
        {492, 116, 561, 135, 0x0A4, 0xAF, 0xAF, 0x0000, 0x0000, 1, 0x0000, 0x0000, 0, 0},  // 56
        {492, 188, 561, 207, 0x0A4, 0x9E, 0x9E, 0x0000, 0x0000, 1, 0x0000, 0x0000, 0, 0},  // 57
        {564, 116, 633, 135, 0x0A4, 0xA0, 0xA0, 0x0000, 0x0000, 1, 0x0000, 0x0000, 0, 0},  // 58
        {564, 188, 633, 207, 0x0A4, 0xDC, 0xDC, 0x0000, 0x0000, 1, 0x0000, 0x0000, 0, 0},  // 59
        {32, 324, 47, 349, 0x500, 0xA3, 0xA6, 0x2196, 0x2197, 1, 0x2000, 0x2000, 0, 0},  // 60
        {130, 324, 145, 349, 0x500, 0xA3, 0xA6, 0x2196, 0x2197, 1, 0x2000, 0x2000, 0, 0},  // 61
        {228, 324, 243, 349, 0x500, 0xA3, 0xA6, 0x2196, 0x2197, 1, 0x2000, 0x2000, 0, 0},  // 62
        {326, 324, 341, 349, 0x500, 0xA3, 0xA6, 0x2196, 0x2197, 1, 0x2000, 0x2000, 0, 0},  // 63
        {424, 324, 439, 349, 0x500, 0xA3, 0xA6, 0x2196, 0x2197, 1, 0x2000, 0x2000, 0, 0},  // 64
        {522, 324, 537, 349, 0x500, 0xA3, 0xA6, 0x2196, 0x2197, 1, 0x2000, 0x2000, 0, 0},  // 65
        {134, 60, 149, 85, 0x500, 0xA3, 0xA6, 0x2196, 0x2197, 1, 0x2000, 0x2000, 0, 0},  // 66
        {222, 60, 237, 85, 0x500, 0xA3, 0xA6, 0x2196, 0x2197, 1, 0x2000, 0x2000, 0, 0},  // 67
        {310, 60, 325, 85, 0x500, 0xA3, 0xA6, 0x2196, 0x2197, 1, 0x2000, 0x2000, 0, 0},  // 68
        {398, 60, 413, 85, 0x500, 0xA3, 0xA6, 0x2196, 0x2197, 1, 0x2000, 0x2000, 0, 0},  // 69
        {134, 120, 149, 145, 0x500, 0xA3, 0xA6, 0x2196, 0x2197, 1, 0x2000, 0x2000, 0, 0},  // 70
        {222, 120, 237, 145, 0x500, 0xA3, 0xA6, 0x2196, 0x2197, 1, 0x2000, 0x2000, 0, 0},  // 71
        {310, 120, 325, 145, 0x500, 0xA3, 0xA6, 0x2196, 0x2197, 1, 0x2000, 0x2000, 0, 0},  // 72
        {398, 120, 413, 145, 0x500, 0xA3, 0xA6, 0x2196, 0x2197, 1, 0x2000, 0x2000, 0, 0},  // 73
        {134, 180, 149, 205, 0x500, 0xA3, 0xA6, 0x2196, 0x2197, 1, 0x2000, 0x2000, 0, 0},  // 74
        {222, 180, 237, 205, 0x500, 0xA3, 0xA6, 0x2196, 0x2197, 1, 0x2000, 0x2000, 0, 0},  // 75
        {310, 180, 325, 205, 0x500, 0xA3, 0xA6, 0x2196, 0x2197, 1, 0x2000, 0x2000, 0, 0},  // 76
        {398, 180, 413, 205, 0x500, 0xA3, 0xA6, 0x2196, 0x2197, 1, 0x2000, 0x2000, 0, 0},  // 77
        {134, 240, 149, 265, 0x500, 0xA3, 0xA6, 0x2196, 0x2197, 1, 0x2000, 0x2000, 0, 0},  // 78
        {222, 240, 237, 265, 0x500, 0xA3, 0xA6, 0x2196, 0x2197, 1, 0x2000, 0x2000, 0, 0},  // 79
        {310, 240, 325, 265, 0x500, 0xA3, 0xA6, 0x2196, 0x2197, 1, 0x2000, 0x2000, 0, 0},  // 80
        {398, 240, 413, 265, 0x500, 0xA3, 0xA6, 0x2196, 0x2197, 1, 0x2000, 0x2000, 0, 0},  // 81
        {8, 2, 183, 21, 0x0A4, 0xDC, 0xDC, 0x0000, 0x0000, 1, 0x0000, 0x0000, 0, 0},  // 82
        {238, 354, 307, 391, 0x100, 0xA3, 0xA6, 0x21ED, 0x21EE, 1, 0x21EB, 0x21EB, 0, 0},  // 83
        {336, 354, 405, 391, 0x100, 0xA3, 0xA6, 0x21EF, 0x21F0, 1, 0x21EC, 0x21EC, 0, 0},  // 84
        {190, 2, 431, 21, 0x0A0, 0xD9, 0xDC, 0x0000, 0x0000, 1, 0x0000, 0x0000, 0, 0},  // 85
    });
}

// --- the video makers (segments 16, 17 and 26) ----------------------------------------------

int RockBach::videoMaker(int kind) {
    // f16_217c (1), f17_20f2 (2), f26_1edc (3). One of six choices picked
    // up (it follows the pointer) goes into the scene clicked next, with
    // that choice's default flags and (backgrounds, effects) its four
    // colour chips; a right click empties the scene under the pointer. The
    // markers under each scene are its flags. GO plays the song with the
    // scene playing lit; the tabs go to the other makers, the VCR button
    // to a preview.
    const bool camera = kind == 3;
    const uint16_t turns = kind == 1 ? kBgTurns : kind == 2 ? kFxTurns : kCamPlays;
    const uint16_t colours = kind == 1 ? kBgColours : kFxColours;
    const uint16_t choice = kind == 1 ? kBackground : kind == 2 ? kEffect : kCamera;
    const int labels = camera ? 56 : 80, nameLabel = camera ? 85 : 91;
    std::vector<Widget>& w = studioWidgets_;
    auto at = [](uint16_t field, int i) { return static_cast<uint16_t>(field + 2 * i); };
    auto member = [this](int i) { return static_cast<int>(static_cast<int8_t>(vbyte(static_cast<uint16_t>(kMembers + i)))); };
    auto event = [this]() {
        uint8_t e = 0;
        ctx_.platform.withFm([&e](ArtechFmDriver& d) { e = d.peek(d.getVar()); });
        return static_cast<int>(static_cast<int8_t>(e));
    };
    auto redraw = [&](int i) { drawWidget(w[i], w[i].flags & Widget::kPressed); };
    auto setToggle = [&](int i, bool on) {
        w[i].toggle = on;
        if (on) w[i].flags |= Widget::kPressed;
        else w[i].flags &= ~Widget::kPressed;
        redraw(i);
    };
    std::array<bool, 6> defaultA{}, defaultB{};  // [bp-1C], [bp-E] (f26); [bp-A0] (f16), [bp-9A] (f17)
    defaultA.fill(true);
    std::array<int, 64> chips{};                 // [bp-8E]: the scenes' colours as drawn
    int prompt = saveArea(0, 0, 2, 2);
    if (kind == 1) backgroundMakerWidgets();
    else if (kind == 2) effectMakerWidgets();
    else cameraMakerWidgets();
    select(2);
    blackout();
    studioBackdrop(0x1005);
    if (!camera) {
        fill(0xEE, 0x162, 0x46, 0x26, 0xC3);
        fill(0x150, 0x162, 0x46, 0x26, 0xC3);
    }
    for (int i = 0; i < 4; ++i) {
        const int m = member(i);
        if (m < 0) continue;
        const uint16_t face = static_cast<uint16_t>(0x200E + data_[0x4E + m]);
        w[6 + i].overlayUp = w[6 + i].overlayDown = face;
        if (camera) w[45 + i].overlayUp = w[45 + i].overlayDown = face;
        w[labels + i].label = dataString(static_cast<uint16_t>(0x194C + m * 11));
    }
    w[nameLabel].label = vstring(kVideoName);
    // "Background Mode", "Special Effect Mode", "Camera View Mode".
    w[camera ? 82 : 84].label = dataString(kind == 1 ? 0x1B04 : kind == 2 ? 0x1B19 : 0x1B2E);
    auto showScene = [&](int i) {
        // The scene's widget from its choice.
        const int c = vword(at(choice, i));
        Widget& s = w[12 + i];
        if (c < 0) return;
        if (kind == 1) {
            s.overlayUp = s.overlayDown = static_cast<uint16_t>(0x214F + c);
        } else if (kind == 2) {
            s.bitmapDown = s.bitmapUp = static_cast<uint16_t>(0x2198 + c);
        } else {
            s.bitmapDown = s.bitmapUp = w[44 + c].bitmapUp;
            s.overlayUp = w[44 + c].overlayUp;
            const int motion = vword(at(kCamMotion, i));
            s.overlayDown = motion > 0 ? w[0x52 + motion].overlayUp : 0x2000;
        }
    };
    for (int i = 0; i < 16; ++i) {
        if (static_cast<int8_t>(vbyte(static_cast<uint16_t>(turns + i))) > 0) w[28 + i].toggle = true, w[28 + i].flags |= Widget::kPressed;
        if (camera && static_cast<int8_t>(vbyte(static_cast<uint16_t>(kCamColours + i))) > 0)
            w[66 + i].toggle = true, w[66 + i].flags |= Widget::kPressed;
        showScene(i);
    }
    static std::mt19937 rng{std::random_device{}()};
    if (!camera)
        for (int i = 56; i < 80; ++i) w[i].faceDown = w[i].faceUp = static_cast<uint8_t>((rng() & 0x7FFF) % 15 + 0xF0);
    initWidgets(w, {0xC0, 0xD8, 0xDB, 0xDC});
    show(2);
    select(1);
    auto squares = [&](int i) {
        // The scene's four colours, small, in its corner.
        if (camera) return;
        for (int k = 0; k < 4; ++k)
            if (chips[i * 4 + k] >= 0) fill(w[12 + i].x0 + 5 + 14 * k, w[12 + i].y0 + 5, 12, 12, static_cast<uint8_t>(chips[i * 4 + k]));
    };
    for (int i = 0; i < 16 && !camera; ++i) {
        for (int k = 0; k < 4; ++k) {
            const int c = vword(static_cast<uint16_t>(colours + i * 8 + 2 * k));
            chips[i * 4 + k] = c < 0 ? -1 : c;
        }
        squares(i);
    }
    // Carrying a choice (its picture at the pointer's corner, kept on the
    // screen: f16_20e8, always with 0x42 x 0x30). x, y are where it is.
    struct Carry {
        bool on = false;
        uint16_t picture = 0xFFFF;
        int w = 0x42, h = 0x30, area = 0, x = -1, y = -1;
    } carry;
    carry.area = saveArea(0, 0, 2, 2);
    auto drawCarry = [&](int mx, int my) {
        if (mx + 0x42 >= Screen::kWidth) mx = Screen::kWidth - 0x42 - 1;
        if (my + 0x30 >= Screen::kHeight) my = Screen::kHeight - 0x30 - 1;
        if (my < 0) my = 0;
        restoreArea(carry.area);
        carry.area = saveArea(mx, my, carry.w, carry.h);
        drawLogo(mx, my, carry.picture);
        carry.x = mx, carry.y = my;
    };
    auto drop = [&] {
        restoreArea(carry.area);
        carry.area = saveArea(0, 0, 2, 2);
        carry.on = false;
    };
    int picked = -1, motion = 0;  // [bp-90] / [bp-2C], [bp-36]
    bool playing = false;         // [bp-94]
    int lit = 0xF;
    int flash = 0;                // [1F44]
    ctx_.countdown[1] = 5;        // (the original flips it every 13000 polls)
    int result = 0x7FFF;
    clearInput();
    while (result == 0x7FFF) {
        const int r = pollWidgets();
        const bool anyClick = r >= 0 || lastClick_.on || lastKey_ != 0;
        if (playing && ctx_.countdown[1] == 0) {
            ctx_.countdown[1] = 5;
            flash = flash ? 0 : 1;
            drawOpaque(0x1F8, 0xE4, static_cast<uint16_t>(0x22B3 + flash));
        }
        if (playing && lit != event()) {
            const int e = event();
            const bool was = carry.on;
            if (was) restoreArea(carry.area), carry.area = saveArea(0, 0, 2, 2);
            lit = e;
            if (e < 16) {
                drawLogo(w[lit + 12].x0, w[lit + 12].y0, 0x21E9);
            } else {
                musicStop();
                playing = false;
                w[10].flags &= ~Widget::kPressed;
                w[11].flags |= Widget::kPressed;
                redraw(10);
                redraw(11);
            }
            if (lit > 0) {
                redraw(lit + 11);
                squares(lit - 1);
            }
            if (was) drawCarry(carry.x, carry.y);
        }
        int rx, ry;
        if (ctx_.platform.takeRightClick(&rx, &ry)) {
            if (carry.on) drop();
            for (int i = 0; i < 16; ++i) {
                Widget& s = w[12 + i];
                if (rx < s.x0 || rx > s.x1 || ry < s.y0 || ry > s.y1) continue;
                s.bitmapDown = s.bitmapUp = 0x21AE;
                s.overlayUp = s.overlayDown = 0x2000;
                redraw(12 + i);
                setVword(at(choice, i), -1);
                vbyte(static_cast<uint16_t>(turns + i)) = 1;
                setToggle(28 + i, true);
                if (camera) {
                    vbyte(static_cast<uint16_t>(kCamColours + i)) = 0;
                    setVword(at(kCamMotion, i), 0);
                    setToggle(66 + i, false);
                } else {
                    for (int k = 0; k < 4; ++k) {
                        setVword(static_cast<uint16_t>(colours + i * 8 + 2 * k), -1);
                        chips[i * 4 + k] = -1;
                    }
                }
                break;
            }
        }
        int mx, my;
        bool down;
        ctx_.platform.mouse(&mx, &my, &down);
        // It follows the pointer's x only: the original's check of y
        // compares [bp-C] with itself.
        if (carry.on && carry.picture != 0xFFFF && mx != carry.x) drawCarry(mx, my);
        if (carry.on && anyClick) drop();
        if (r == 0) {
            result = -0x42;
            redraw(0);
            if (!vbyte(kVideoNamed)) {
                vbyte(kVideoNamed) = 1;
                sound(0x6025);
                edisonSays(&prompt, dataString(0x1EF0), true, 2);
                if (vstring(kVideoName).empty()) setVstring(kVideoName, dataString(0x1F45));
            }
        }
        if (r >= 1 && r <= 3) {
            redraw(r);
            if (r != kind) result = r;
        }
        if (r == 4) {
            redraw(4);
            redraw(5);
        }
        if (r == 5) {
            // The VCR's lever (22B5-22B9), then the preview.
            static const int kLever[7] = {1, 2, 5, 4, 3, 2, 1};
            redraw(5);
            redraw(4);
            for (int f : kLever) {
                drawOpaque(0x10, 0xDC, static_cast<uint16_t>(0x22B4 + f));
                waitCountdown(3);
            }
            result = 10 + kind;
        }
        if (r == 10 && !playing) {
            playing = true;
            lit = 0xF;
            int song[16][2];
            for (int i = 0; i < 16; ++i) song[i][0] = vword(static_cast<uint16_t>(kSong + 4 * i)), song[i][1] = vword(static_cast<uint16_t>(kSong + 2 + 4 * i));
            setSong(song);
            tempo_ = static_cast<uint8_t>(vword(kTempo));
            musicPlay();
            ctx_.platform.withFm([](ArtechFmDriver& d) { d.poke(d.getVar(), 0); });
            redraw(10);
            redraw(11);
        }
        if (r == 11) {
            playing = false;
            musicStop();
            const int e = std::clamp(event(), 0, 16);
            redraw(e + 12);
            redraw(11);
            redraw(10);
            if (e < 16) squares(e);
        }
        if (r >= 12 && r <= 27) {
            const int i = r - 12;
            if (!camera && picked >= 0) {
                setVword(at(choice, i), picked);
                vbyte(static_cast<uint16_t>(turns + i)) = defaultA[picked];
                showScene(i);
                redraw(r);
                setToggle(r + 16, defaultA[picked]);
                for (int k = 0; k < 4; ++k) {
                    const int c = w[0x38 + picked * 4 + k].faceDown;
                    setVword(static_cast<uint16_t>(colours + i * 8 + 2 * k), c);
                    chips[i * 4 + k] = c;
                }
            } else if (camera && picked < 0 && motion > 0) {
                setVword(at(kCamMotion, i), motion);
                showScene(i);
                redraw(r);
            } else if (camera && picked >= 0) {
                setVword(at(choice, i), picked);
                vbyte(static_cast<uint16_t>(kCamPlays + i)) = defaultA[picked];
                vbyte(static_cast<uint16_t>(kCamColours + i)) = defaultB[picked];
                setVword(at(kCamMotion, i), 0);
                showScene(i);
                redraw(r);
                setToggle(r + 16, defaultA[picked]);
                setToggle(r + 54, defaultB[picked]);
            }
            if (playing && event() == i) drawLogo(w[r].x0, w[r].y0, 0x21E9);
            squares(i);
            picked = -1;
            motion = 0;
            carry.picture = 0xFFFF;
        }
        if (r >= 28 && r <= 43) {
            uint8_t& f = vbyte(static_cast<uint16_t>(turns + r - 28));
            f = f >= 1 ? 0 : 1;
            redraw(r);
        }
        if (r >= 50 && r <= 55) {
            defaultA[r - 50] = !defaultA[r - 50];
            redraw(r);
        }
        if (camera && r >= 60 && r <= 65) {
            defaultB[r - 60] = !defaultB[r - 60];
            redraw(r);
        }
        if (camera && r >= 66 && r <= 81) {
            uint8_t& f = vbyte(static_cast<uint16_t>(kCamColours + r - 66));
            f = f >= 1 ? 0 : 1;
            redraw(r);
        }
        if (!camera && r >= 56 && r <= 79) {
            // A chip: its next colour (F0-FE).
            uint8_t c = static_cast<uint8_t>(w[r].faceDown + 1);
            if (c == 0xFF || c == 0) c = 0xF0;
            w[r].faceDown = w[r].faceUp = c;
            redraw(r);
        }
        if (!camera && r >= 85 && r <= 90) {
            // A choice's four chips at once (no wrap: as the original).
            const int base = 0x38 + (r - 0x55) * 4;
            w[base].faceDown = w[base].faceUp = static_cast<uint8_t>(w[base].faceDown + 1);
            redraw(base);
            for (int j = base + 1; j <= base + 3; ++j) {
                w[j].faceDown = w[j].faceUp = w[base].faceDown;
                redraw(j);
            }
        }
        if ((r >= 44 && r <= 49) || (camera && (r == 83 || r == 84))) {
            if (r <= 49) {
                picked = r - 44;
                motion = 0;
            } else {
                picked = -1;
                motion = r - 0x52;
            }
            restoreArea(carry.area);
            carry = Carry{true, camera ? w[r].overlayDown : w[r].bitmapUp, 0x42, r <= 49 ? 0x30 : 0x22, saveArea(0, 0, 2, 2)};
            drawCarry(mx, my);
        }
        if (r == nameLabel) {
            vbyte(kVideoNamed) = 1;
            sound(0x6025);
            edisonSays(&prompt, dataString(0x1EF0), true, 2);
            if (vstring(kVideoName).empty()) setVstring(kVideoName, dataString(0x1F4B));
            w[nameLabel].label = vstring(kVideoName);
            redraw(nameLabel);
        }
    }
    musicStop();
    freeArea(carry.area);
    freeArea(prompt);
    return result;
}

// --- the Studio (segment 35) ----------------------------------------------------------

int RockBach::studioSong() {
    // f35_0000: the band maker (once for each new video), then the song
    // maker, which can go back to the band.
    if (!bandMade_) {
        bandMaker();
        bandMade_ = true;
    }
    int r;
    while ((r = songMaker()) == -99) bandMaker();
    videoChanged_ = true;
    return r;
}

int RockBach::studioVideo() {
    // f35_005a: the camera maker first, then whichever the tabs ask for;
    // a preview plays in the makers' picture (8 right, 1 up), black first.
    int r = videoMaker(3);
    while (r > 0) {
        if (r >= 1 && r <= 3) r = videoMaker(r);
        if (r >= 11 && r <= 13) {
            videoX_ += 8;
            videoY_ -= 1;
            fill(videoX_, videoY_, 0x160, 0xF8, 0);
            playVideo(r - 10);
            r -= 10;
            videoX_ -= 8;
            videoY_ += 1;
        }
    }
    videoChanged_ = true;
    return r;
}

int RockBach::studio() {
    // f35_018a.
    select(1);
    bool edit = false;
    videoOpen_ = false;
    videoChanged_ = false;
    videoBlank(false);
    blackout();
    studioBackdrop(0x1004);
    show(2);
    for (int r = -1; r != 0;) {
        r = studioRoom(edit);
        edit = false;
        if (r == 2) {
            studioSong();
            videoOpen_ = true;
            edit = true;
        }
        if (r == 5) {
            studioVideo();
            edit = true;
        }
        if (r == 6 && videoOpen_) playVideo(0);
    }
    musicStop();  // f20_004a
    return 0;
}

}  // namespace edison
