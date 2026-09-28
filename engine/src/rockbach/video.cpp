// WINMAIN's segment 18: the Studio's video player. A video is 16 scenes,
// one for each of its song's slots: a background (two halves of a picture,
// or a colour), a special effect drawn over it, and a camera view of the
// band (which members, how big, still, bouncing or spinning). Each scene's
// colours become ramps in palette entries 1-3F (the background) and 40-7F
// (the effect), which turn while it plays.

#include <algorithm>
#include <random>

#include "rockbach/rockbach.h"

#include "audio/artech_fm_driver.h"

namespace edison {
namespace {

constexpr int kW = 0x160, kH = 0xF8;       // the picture
constexpr int kDrawSlot = 7, kTickSlot = 8;  // g18_2286 (15 a second), g18_2236 (10 a second)

int rnd() {
    // f36_0ec2: rand().
    static std::mt19937 rng{std::random_device{}()};
    return static_cast<int>(rng() & 0x7FFF);
}

}  // namespace

int RockBach::sine(int a) const {
    // f53_103b: a quarter wave of 2048 words (0-FFFF), mirrored and
    // negated for the rest of the turn, then / 16.
    const unsigned u = static_cast<unsigned>(a) & 0xFFFF;
    unsigned at = (u >> 2) & 0xFFE;
    if (u & 0x4000) at = 0xFFE - at;
    const int v = at + 1 < sine_.size() ? (sine_[at] | sine_[at + 1] << 8) >> 4 : 0;
    return u & 0x8000 ? -v : v;
}

void RockBach::videoColours(const std::vector<Rgb>& colours, int first) {
    // f18_0000: into screen 1's palette and the display, and screens 2 and
    // 3 take screen 1's.
    setColours(colours, first);
    ctx_.screens[2].palette = ctx_.screens[3].palette = ctx_.screens[1].palette;
}

void RockBach::videoTurn(int first, int count) {
    // f18_0ab8 (1-3F) / f18_09c0 (40-7F): the colours one step round.
    const Palette& p = ctx_.screens[1].palette;
    std::vector<Rgb> c(p.begin() + first + 1, p.begin() + first + count);
    c.push_back(p[first]);
    videoColours(c, first);
}

void RockBach::videoRamps(int scene, uint16_t colours, int first) {
    // f18_0520 (the background's, 67AB, into 1-3F) / f18_0778 (the
    // effect's, 685B, into 40-7F): each of the scene's four colours makes
    // 16 shades of itself, darker by sixteenths (up, then down, then up...).
    std::vector<Rgb> ramp(64);
    for (int k = 0; k < 4; ++k) {
        const Rgb c = ctx_.screens[1].palette[vword(static_cast<uint16_t>(colours + scene * 8 + 2 * k)) & 0xFF];
        for (int j = 0; j < 16; ++j) {
            const int step = k % 2 ? 15 - j : j;
            ramp[k * 16 + j] = Rgb{static_cast<uint8_t>(c.r - step * (c.r / 16)), static_cast<uint8_t>(c.g - step * (c.g / 16)),
                                   static_cast<uint8_t>(c.b - step * (c.b / 16))};
        }
    }
    if (first == 1) videoColours(std::vector<Rgb>(ramp.begin() + 1, ramp.end()), 1);
    else videoColours(ramp, first);
}

void RockBach::videoMeter() {
    // f18_00d0: the VCR's level meter, 17 bars, rising while a sound
    // plays and falling otherwise.
    static const int kBars[17][2] = {{0xEE, 0x14C}, {0xF6, 0x14C}, {0xFE, 0x14C}, {0x106, 0x14C}, {0x10E, 0x14D}, {0x116, 0x14E},
                                     {0x11E, 0x14F}, {0x126, 0x150}, {0x12E, 0x151}, {0x136, 0x152}, {0x13E, 0x153}, {0x146, 0x154},
                                     {0x14E, 0x155}, {0x156, 0x156}, {0x15E, 0x157}, {0x166, 0x158}, {0x16E, 0x159}};
    int& m = vid_.meter;
    if (ctx_.platform.wavPlaying()) {
        if (++m >= 0x11) m = rnd() % 3 + 0xE;
    } else {
        m -= 2;
        if (m < 4) m = rnd() % 4 + 2;
    }
    for (int i = 0; i < 17; ++i) fill(kBars[i][0], kBars[i][1], 2, 0x15B - kBars[i][1] + 1, i < m ? 0xF0 : 0);
}

void RockBach::videoSign() {
    // f18_03f8: Edison watching from his chair (2211, blinking 2212/2213;
    // now and then a laugh, 6001 or 6002) and the tape turning (2214/2215).
    select(2);
    restoreArea(signArea_);
    signArea_ = saveArea(0x22, 0xF4, 0x5C, 0x96);
    drawLogo(0x22, 0xF4, 0x2211);
    int f = rnd() % 5;
    if (f > 1) f = 0;
    drawLogo(0x22, 0xF4, static_cast<uint16_t>(0x2212 + f));
    if (f == 1 && rnd() % 50 == 0) sound(rnd() % 2 == 0 ? 0x6001 : 0x6002);
    if (++vid_.tape > 1) vid_.tape = 0;
    drawLogo(99, 0x152, static_cast<uint16_t>(0x2214 + vid_.tape));
    copyArea(2, 1, 0x22, 0xF4, 0x5C, 0x96);
    select(1);
}

void RockBach::videoBackground(int scene, int mode) {
    // f18_1f78: the scene's background (only when it changes, or after a
    // bouncing scene), or black with a word for the previews without one.
    const int bg = vword(static_cast<uint16_t>(kBackground + 2 * scene));
    restoreArea(vid_.clean);
    auto centred = [&](uint16_t s) {
        const std::string t = dataString(s);
        font_->draw(ctx_.screens[current()], videoX_ + 0xB0 - font_->width(t) / 2, videoY_ + 10, t, 0xFF);
    };
    if (bg < 0 && (mode == 0 || mode == 1)) {
        fill(videoX_, videoY_, kW, kH, 0);
        if (mode == 1) centred(0x201A);  // "No BACKGROUND selected for this slot"
    } else if (mode < 2) {
        if (scene < 1 || bg != vword(static_cast<uint16_t>(kBackground + 2 * (scene - 1))) ||
            vword(static_cast<uint16_t>(kCamMotion + 2 * (scene - 1))) == 1) {
            if (bg == 5) {
                fill(videoX_, videoY_, kW, kH, 1);
            } else {
                drawOpaque(videoX_, videoY_, static_cast<uint16_t>(0x2165 + bg));
                drawOpaque(videoX_ + 0xB0, videoY_, static_cast<uint16_t>(0x216A + bg));
            }
        }
    } else {
        fill(videoX_, videoY_, kW, kH, 0);
        if (mode == 3 && vword(static_cast<uint16_t>(kCamera + 2 * scene)) < 0) centred(0x2066);  // "No CAMERA VIEW ..."
    }
    vid_.clean = saveArea(videoX_, videoY_, kW, kH);
    vid_.bgTurns = vbyte(static_cast<uint16_t>(kBgTurns + scene)) != 0;
}

void RockBach::videoEffectStart(int scene) {
    // f18_0252: where the fireworks start, the bouncing members' speeds,
    // where the walker walks.
    const int x = rnd() % 0x12E + videoX_, y = rnd() % 0xC6 + videoY_;
    vid_.sparkX.fill(x + 0x19);
    vid_.sparkY.fill(y + 0x19);
    vid_.sparkTick = 0;
    vid_.sparkEvery = rnd() % 4 + 3;
    for (int i = 0; i < 4; ++i) {
        vid_.vx[i] = rnd() % 20 - 10;
        vid_.vy[i] = rnd() % 20 - 10;
    }
    vid_.flash = -1;
    const Bitmap& w = ctx_.bitmap(0x2218);
    vid_.walkerX = rnd() % (kW - w.width) + videoX_ + w.width / 2;
    vid_.walkerY = videoY_ + kH - w.height;
    freeArea(vid_.walkerArea);
    vid_.walkerArea = saveArea(vid_.walkerX, vid_.walkerY, w.width, w.height);
    vid_.walker = 0;
    vid_.fxTurns = vbyte(static_cast<uint16_t>(kFxTurns + scene)) != 0;
}

void RockBach::videoCamera(int scene, int mode) {
    // f18_1c00: the camera view picks the members' places and size (all
    // four, or one at twice the size); its first flag starts one playing
    // (with the crowd), its second puts the members' and Edison's colours
    // back.
    static const int kScale[6] = {0x100, 0x200, 0x200, 0x200, 0x200, 0x100};
    static const int kQuad[4][2] = {{0xD6, 0x5A}, {0x186, 0x5A}, {0xD6, 0xD6}, {0x186, 0xD6}};
    auto hide = [&] {
        for (int i = 0; i < 4; ++i) {
            setVword(static_cast<uint16_t>(kCamPlaces + 4 * i), 0);
            setVword(static_cast<uint16_t>(kCamPlaces + 2 + 4 * i), 0);
            vid_.shown[i] = false;
        }
        vid_.scale = 0x100;
    };
    if (mode != 0 && mode != 3) {
        hide();
        return;
    }
    if (vbyte(static_cast<uint16_t>(kCamColours + scene)) == 1) {
        videoColours(std::vector<Rgb>(cameraColours_.begin(), cameraColours_.end()), 0x80);
        videoColours(std::vector<Rgb>(savedLook_.begin(), savedLook_.end()), 0xE1);
    }
    const int cam = vword(static_cast<uint16_t>(kCamera + 2 * scene));
    if (cam < 0) {
        hide();
    } else {
        for (int i = 0; i < 4; ++i) {
            int x = 0, y = 0;
            if (cam == 0 || cam == 5) x = kQuad[i][0], y = kQuad[i][1];
            else if (i == cam - 1) x = 0x12E, y = 0x98;
            setVword(static_cast<uint16_t>(kCamPlaces + 4 * i), x);
            setVword(static_cast<uint16_t>(kCamPlaces + 2 + 4 * i), y);
            vid_.shown[i] = x >= 1;
        }
        vid_.scale = kScale[std::min(cam, 5)];
    }
    if (vbyte(static_cast<uint16_t>(kCamPlays + scene)) == 1 && cam >= 0 && cam < 5) {
        const int role = cam == 0 ? rnd() % 4 : cam - 1;
        vid_.rolePlaying[role] = 1;
        const int m = static_cast<int8_t>(vbyte(static_cast<uint16_t>(kMembers + role)));
        if (m >= 0 && m < 36) memberPlaying_[m] = true;
        sound(0x6000);
    }
}

void RockBach::videoScene(int scene, int mode) {
    // f18_219a: on screen 2, then the picture onto the display.
    select(2);
    videoBackground(scene, mode);
    if (mode == 0 || (mode == 1 && vword(static_cast<uint16_t>(kBackground + 2 * scene)) > 0)) videoRamps(scene, kBgColours, 1);
    if (mode == 0 || (mode == 2 && vword(static_cast<uint16_t>(kEffect + 2 * scene)) > 0)) videoRamps(scene, kFxColours, 0x40);
    videoEffectStart(scene);
    videoCamera(scene, mode);
    videoFrame(mode, false);
    copyArea(2, 1, videoX_, videoY_, kW, kH);
    select(1);
}

void RockBach::videoFrame(int mode, bool onDisplay) {
    // f18_0bb0: the effect's next step over the clean picture (which then
    // keeps it), then the band over that.
    const int scene = vid_.scene;
    const int effect = vword(static_cast<uint16_t>(kEffect + 2 * scene));
    const int motion = vword(static_cast<uint16_t>(kCamMotion + 2 * scene));  // 1 bouncing, 2 spinning
    const int cam = vword(static_cast<uint16_t>(kCamera + 2 * scene));
    const int X = videoX_, Y = videoY_;
    const int cx = X + 0xB0, cy = Y + 0x7C;  // [6732], [66F2]
    constexpr int kR = 0x32, kStep = 0xA6;    // [6664], [8B4C]
    auto nextColour = [&](int by, int top) {
        vid_.colour += by;
        if (vid_.colour >= top) vid_.colour = 0x40;
    };
    // The spirograph's and the spin's points: a corner's x from a cosine,
    // its y from a sine, of the angle scaled and moved.
    auto px = [&](int32_t a) { return cx + static_cast<int>((static_cast<int32_t>(cosine(a)) * kW) >> 13); };
    auto py = [&](int32_t a) { return cy + static_cast<int>((static_cast<int32_t>(sine(a)) * kH) >> 13); };
    const auto member = [this](int i) { return static_cast<int>(static_cast<int8_t>(vbyte(static_cast<uint16_t>(kMembers + i)))); };
    auto body = [&](int m) {
        return static_cast<uint16_t>(0x206F + m * 5 + static_cast<int8_t>(data_[0x194 + m * 16 + memberFrame_[m]]));
    };
    auto face = [&](int m) { return static_cast<uint16_t>(0x200E + data_[0x4E + m]); };
    if (onDisplay) select(2);
    if (mode == 0 || mode == 2) {
        if (mode == 0 && motion == 1) freeArea(vid_.clean);
        else restoreArea(vid_.clean);
        switch (effect) {
            case -1:
                if (mode == 2) {
                    const std::string t = dataString(0x1FF8);  // "No EFFECT selected for this slot"
                    font_->draw(ctx_.screens[2], X + 0xB0 - font_->width(t) / 2, Y + 10, t, 0xFF);
                }
                break;
            case 0: {
                // Notes (2177-2186) scattered at random sizes.
                const int s = rnd() % 100 + 0xCE;
                const uint16_t id = static_cast<uint16_t>(0x2177 + rnd() % 16);
                const int y = rnd() % kH + Y;
                drawScaledCentred(rnd() % kW + X, y, s, s, id);
                break;
            }
            case 1:
                // The spirograph: two lines a step.
                for (int k = 0; k < 2; ++k) {
                    nextColour(1, 0x80);
                    vid_.angle += kStep;
                    const int32_t a = vid_.angle, ar = static_cast<int32_t>(static_cast<int64_t>(a) * kR);  // (f36_12e4, wrapping)
                    line(px(kR * 100 + a), py(ar / 10), px(ar / 9 + 3000 + kR * 80), py(ar / 8 + 8000),
                         static_cast<uint8_t>(vid_.colour));
                }
                break;
            case 2:
                // Three frames of a sprite (2187-2189) somewhere; then gone.
                ++vid_.flash;
                if (vid_.flash == 0) {
                    vid_.flashX = rnd() % 0xF8 + X;
                    vid_.flashY = rnd() % 0x9B + Y;
                    freeArea(vid_.flashArea);
                    vid_.flashArea = saveArea(vid_.flashX, vid_.flashY, 0x68, 0x5D);
                    drawLogo(vid_.flashX, vid_.flashY, 0x2187);
                } else if (vid_.flash == 1 || vid_.flash == 2) {
                    drawLogo(vid_.flashX, vid_.flashY, static_cast<uint16_t>(0x2187 + vid_.flash));
                } else if (vid_.flash == 3) {
                    restoreArea(vid_.flashArea);
                    vid_.flashArea = saveArea(0, 0, 2, 2);
                    vid_.flash = -1;
                }
                break;
            case 3: {
                // Seven boxes, each a little smaller, in the next colours.
                vid_.boxW = rnd() % 0x3C + 0x1E;
                vid_.boxH = rnd() % 0x32 + 0x1E;
                int x = rnd() % (kW - vid_.boxW) + X;
                int y = rnd() % (kH - vid_.boxH) + Y;
                for (int k = 0; k < 7; ++k) {
                    nextColour(1, 0x80);
                    fill(x, y, vid_.boxW, vid_.boxH, static_cast<uint8_t>(vid_.colour));
                    vid_.boxW -= vid_.boxW / 10;
                    vid_.boxH -= vid_.boxH / 10;
                    x += vid_.boxW / 10 / 2;
                    y += vid_.boxH / 10 / 2;
                }
                break;
            }
            case 4: {
                // Fireworks: eight sparks flying out, a burst every few steps.
                static const int kDir[8][2] = {{8, 0}, {8, -8}, {0, -8}, {-8, -8}, {-8, 0}, {-8, 8}, {0, 8}, {8, 8}};
                for (int i = 0; i < 8; ++i) vid_.sparkX[i] += kDir[i][0], vid_.sparkY[i] += kDir[i][1];
                nextColour(3, 0x7E);
                auto run = [&](int x, int y, int n, int c) {  // g37_2c4c
                    if (y >= 0 && y < Screen::kHeight) fill(x, y, n, 1, static_cast<uint8_t>(c));
                };
                for (int i = 0; i < 8; ++i) {
                    run(vid_.sparkX[i], vid_.sparkY[i], 2, vid_.colour);
                    run(vid_.sparkX[i], vid_.sparkY[i], 3, vid_.colour + 1);
                    run(vid_.sparkX[i], vid_.sparkY[i], 1, vid_.colour + 2);
                }
                if (++vid_.sparkTick > vid_.sparkEvery) {
                    vid_.sparkTick = 0;
                    vid_.sparkEvery = rnd() % 4 + 3;
                    const int x = rnd() % 0x12E + X, y = rnd() % 0xC6 + Y;
                    vid_.sparkX.fill(x + 0x19);
                    vid_.sparkY.fill(y + 0x19);
                }
                break;
            }
            case 5: {
                // A walker (2218-221B) along the bottom, somewhere new every
                // four steps.
                drawLogo(vid_.walkerX, vid_.walkerY, static_cast<uint16_t>(0x2218 + vid_.walker));
                if (++vid_.walker > 3) {
                    restoreArea(vid_.walkerArea);
                    vid_.walker = 0;
                    const Bitmap& w = ctx_.bitmap(0x2218);
                    vid_.walkerX = rnd() % (kW - w.width) + X + w.width / 2;
                    vid_.walkerY = Y + kH - w.height;
                    vid_.walkerArea = saveArea(vid_.walkerX, vid_.walkerY, w.width, w.height);
                }
                break;
            }
            default:
                break;
        }
        vid_.clean = saveArea(X, Y, kW, kH);
    } else if (mode == 1 || mode == 3) {
        restoreArea(vid_.clean);
        vid_.clean = saveArea(X, Y, kW, kH);
    }
    for (int i = 0; i < 4; ++i) {
        if (vid_.bgDue && vid_.bgTurns && (mode == 0 || mode == 1)) {
            vid_.bgDue = false;
            videoTurn(1, 0x3F);
        }
        if (vid_.fxDue && vid_.fxTurns && (mode == 0 || mode == 2)) {
            vid_.fxDue = false;
            videoTurn(0x40, 0x40);
        }
        if (!vid_.shown[i] || (mode != 0 && mode != 3)) continue;
        const int m = member(i);
        if (m < 0 || m >= 36) continue;
        if (motion == 2) {
            // Spinning: a quadrilateral filled with a member's picture
            // (tiled), its corners going round; only one member.
            vid_.angle += kStep;
            if (vid_.angle <= 0) vid_.angle += kStep;
            const int32_t a = vid_.angle, ar = static_cast<int32_t>(static_cast<int64_t>(a) * kR);  // (f36_12e4, wrapping)
            const std::vector<std::pair<int, int>> q = {
                {px(kR * 100 + a), py(ar / 10)},
                {px(ar / 9 + 3000 + kR * 80), py(ar / 8 + 8000)},
                {px(kR * 50 + a), py(ar / 20)},
                {px(ar / 5 + 4000 + kR * 90), py(ar / 9 + 5000)},
            };
            if (q[0].first == q[1].first && q[2].first == q[3].first &&
                (q[0].second != q[1].second || q[2].second != q[3].second))
                continue;
            uint16_t id;
            if (cam == 0) {
                const int o = member(rnd() % 4);
                id = o >= 0 ? body(o) : body(m);
            } else if (cam == 5) {
                const int o = member(rnd() % 4);
                id = face(o >= 0 ? o : m);
            } else {
                id = body(m);
            }
            fillPolygonWith(q, id);
            break;
        }
        // The member's next frame: playing through its list (FE ends it),
        // else idling on frames 0 and 1.
        int f = memberFrame_[m] + 1;
        if (memberPlaying_[m]) {
            if (static_cast<int8_t>(data_[0x194 + m * 16 + f]) == -2) {
                memberPlaying_[m] = false;
                f = 0;
                vid_.rolePlaying[i] = 0;
            }
        } else if (f > 1) {
            f = 0;
        }
        memberFrame_[m] = f;
        const uint16_t id = cam == 5 ? face(m) : body(m);
        const uint16_t ax = static_cast<uint16_t>(kCamPlaces + 4 * i), ay = static_cast<uint16_t>(kCamPlaces + 2 + 4 * i);
        if (motion == 1) {
            // Bouncing off the picture's edges.
            const Bitmap& b = ctx_.bitmap(id);
            const int hw = b.width / 2, hh = b.height / 2;
            setVword(ax, vword(ax) + vid_.vx[i]);
            if (vword(ax) - hw <= X || vword(ax) + hw >= X + kW) {
                vid_.vx[i] = -vid_.vx[i];
                setVword(ax, vword(ax) + vid_.vx[i]);
            }
            setVword(ay, vword(ay) + vid_.vy[i]);
            if (vword(ay) - hh <= Y || vword(ay) + hh >= Y + kH) {
                vid_.vy[i] = -vid_.vy[i];
                setVword(ay, vword(ay) + vid_.vy[i]);
            }
        }
        drawScaledCentred(vword(ax), vword(ay), vid_.scale, vid_.scale, id);
    }
    if (onDisplay) {
        copyArea(2, 1, X, Y, kW, kH);
        select(1);
    }
}

void RockBach::videoCredits() {
    // f04_08cc: over the picture, centred, in white.
    const int x = videoX_ + 0xB0, y = videoY_ + 0xF;
    auto line = [&](int row, const std::string& s) { text(x - font_->width(s) / 2, y + row * 0xF, s, 0xFF); };
    line(0, dataString(0x663));   // Corel Productions
    line(1, dataString(0x686));   // Proudly Presents
    line(2, vstring(kBandName));     // the band
    line(4, dataString(0x69F));   // Singing
    line(5, vstring(kSongName));     // the song
    line(7, dataString(0x6BA));   // In their new video
    line(8, vstring(kVideoName));     // the video
    line(10, dataString(0x6E6));  // Produced and Directed By
    line(11, vstring(kProducer));    // the producer
}

void RockBach::playVideo(int mode) {
    // f18_22a4: until the song's last slot ends, or a click or key. The
    // whole video (mode 0) starts with the credits, Edison's remote and
    // the lights going down; it keeps his chair and the VCR's meter going.
    vid_ = VideoState{};
    select(1);
    if (mode == 0) {
        studioBackdrop(0x1004);
    } else {
        copyArea(1, 2, 0, 0, Screen::kWidth, Screen::kHeight);
        std::copy(savedLook_.begin(), savedLook_.end(), ctx_.screens[2].palette.begin() + 0xE1);
    }
    select(2);
    vid_.clean = saveArea(videoX_, videoY_, kW, kH);
    vid_.walkerArea = saveArea(0, 0, 2, 2);
    int song[16][2];
    for (int i = 0; i < 16; ++i) song[i][0] = vword(static_cast<uint16_t>(kSong + 4 * i)), song[i][1] = vword(static_cast<uint16_t>(kSong + 2 + 4 * i));
    setSong(song);
    select(1);
    show(2);
    std::copy_n(ctx_.screens[1].palette.begin() + 0x80, cameraColours_.size(), cameraColours_.begin());
    if (mode == 0) {
        signArea_ = saveArea(0x22, 0xF4, 0x5C, 0x96);
        videoSign();
    }
    vid_.flashArea = saveArea(0, 0, 1, 1);
    if (mode == 0) {
        videoCredits();
        // Edison's remote (22CA-22E2).
        static const uint8_t kRemote[29] = {1,    2,    3,    4,    5,    6,    7,    8,    9,    0xA,  0xB,  0xC,  0xD,  0xE,  0xF,
                                            0x10, 0x11, 0x12, 0x13, 0x13, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x19, 0x18};
        constexpr int kRx = 0x1D2, kRy = 0x13C, kRw = 0xAD, kRh = 0x54;
        int remote = saveArea(kRx, kRy, kRw, kRh);
        select(2);
        for (uint8_t f : kRemote) {
            restoreArea(remote);
            remote = saveArea(kRx, kRy, kRw, kRh);
            drawLogo(kRx, kRy, static_cast<uint16_t>(0x22C9 + f));
            copyArea(2, 1, kRx, kRy, kRw, kRh);
            waitCountdown(1);
        }
        freeArea(remote);
        select(1);
        // The lights (colours C0-DF) go down.
        const std::vector<Rgb> lights(ctx_.displayPalette.begin() + 0xC0, ctx_.displayPalette.begin() + 0xE0);
        std::vector<Rgb> dim = lights;
        for (int factor = 0x100; factor >= 0x50; factor -= 0x19) {
            for (size_t k = 0; k < lights.size(); ++k)  // f44_0000
                dim[k] = factor >= 0x100 ? lights[k]
                                         : Rgb{static_cast<uint8_t>(lights[k].r * factor >> 8), static_cast<uint8_t>(lights[k].g * factor >> 8),
                                               static_cast<uint8_t>(lights[k].b * factor >> 8)};
            setColours(dim, 0xC0);
            waitCountdown(1);
        }
        setColours(dim, 0xC0);
        waitCountdown(5);
    }
    clearInput();
    videoScene(0, mode);
    tempo_ = static_cast<uint8_t>(vword(kTempo));
    musicPlay();
    ctx_.platform.withFm([](ArtechFmDriver& d) { d.poke(d.getVar(), 0); });
    bool drawDue = false, meterDue = false, signDue = false;
    int ticks = 0;  // [20B4]
    ctx_.timer.setPeriodic(kDrawSlot, 15, [this, &drawDue] { drawDue = true, vid_.fxDue = true; });  // g18_2286
    ctx_.timer.setPeriodic(kTickSlot, 10, [this, &ticks, &meterDue, &signDue] {                      // g18_2236
        vid_.bgDue = true;
        if (++ticks > 2) meterDue = signDue = true, ticks = 0;
    });
    clearInput();
    for (bool ended = false; !ended && !anyInput();) {
        ctx_.pump();
        if (meterDue && mode == 0) {
            meterDue = false;
            videoMeter();
        }
        if (signDue && mode == 0) {
            signDue = false;
            videoSign();
        }
        if (vid_.bgDue && vid_.bgTurns && (mode == 0 || mode == 1)) {
            vid_.bgDue = false;
            videoTurn(1, 0x3F);
        }
        if (vid_.fxDue && vid_.fxTurns && (mode == 0 || mode == 2)) {
            vid_.fxDue = false;
            videoTurn(0x40, 0x40);
        }
        if (drawDue && mode != 1) {
            videoFrame(mode, true);
            drawDue = false;
        }
        uint8_t event = 0;
        ctx_.platform.withFm([&event](ArtechFmDriver& d) {
            event = d.peek(d.getVar());
            if (event) d.poke(d.getVar(), 0);
        });
        if (event) {
            if (++vid_.scene > 15) {
                vid_.scene = 15;
                ended = true;
            }
            videoScene(vid_.scene, mode);
            if (mode == 0) videoMeter();
        }
    }
    musicStop();
    freeArea(vid_.clean);
    freeArea(vid_.flashArea);
    freeArea(vid_.walkerArea);
    if (mode == 0) freeArea(signArea_);
    memberPlaying_.fill(false);
    memberFrame_.fill(0);
    ctx_.timer.setPeriodic(kDrawSlot, 0, nullptr);
    ctx_.timer.setPeriodic(kTickSlot, 0, nullptr);
    clearInput();
}

}  // namespace edison
