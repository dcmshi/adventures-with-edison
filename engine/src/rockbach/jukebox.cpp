// WINMAIN's segment 3 (the jukebox: pick a band of four, a song, the tempo,
// each player's volume and the stage lights) and segment 9 (its music: each
// role's riffs for the song, in ADLIB.DLL).

#include <algorithm>
#include <array>

#include "rockbach/rockbach.h"

#include "audio/artech_fm_driver.h"

namespace edison {
namespace {

// The stage: its backdrop (212A) at (186, 42), 270 x 178.
constexpr int kStageX = 0xBA, kStageY = 0x2A, kStageW = 0x10E, kStageH = 0xB2;
constexpr int kJukeboxSlot = 7;  // the timer slot (g03_1770, 8 a second)

}  // namespace

// --- the music (segment 9) ------------------------------------------------------

void RockBach::bandCommand(const std::vector<uint8_t>& bytes) {
    // f09_036e: the bytes over sound 1, which is then started.
    if (!options_.music) return;
    ctx_.platform.withFm([&bytes](ArtechFmDriver& d) {
        const uint16_t to = d.soundAddress(1);  // f09_03a8 / f09_0404 (GETADDR)
        for (size_t k = 0; k < bytes.size(); ++k) d.poke(static_cast<uint16_t>(to + k), bytes[k]);
        d.sendSound(1);
    });
}

void RockBach::bandReset() {
    // f09_0000: song 0, every role silent (the empty part at DS:1274),
    // tempo A0.
    band_.song = 0;
    band_.volume.fill(0);
    band_.part.fill(0x1274);
    band_.tempo = 0;
    bandTempo(0xA0);
}

void RockBach::bandStop() {
    // f09_0220: sound 0, the event queue flushed; then 0.1 s.
    if (!options_.music) return;
    ctx_.platform.withFm([](ArtechFmDriver& d) {
        d.sendSound(0);
        d.flushEvents();
    });
    ctx_.countdown[3] = 1;
    while (ctx_.countdown[3] != 0) ctx_.pump();
}

void RockBach::bandStart() {
    // f09_00fc: each role's riffs for the song (its part's record: a count,
    // the FM channels at +4, 8 songs of 3 sounds at +7), then the
    // volumes, 0.1 s apart, and the tempo.
    if (!options_.music) return;
    ctx_.platform.withFm([this](ArtechFmDriver& d) {
        d.flushEvents();
        for (int role = 0; role < 4; ++role) {
            const uint16_t part = band_.part[role];
            for (int i = 0; i < dataWord(part); ++i)
                d.sendSound(data_[part + 7 + band_.song * 3 + i]);
        }
    });
    for (int role : {0, 3, 2, 1}) {
        bandVolume(role, band_.volume[role]);
        ctx_.countdown[3] = 1;
        while (ctx_.countdown[3] != 0) ctx_.pump();
    }
    bandTempo(band_.tempo);
}

void RockBach::bandVolume(int role, int volume) {
    // f09_0262: "AC channel volume" for each of the part's channels.
    band_.volume[role] = static_cast<uint8_t>(volume);
    if (!options_.music) return;
    const uint16_t part = band_.part[role];
    std::vector<uint8_t> bytes = {0x09, 0x00};
    for (int i = 0; i < dataWord(part); ++i)
        bytes.insert(bytes.end(), {0xAC, data_[part + 4 + i], static_cast<uint8_t>(volume)});
    bytes.push_back(0x88);
    bandCommand(bytes);
}

void RockBach::bandTempo(int tempo) {
    // f09_0320: "A6 tempo".
    band_.tempo = static_cast<uint8_t>(tempo);
    bandCommand({0x09, 0x00, 0xA6, static_cast<uint8_t>(tempo), 0x88});
}

void RockBach::bandPart(int role, int choice) {
    // f09_00bc: the role's part for the member chosen (DS:1494: 4 roles of
    // 4 words).
    if (choice >= 0) band_.part[role] = dataWord(static_cast<uint16_t>(0x1494 + role * 8 + choice * 2));
}

// --- the jukebox (segment 3) ------------------------------------------------------

void RockBach::jukeboxWidgets() {
    // f03_006c: 64 widgets.
    auto w = [](int x0, int y0, int x1, int y1, uint16_t flags, uint16_t down, uint16_t up, uint16_t ovUp = 0x2000,
                uint16_t ovDown = 0x2000, int group = 0, char mode = 0) {
        Widget r{x0, y0, x1, y1, flags};
        r.group = group;
        r.bitmapDown = down;
        r.bitmapUp = up;
        r.overlayUp = ovUp;
        r.overlayDown = ovDown;
        r.mode = mode;
        return r;
    };
    std::vector<Widget>& l = jukeboxWidgets_;
    l.clear();
    // 0-15: the members to pick, four columns (drums, chords, bass, solo).
    static const int kColumnX[4] = {20, 92, 480, 552}, kGroup[4] = {2, 4, 1, 3};
    static const uint16_t kFaces[4][2] = {{0x2004, 0x2003}, {0x2008, 0x2007}, {0x2002, 0x2001}, {0x2006, 0x2005}};
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r)
            l.push_back(w(kColumnX[c], 34 + 54 * r, kColumnX[c] + 69, 85 + 54 * r, 0x140, kFaces[r][0], kFaces[r][1],
                          0, 0, kGroup[c]));
    l.push_back(w(570, 348, 627, 393, 0x100, 0x200D, 0x200E));                 // 16 exit
    l.push_back(w(186, 226, 245, 255, 0x140, 0x2009, 0x200A, 0x2000, 0x2000, 5));  // 17 go
    l.push_back(w(376, 226, 455, 255, 0x142, 0x200B, 0x200C, 0x2000, 0x2000, 5));  // 18 stop
    for (int i = 0; i < 4; ++i)                                                 // 19-22 the band
        l.push_back(w(178 + 72 * i, 288, 247 + 72 * i, 339, 0x104, 0x2001 + 2 * i, 0x2001 + 2 * i));
    for (int i = 0; i < 12; ++i) {                                              // 23-34 the songs
        const int x = 22 + 36 * (i % 4), y = 306 + 42 * (i / 4);
        l.push_back(w(x, y, x + 33, y + 33, i == 0 ? 0x146 : i < 8 ? 0x144 : 0x344, 0x2057 + i, 0x2063 + i, 0x2000,
                      0x2000, 6, 'c'));
    }
    for (int i = 0; i < 12; ++i) {                                              // 35-46 the lights
        const int x = 480 + 38 * (i % 4), y = 268 + 26 * (i / 4);
        l.push_back(w(x, y, x + 31, y + 19, i < 4 ? 0x100 : 0x500, 0x2124, 0x2125));
    }
    for (int i = 0; i < 8; ++i) {                                               // 47-54 volume, brightness
        const int x = (i < 4 ? 190 : 224) + 72 * (i % 4);
        Widget s = w(x, 346, x + 11, 391, 0x110, 0x2128, 0x2128, i < 4 ? 0x2126 : 0x2127, i < 4 ? 0x2126 : 0x2127);
        s.slider = &jukeboxSliders_[i];
        l.push_back(s);
    }
    for (int i = 0; i < 4; ++i)                                                 // 55-58 the names
        l.push_back(w(176 + 72 * i, 266, 245 + 72 * i, 283, 0x124, 0x2129, 0x2129));
    Widget tempo = w(252, 12, 383, 30, 0x108, 0x212B, 0x212B, 0x212D, 0x212D);  // 59 tempo
    tempo.slider = &jukeboxSliders_[8];
    l.push_back(tempo);
    l.push_back(w(250, 226, 371, 257, 0x104, 0x206F, 0x206F));                 // 60 the song's name
    l.push_back(w(436, 200, 455, 219, 0x300, 0x21C1, 0x21C2));                 // 61
    l.push_back(w(188, 6, 245, 35, 0x104, 0x2366, 0x2366, 0x214A, 0x214B, 0, 'c'));  // 62 slower
    l.push_back(w(390, 4, 455, 37, 0x104, 0x214E, 0x214E, 0x214C, 0x214D, 0, 'c'));  // 63 faster
    for (Widget& x : l) x.hotkey = 1;
}

int RockBach::jukebox() {
    // f03_22e8.
    static const uint8_t kMembers[16] = {1, 2, 6, 5, 0xA, 9, 0xF, 0xC, 0x13, 0x12, 0x18, 0x16, 0x1C, 0x21, 0x1B, 0x1F};
    static const uint16_t kNames[16] = {0x48C, 0x498, 0x4A1, 0x4AE, 0x4BE, 0x4CD, 0x4DB, 0x4EB,
                                        0x4FB, 0x506, 0x511, 0x51F, 0x52D, 0x53A, 0x546, 0x554};
    static const uint16_t kSongs[8] = {0x3DD, 0x3EF, 0x3FF, 0x411, 0x427, 0x43F, 0x45A, 0x476};
    static const int kLightOf[4] = {2, 0, 3, 1};  // the brightness sliders' lights
    // Where a click on the stage makes each role's member play.
    static const int kStage[4][4] = {{0x10C, 0x2B, 0x177, 0x79}, {0xBB, 0x2B, 0x10A, 0xDB},
                                     {0x179, 0x2B, 0x1C7, 0xDB}, {0x10C, 0x7C, 0x177, 0xDB}};
    bool playing = false;                    // [bp-F0]
    std::array<bool, 4> active{}, soloing{};  // [bp-E]: the part plays; [bp-6]: clicked
    std::array<int, 4> lightColour{};        // [bp-A]
    std::array<std::array<std::array<int, 11>, 4>, 3> flash{};  // [bp-EE]: f03_17a6's steps
    int partsPlaying = 0;                   // [83CE]
    bool songEnded = false;                  // [72]

    jukeboxWidgets();
    Widget& title = jukeboxWidgets_[60];
    title.flags |= Widget::kLabel;
    title.label = dataString(kSongs[0]);
    blackout();
    backdrop(0x1000);
    show(2);
    select(2);

    // f03_1e9a: the pictures of the members to pick; the first of each
    // column picked; sliders at the top; tempo 3A (B4).
    for (int i = 0; i < 16; ++i) {
        jukeboxWidgets_[i].overlayUp = static_cast<uint16_t>(0x200F + kMembers[i]);
        jukeboxWidgets_[i].overlayDown = static_cast<uint16_t>(0x2033 + kMembers[i]);
    }
    for (int i = 0; i < 8; ++i)  // f03_1d1a
        jukeboxSliders_[i] = Slider{0, 0, i < 4 ? 0x29 : 0xFF, 0xC, 8, 8, 2, 0x2E};
    jukeboxSliders_[8] = Slider{0, 0, 0x96, 0xF, 0x13, 6, 4, 0x84};
    bandTempo(0xB4);
    jukeboxSliders_[8].value = 0x3A;
    for (auto& a : flash)
        for (auto& b : a) b.fill(-1);
    for (int role = 0; role < 4; ++role) {
        jukeboxWidgets_[role * 4].flags |= Widget::kPressed;
        band_.members[role] = kMembers[role * 4];
        bandVolume(role, 0);
        jukeboxWidgets_[55 + role].label = dataString(kNames[role * 4]);
        Widget& face = jukeboxWidgets_[19 + role];
        face.overlayUp = face.overlayDown = static_cast<uint16_t>(0x200F + band_.members[role]);
    }
    auto memberX = [this](int m) { return static_cast<int16_t>(dataWord(static_cast<uint16_t>(0x78 + 8 * m))); };
    auto memberY = [this](int m) { return static_cast<int16_t>(dataWord(static_cast<uint16_t>(0x7A + 8 * m))); };
    auto memberSprite = [this](int m, int frame) {
        return static_cast<uint16_t>(0x206F + data_[0x194 + m * 16 + frame] + m * 5);
    };
    for (int role = 0; role < 4; ++role) {
        const int m = band_.members[role];
        drawLogo(memberX(m), memberY(m), memberSprite(m, 0));
        memberFrame_[m] = 0;
        bandPart(role, 0);
    }
    initWidgets(jukeboxWidgets_, {0xFB, 0xFC, 0xFD, 0xFE});
    copyArea(2, 1, 0, 0, Screen::kWidth, Screen::kHeight);
    select(2);
    drawOpaque(kStageX, kStageY, 0x212A);  // screen 2 keeps the empty stage
    select(1);

    bool due = false, chase = false, pulse = false;  // [8E1E], [8DD6], [52A6]
    ctx_.timer.setPeriodic(kJukeboxSlot, 8, [&] { due = chase = pulse = true; });  // g03_1770
    clearInput();

    auto stopBand = [&] {  // f03_0000
        jukeboxWidgets_[17].flags &= ~Widget::kPressed;
        jukeboxWidgets_[18].flags |= Widget::kPressed;
        bandStop();
        playing = false;
        drawWidget(jukeboxWidgets_[17], false);
        drawWidget(jukeboxWidgets_[18], true);
        active.fill(false);
        soloing.fill(false);
    };
    auto stageFrame = [&] {  // f03_1b58
        select(2);
        drawOpaque(kStageX, kStageY, 0x212A);
        for (int role = 0; role < 4; ++role) {
            const int m = band_.members[role];
            int frame = active[role] || soloing[role] ? memberFrame_[m] + 1 : 0;
            if (memberPlaying_[m]) {
                if (data_[0x194 + m * 16 + frame] == 0xFE) {
                    memberPlaying_[m] = false;
                    frame = 0;
                    soloing[role] = false;
                }
            } else if (frame >= 2) {
                frame = 0;
            }
            memberFrame_[m] = frame;
            ctx_.screens.drawSprite(2, ctx_.bitmap(memberSprite(m, frame)), memberX(m), memberY(m));
        }
        copyArea(2, 1, kStageX, kStageY, kStageW, kStageH);
        drawOpaque(kStageX, kStageY, 0x212A);
        select(1);
    };
    auto stopMembers = [&] {
        for (int role = 0; role < 4; ++role) {
            memberPlaying_[band_.members[role]] = false;
            memberFrame_[band_.members[role]] = 0;
        }
    };
    auto displayColours = [this](int first, int count) {
        return std::vector<Rgb>(ctx_.displayPalette.begin() + first, ctx_.displayPalette.begin() + first + count);
    };

    for (bool done = false; !done;) {
        int r = pollWidgets();
        // A click on the stage while the band plays: that member's solo
        // (unless one is on already), to the crowd.
        if (playing && r < 0 && lastClick_.on && lastClick_.x >= kStageX && lastClick_.x <= 0x1C8 &&
            lastClick_.y >= kStageY && lastClick_.y <= 0xDC) {
            bool any = false;
            for (int role = 0; role < 4; ++role) any = any || memberPlaying_[band_.members[role]];
            for (int role = 0; role < 4 && !any; ++role) {
                const int* s = kStage[role];
                if (lastClick_.x >= s[0] && lastClick_.x <= s[2] && lastClick_.y >= s[1] && lastClick_.y <= s[3]) {
                    memberPlaying_[band_.members[role]] = true;
                    soloing[role] = true;
                    sound(0x6000);
                    break;
                }
            }
        }
        if (playing && due) {
            // f03_1ade: the driver's event (role << 4 | 1 starts, | 2 stops);
            // the song is over when no part plays.
            uint8_t event = 0;
            if (options_.music) ctx_.platform.withFm([&event](ArtechFmDriver& d) { event = d.nextEvent(); });
            const int role = event >> 4 & 3;
            if ((event & 0xF) == 1) active[role] = true, ++partsPlaying;
            if ((event & 0xF) == 2) active[role] = false, --partsPlaying;
            if (partsPlaying <= 0 && options_.music) songEnded = true;
            stageFrame();
            due = false;
        }
        if (playing && songEnded) {
            // Again from the top.
            stopMembers();
            bandStart();
            partsPlaying = 0;
            songEnded = false;
        }
        if (chase) {
            // f03_1a22: a light's 11 colours turn one step.
            for (int i = 0; i < 4; ++i)
                if (jukeboxWidgets_[39 + i].toggle) {
                    const int first = i * 16 + 5;
                    const std::vector<Rgb> ten = displayColours(first, 10), last = displayColours(first + 10, 1);
                    setColours(ten, first + 1);
                    setColours(last, first);
                }
            chase = false;
        }
        if (pulse) {
            // f03_17a6: each component of a light's colours one step up or
            // down, turning at 0 and 255.
            for (int i = 0; i < 4; ++i)
                if (jukeboxWidgets_[43 + i].toggle) {
                    const int first = i * 16 + 5;
                    std::vector<Rgb> c = displayColours(first, 11);
                    for (int k = 0; k < 11; ++k) {
                        uint8_t* comp[3] = {&c[k].r, &c[k].g, &c[k].b};
                        for (int j = 0; j < 3; ++j) {
                            int& step = flash[j][i][k];
                            int v = *comp[j] + step;
                            if (v >= 0xFF) v = 0xFF, step = -1;
                            if (v <= 0) v = 0, step = 1;
                            *comp[j] = static_cast<uint8_t>(v);
                        }
                    }
                    setColours(c, first);
                }
            pulse = false;
        }
        if (r < 0) continue;
        if (r == 16) done = true;
        if (r <= 15) {
            // A member picked: the name and picture below the stage, the
            // stage redrawn, the band stopped.
            const int role = r / 4;
            band_.members[role] = kMembers[r];
            Widget& name = jukeboxWidgets_[55 + role];
            name.label = dataString(kNames[r]);
            drawWidget(name, false);
            Widget& face = jukeboxWidgets_[19 + role];
            face.overlayUp = face.overlayDown = static_cast<uint16_t>(0x200F + kMembers[r]);
            drawWidget(face, face.flags & Widget::kPressed);
            select(2);
            drawOpaque(kStageX, kStageY, 0x212A);
            for (int k = 0; k < 4; ++k) {
                const int m = band_.members[k];
                ctx_.screens.drawSprite(2, ctx_.bitmap(memberSprite(m, 0)), memberX(m), memberY(m));
            }
            select(1);
            copyArea(2, 1, kStageX, kStageY, kStageW, kStageH);
            memberFrame_[kMembers[r]] = 0;
            stopBand();
            bandPart(role, r % 4);
        }
        if (r == 17 && !playing) {
            stopMembers();
            playing = true;
            bandStart();
            partsPlaying = 0;
            songEnded = false;
        }
        if (r == 18) {
            playing = false;
            bandStop();
            active.fill(false);
            soloing.fill(false);
        }
        if (r >= 35 && r <= 38) {
            // A light's next colours (of 4, at 45 + 16n); its brightness back
            // to full.
            const int i = r - 35;
            lightColour[i] = lightColour[i] == 3 ? 0 : lightColour[i] + 1;
            setColours(displayColours(lightColour[i] * 16 + 0x45, 11), i * 16 + 5);  // f03_1a8c
            jukeboxSliders_[4 + i].value = 0;
            placeSlider(jukeboxWidgets_[r + 16], true);
        }
        if (r == 59) bandTempo(jukeboxSliders_[8].value + 0x7A);
        if (r >= 51 && r <= 54) {
            // f44_0000: the light's colours at (256 - value) / 256.
            const int light = kLightOf[r - 51];
            const int scale = 0x100 - jukeboxSliders_[r - 47].value;
            std::vector<Rgb> c = displayColours(lightColour[light] * 16 + 0x45, 11);
            if (scale < 0x100)
                for (Rgb& x : c)
                    x = Rgb{static_cast<uint8_t>(x.r * scale >> 8), static_cast<uint8_t>(x.g * scale >> 8),
                            static_cast<uint8_t>(x.b * scale >> 8)};
            setColours(c, light * 16 + 5);
        }
        if (r >= 47 && r <= 50) bandVolume(r - 47, jukeboxSliders_[r - 47].value);
        if (r >= 23 && r <= 30) {
            stopBand();
            title.label = dataString(kSongs[r - 23]);
            drawWidget(title, true);
            band_.song = r - 23;  // f09_0096
        }
        if (r == 62 || r == 63) {
            Slider& t = jukeboxSliders_[8];
            t.value = std::clamp(t.value + (r == 62 ? -8 : 8), 0, 0x85);
            placeSlider(jukeboxWidgets_[59], true);
            bandTempo(t.value + 0x7A);
        }
        clearInput();
    }
    memberPlaying_.fill(false);
    memberFrame_.fill(0);
    ctx_.timer.setPeriodic(kJukeboxSlot, 0, nullptr);
    return 0;
}

}  // namespace edison
