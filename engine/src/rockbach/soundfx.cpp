// WINMAIN's segment 29 (Sound FX: load a WAV, pick out part of it, change
// its speed, add an echo, reverb or a low filter, play it backward or over
// and over, save it) and segment 13 (the effects).

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <initializer_list>
#include <fstream>
#include <iterator>

#include "rockbach/rockbach.h"

namespace edison {
namespace {

constexpr long kBuffer = 0xD6D8;  // 55000: each buffer
constexpr int kHandleY = 0xA, kHandleW = 0x12, kHandleH = 0x84, kRightMost = 0x266;

int8_t wrap(long v) { return static_cast<int8_t>(static_cast<uint8_t>(v & 0xFF)); }

}  // namespace

// --- the effects (segment 13) ------------------------------------------------------

void RockBach::sfxProcess() {
    // f13_0a90: reverse, the low filter, the echo, the reverb, in that
    // order on the input; then f13_0d22 scales the result to its loudest and
    // makes it 8-bit unsigned (buffer A).
    SoundFxState& s = sfx_;
    s.processing = true;
    const long oldLen = s.len;
    const bool wasAll = s.start < 10 && s.end > oldLen - 11;
    std::vector<int8_t> cur = s.input;
    const long nIn = static_cast<long>(s.input.size());
    if (s.reverse) std::reverse(cur.begin(), cur.end());  // f13_0350
    if (s.filter) {
        // f13_066c: two poles, coefficients DS:18EC by (freq * 16) / rate.
        const long k = std::min<long>(7, s.fileRate ? (static_cast<long>(s.filterFreq) << 4) / s.fileRate : 7);
        auto coef = [this, k](int j) {
            const uint16_t at = static_cast<uint16_t>(0x18EC + k * 12 + j * 4);
            return static_cast<int32_t>(dataWord(at) | static_cast<uint32_t>(dataWord(static_cast<uint16_t>(at + 2))) << 16);
        };
        const long a = coef(0), b = coef(1), c = coef(2);
        long y0 = 0, y1 = 0;
        for (long i = 0; i < nIn; ++i) {
            const long r = (a * y0 - b * y1 + ((c * cur[i]) << 4)) >> 4;
            y1 = y0;
            y0 = r;
            cur[i] = static_cast<int8_t>(std::clamp<long>(r >> 4, -128, 127));
        }
    }
    if (s.echo) {
        // f13_0146: one echo, delay ms later.
        const std::vector<int8_t> src = cur;
        long d = static_cast<long>(s.rate) * s.echoDelay / 1000;
        const long len0 = static_cast<long>(src.size());
        if (len0 + d >= kBuffer) d = kBuffer - len0;
        const long g1 = 0x6400 / (s.echoGain + 100), g2 = (static_cast<long>(s.echoGain) << 8) / 100;
        cur.assign(static_cast<size_t>(len0 + d), 0);
        for (long i = 0; i < len0 + d; ++i) {
            const long si = i < nIn ? src[i] : 0;
            cur[i] = i < d ? wrap((si * g1) >> 8) : wrap(((si + ((src[i - d] * g2) >> 8)) * g1) >> 8);
        }
    }
    if (s.reverb) {
        // f13_0408: two taps of its own output, delay and twice it back.
        const std::vector<int8_t> src = cur;
        const long t = static_cast<long>(s.rate) * s.reverbDelay;
        const long d0 = t / 1000, d1 = (t - 5) * 2 / 1000;
        const long origLen = static_cast<long>(src.size()), len = std::min(origLen + d1, kBuffer);
        const long g2 = (static_cast<long>(s.reverbGain) << 8) / 100, g1 = 0x6400 / (2L * s.reverbGain + 100);
        cur.assign(static_cast<size_t>(std::max(0L, len)), 0);
        for (long i = 0; i < len; ++i) {
            long acc = (i > d0 ? cur[i - d0 - 1] : 0) + (i > d1 ? cur[i - d1 - 1] : 0);
            acc = (acc * g2) >> 8;
            if (i < origLen) acc += src[i];
            cur[i] = wrap((acc * g1) >> 8);
        }
    }
    // f13_0d22.
    s.len = static_cast<long>(cur.size());
    long m = 0;
    for (long i = 0x10; i < s.len; ++i) m = std::max<long>(m, std::abs(cur[i]));
    const long sc = m ? 0x8000 / m : 0x100;
    s.a.assign(cur.size(), 0x80);
    for (long i = 0; i < s.len; ++i) {
        const long v = i > 0x10 ? (cur[i] * sc) >> 8 : cur[i];
        s.a[i] = static_cast<uint8_t>(std::clamp<long>(v, -128, 127) + 0x80);
    }
    if (wasAll) s.start = 0, s.end = s.len;
    if (s.start > s.len) s.start = s.len - (s.end - s.start);
    if (s.end > s.len) s.end = s.len - 1;
    s.upToDate = true;
    s.processing = false;
}

void RockBach::sfxSet(int type, int value) {
    // f13_0964: an effect on or off (value < 0: all off); processed at once
    // unless it's the loop.
    SoundFxState& s = sfx_;
    if (!options_.music) return;
    s.upToDate = false;
    if (value < 0) {
        s.reverse = s.echo = s.reverb = s.filter = s.loop = false;
    } else {
        switch (type) {
            case 2: s.reverse = value; break;
            case 3: s.echo = value; break;
            case 4: s.filter = value; break;
            case 5: s.reverb = value; break;
            case 6: s.loop = value, s.upToDate = true; break;
            default: break;
        }
    }
    if (value >= 0 && type != 6 && !s.processing && !s.input.empty()) sfxProcess();
}

bool RockBach::sfxLoad(const std::string& path) {
    // f13_0eb6: at most 55000 bytes; the header (2C bytes, or 2A when the
    // format tag is even) kept; the samples made signed.
    SoundFxState& s = sfx_;
    ctx_.platform.stopWav();
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::vector<uint8_t> file((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (file.size() > static_cast<size_t>(kBuffer)) file.resize(kBuffer - 1);
    if (file.size() < 0x2C) return false;
    s.hdr = (file[0x14] & 1) ? 0x2C : 0x2A;
    s.header.assign(file.begin(), file.begin() + s.hdr);
    s.fileRate = s.rate = file[0x18] | file[0x19] << 8;
    s.input.clear();
    for (size_t i = static_cast<size_t>(s.hdr); i < file.size(); ++i) s.input.push_back(static_cast<int8_t>(file[i] - 0x80));
    s.start = 0;
    s.end = static_cast<long>(s.input.size()) - 1;
    s.len = 0;
    s.upToDate = false;
    return true;
}

std::vector<uint8_t> RockBach::sfxWav(long from, long to) const {
    // A WAV of A[from, to) at the playing rate (8-bit mono).
    const SoundFxState& s = sfx_;
    from = std::clamp(from, 0L, s.len);
    to = std::clamp(to, from, s.len);
    const uint32_t n = static_cast<uint32_t>(to - from), rate = static_cast<uint32_t>(std::max(1000, s.rate));
    std::vector<uint8_t> w(44 + n);
    auto put32 = [&w](size_t at, uint32_t v) { for (int k = 0; k < 4; ++k) w[at + k] = static_cast<uint8_t>(v >> (8 * k)); };
    std::memcpy(w.data(), "RIFF", 4);
    put32(4, 36 + n);
    std::memcpy(w.data() + 8, "WAVEfmt ", 8);
    put32(16, 16);
    w[20] = 1, w[22] = 1;
    put32(24, rate);
    put32(28, rate);
    w[32] = 1, w[34] = 8;
    std::memcpy(w.data() + 36, "data", 4);
    put32(40, n);
    std::copy(s.a.begin() + from, s.a.begin() + to, w.begin() + 44);
    return w;
}

void RockBach::sfxPlay() {
    // f13_117e: the selection at the playing rate. (The original writes
    // the WAV header over the selection's first bytes in A and plays from
    // there, so those bytes are lost: kept here.)
    SoundFxState& s = sfx_;
    ctx_.platform.stopWav();
    if (!options_.music || s.input.empty()) return;
    if (!s.upToDate && !s.processing) sfxProcess();
    for (int k = 0; k < s.hdr && s.start + k < s.len; ++k) s.a[s.start + k] = s.header[k];
    ctx_.platform.playWav(sfxWav(s.start + s.hdr, s.end));
}

// --- Sound FX (segment 29) ------------------------------------------------------

void RockBach::soundFxWidgets() {
    // f29_0000: 38 widgets.
    auto w = [](int x0, int y0, int x1, int y1, uint16_t flags, uint16_t down, uint16_t up, uint16_t ovUp = 0x2000,
                uint16_t ovDown = 0x2000, int group = 0) {
        Widget r{x0, y0, x1, y1, flags};
        r.group = group;
        r.bitmapDown = down;
        r.bitmapUp = up;
        r.overlayUp = ovUp;
        r.overlayDown = ovDown;
        r.hotkey = 1;
        return r;
    };
    std::vector<Widget>& l = soundFxWidgets_;
    l.clear();
    l.push_back(w(534, 352, 591, 397, 0x100, 0x2229, 0x222A));  // 0 exit
    l.push_back(w(196, 362, 265, 391, 0x100, 0x2225, 0x2226));  // 1 go
    l.push_back(w(280, 362, 349, 391, 0x100, 0x2255, 0x2256));  // 2 stop
    l.push_back(w(43, 192, 138, 220, 0x104, 0x2309, 0x2309, 0x2230, 0x2231));  // 3 load
    l.push_back(w(43, 234, 138, 262, 0x104, 0x2309, 0x2309, 0x2232, 0x2233));  // 4 save
    l.push_back(w(42, 278, 138, 306, 0x104, 0x2309, 0x2309, 0x2234, 0x2235));  // 5 delete
    l.push_back(w(132, 176, 167, 209, 0x340, 0x222E, 0x222F, 0x2236, 0x2236, 3));  // 6-26 (never shown)
    for (int i = 0; i < 20; ++i) {
        const int x = 18 + 38 * (i % 4), y = 212 + 36 * (i / 4);
        const uint16_t ov = static_cast<uint16_t>(0x2237 + i);
        l.push_back(w(x, y, x + 35, y + 33, 0x340, 0x222E, 0x222F, ov, ov, 3));
    }
    l.push_back(w(364, 362, 433, 391, 0x100, 0x225D, 0x225E));  // 27 loop
    l.push_back(w(448, 362, 517, 391, 0x100, 0x2257, 0x2258));  // 28 backward
    l.push_back(w(268, 148, 409, 167, 0x100, 0x2227, 0x2228));  // 29 echo
    l.push_back(w(410, 148, 553, 167, 0x100, 0x2259, 0x225A));  // 30 reverb
    l.push_back(w(554, 148, 625, 167, 0x100, 0x225B, 0x225C));  // 31 lofilter
    static const int kX[6] = {224, 296, 368, 440, 512, 584};
    for (int i = 0; i < 6; ++i) {  // 32-37 speed, echo delay and gain, reverb delay and gain, the filter
        Widget s = w(kX[i], 198, kX[i] + 13, 307, 0x110, i ? 0x2251 : 0x2252, i ? 0x2251 : 0x2252, i ? 0x2254 : 0x2253,
                     i ? 0x2254 : 0x2253);
        s.slider = &sfxSliders_[i];
        l.push_back(s);
    }
}

void RockBach::sfxTip(int n) {
    // f29_16f4: two lines (DS:2AA2) under the sliders.
    const int previous = current();
    select(2);
    fill(0xC4, 0x144, 0x1AC, 0x18, 0);
    const uint16_t at = static_cast<uint16_t>(0x2AA2 + 8 * n);
    font_->draw(ctx_.screens[2], 0xCA, 0x144, dataString(dataWord(at)), 0xFF);
    font_->draw(ctx_.screens[2], 0xCA, 0x14E, dataString(dataWord(static_cast<uint16_t>(at + 4))), 0xFF);
    copyArea(2, 1, 0xC4, 0x144, 0x1AC, 0x18);
    select(previous);
}

void RockBach::sfxSliders(int which, bool apply) {
    // f29_1322: the sliders' values into the effects (all of them, reset,
    // for -1; else echo 0, reverb 1, the filter 2).
    SoundFxState& s = sfx_;
    std::vector<Widget>& w = soundFxWidgets_;
    auto v = [this](int i) { return sfxSliders_[i].value; };
    if (which < 0) {
        static const int kStart[6] = {100, 0, 30, 5, 30, 0x7E};
        for (int i = 0; i < 6; ++i) {
            sfxSliders_[i].value = kStart[i];
            placeSlider(w[32 + i], true);
        }
        if (apply) {
            s.rate = s.fileRate + (200 - v(0)) * 20 - 2000;
            s.echoDelay = v(1) * 10 + 300, s.echoGain = v(2) + 20;
            s.reverbDelay = v(3) * 5 + 5, s.reverbGain = v(4) + 20;
            s.filterFreq = (200 - v(5)) * 12 + 100;
        }
        for (int i = 33; i <= 37; ++i) {
            w[i].bitmapUp = w[i].bitmapDown = 0x2251;
            w[i].overlayUp = w[i].overlayDown = 0x2254;
            w[i].flags |= Widget::kHidden;
            placeSlider(w[i], false);
        }
    } else if (which == 0) {
        placeSlider(w[33], true), placeSlider(w[34], true);
        s.echoDelay = v(1) * 10 + 300, s.echoGain = v(2) + 20;
    } else if (which == 1) {
        placeSlider(w[35], true), placeSlider(w[36], true);
        s.reverbDelay = v(3) * 5 + 5, s.reverbGain = v(4) + 20;
    } else {
        placeSlider(w[37], true);
        s.filterFreq = (200 - v(5)) * 12 + 100;
    }
}

void RockBach::sfxToggle(int k, std::array<bool, 5>& on) {
    // f29_152e: widgets 27-31 (loop, backward, echo, reverb, the filter) on
    // or off; -1: all off, the sliders back.
    static const int kType[5] = {6, 2, 3, 5, 4};
    std::vector<Widget>& w = soundFxWidgets_;
    if (k < 0) {
        for (int i = 0; i < 5; ++i) {
            w[27 + i].flags &= ~Widget::kPressed;
            drawWidget(w[27 + i], false);
            on[i] = false;
        }
        sfxSet(0, -1);
        sfxSet(1, 1);
        sfxSliders(-1, true);
        return;
    }
    on[k] = !on[k];
    if (on[k]) w[27 + k].flags |= Widget::kPressed;
    else w[27 + k].flags &= ~Widget::kPressed;
    drawWidget(w[27 + k], on[k]);
    sfxSet(kType[k], on[k]);
}

void RockBach::sfxWave(int left, int right) {
    // f29_0ffe: the waveform (the output, a column every len / 596), the
    // handles at the selection when asked, and the lines between them.
    SoundFxState& s = sfx_;
    if (!options_.music) return;
    select(2);
    restoreArea(sfxArea_[0]);
    restoreArea(sfxArea_[1]);
    sfxArea_[0] = saveArea(sfxHandle_[0], kHandleY, kHandleW, kHandleH);
    sfxArea_[1] = saveArea(sfxHandle_[1], kHandleY, kHandleW, kHandleH);
    restoreArea(sfxWaveArea_);
    sfxWaveArea_ = saveArea(0, 8, 0x280, 0x8A);
    int leftX = 0, rightX = kRightMost;
    constexpr int kW = 0x254;
    const long step = std::max(1L, (s.len << 8) / kW);
    sfxScale_ = static_cast<int>(step >> 8);
    Screen& scr = ctx_.screens[2];
    long pos = 0;
    for (int col = 0; col < kW && !s.a.empty(); ++col, pos += step) {
        const int v = s.a[std::min<long>(pos >> 8, s.len - 1)] >> 1;
        for (int y = 0x8A - v; y <= 0x4A; ++y) scr.pixels[static_cast<size_t>(y) * Screen::kWidth + 0x12 + col] = 0xE0;
        for (int y = 0x4A; y <= 0x8A - v; ++y) scr.pixels[static_cast<size_t>(y) * Screen::kWidth + 0x12 + col] = 0xE0;
    }
    if (left >= 0) {
        restoreArea(sfxArea_[0]);
        const int lx = static_cast<int>((s.start << 8) / step);
        sfxArea_[0] = saveArea(lx, kHandleY, kHandleW, kHandleH);
        drawLogo(lx, kHandleY, 0x222B);
        leftX = sfxHandle_[0] = lx;
    }
    if (right >= 0) {
        restoreArea(sfxArea_[1]);
        const int rx = std::abs(static_cast<int>(((s.end + 1) << 8) / step) + 0x12);
        sfxArea_[1] = saveArea(rx, kHandleY, kHandleW, kHandleH);
        drawLogo(rx, kHandleY, 0x222C);
        rightX = sfxHandle_[1] = rx;
    }
    if (left >= 0 || right >= 0) {
        line(0xD, 9, 0x269, 9, 0), line(0xD, 0x8E, 0x269, 0x8E, 0);
        line(leftX + 0xE, 9, rightX + 3, 9, 0xF0), line(leftX + 0xE, 0x8E, rightX + 3, 0x8E, 0xF0);
    }
    copyArea(2, 1, 0, 8, 0x280, 0x8A);
    select(1);
}

int RockBach::soundFx() {
    // f29_17e2.
    SoundFxState& s = sfx_;
    s = SoundFxState{};
    std::vector<Widget>& w = soundFxWidgets_;
    std::string name = dataString(0x2ADB), path;  // DS:4D72 "Noname", DS:4B6E
    const std::string ext = dataString(0x2AD6);    // ".wav"
    bool loaded = false, looping = false;
    std::array<bool, 5> on{};  // loop, backward, echo, reverb, the filter
    soundFxWidgets();
    static const int kMax[6] = {0xE0, 0x4E, 0x54, 0x58, 0x54, 0xE0}, kStart[6] = {0x64, 0, 0x1E, 5, 0x1E, 0x7D};
    for (int i = 0; i < 6; ++i) sfxSliders_[i] = Slider{0, kStart[i], kMax[i], 0xE, 0xC, 0xB, 5, 0x6F};
    select(2);
    blackout();
    backdrop(0x1008);
    show(2);
    initWidgets(w, {0xFB, 0xFC, 0xFD, 0xFE});
    for (int i = 33; i <= 37; ++i) w[i].flags |= Widget::kHidden;
    sfxHandle_ = {0, kRightMost};
    sfxArea_[0] = saveArea(0, kHandleY, kHandleW, kHandleH);
    sfxArea_[1] = saveArea(kRightMost, kHandleY, kHandleW, kHandleH);
    fill(0xE, 0xA, 0x25A, 0x84, 0);
    line(0x12, 0x4A, 0x265, 0x4A, 0xE0);
    drawLogo(0, kHandleY, 0x222B);
    drawLogo(kRightMost, kHandleY, 0x222C);
    line(0xE, 9, 0x269, 9, 0xF0);
    line(0xE, 0x8E, 0x269, 0x8E, 0xF0);
    select(1);
    copyArea(2, 1, 0, 0, Screen::kWidth, Screen::kHeight);
    sfxWaveArea_ = saveArea(0, 8, 0x280, 0x8A);
    sfxTip(0);
    sfxSliders(-1, false);
    clearInput();
    auto resetHandles = [&] {
        restoreArea(sfxArea_[0]);
        restoreArea(sfxArea_[1]);
        const std::array<int, 2> old = sfxHandle_;
        sfxHandle_ = {0, kRightMost};
        sfxArea_[0] = saveArea(0, kHandleY, kHandleW, kHandleH);
        sfxArea_[1] = saveArea(kRightMost, kHandleY, kHandleW, kHandleH);
        return old;
    };
    for (bool done = false; !done;) {
        const int r = pollWidgets();
        if (r < 0 && lastClick_.on && lastClick_.x >= sfxHandle_[1] && lastClick_.x < sfxHandle_[1] + kHandleW &&
            lastClick_.y >= kHandleY && lastClick_.y < kHandleY + kHandleH && loaded) {
            // Dragging the end handle (the original never hit-tests the start's).
            const int off = lastClick_.x - sfxHandle_[1];
            int nx = sfxHandle_[1];
            for (bool held = true; held;) {
                ctx_.pump();
                int mx, my;
                ctx_.platform.mouse(&mx, &my, &held);
                const int want = std::clamp(mx - off, sfxHandle_[0] + 0x50, kRightMost);
                if (want == nx) continue;
                nx = want;
                line(0xD, kHandleY - 1, kHandleW + 0x257, kHandleY - 1, 0);
                line(0xD, kHandleY + kHandleH, kHandleW + 0x257, kHandleY + kHandleH, 0);
                line(sfxHandle_[0] + 0xE, kHandleY - 1, nx + kHandleW - 0xF, kHandleY - 1, 0xF0);
                line(sfxHandle_[0] + 0xE, kHandleY + kHandleH, nx + kHandleW - 0xF, kHandleY + kHandleH, 0xF0);
                long end = static_cast<long>(nx) * sfxScale_;
                if (end > s.len || mx >= off + kRightMost) end = s.len;
                if (end < s.start) end = s.start + 2;
                s.end = end - 1;
                restoreArea(sfxArea_[1]);
                sfxArea_[1] = saveArea(nx, kHandleY, kHandleW, kHandleH);
                drawLogo(nx, kHandleY, 0x222C);
            }
            sfxHandle_[1] = nx;
        }
        if (r == 0) done = true;
        if (!ctx_.platform.wavPlaying() && looping && loaded) sfxPlay();
        if (r == 1 && loaded && !looping) {
            sfxPlay();
            if (on[0]) looping = true;
        }
        if (r == 2) {
            looping = false;
            ctx_.platform.stopWav();
        }
        if (r == 3 && loadDialog(0x73, 0x2B, ext, &path, 1, dataString(0x28C2))) {
            sfxTip(0);
            sfxSet(1, -1);
            on.fill(false);
            looping = false;
            for (int i = 27; i <= 31; ++i) {
                w[i].flags &= ~Widget::kPressed;
                drawWidget(w[i], false);
            }
            resetHandles();
            loaded = sfxLoad(path);
            if (loaded) {
                const auto slash = path.find_last_of("/\\");
                name = path.substr(slash == std::string::npos ? 0 : slash + 1);
                name = name.substr(0, name.find('.'));
                sfxToggle(-1, on);
                sfxSet(1, 1);
                sfxWave(-1, -1);
            }
        }
        if (r == 4 && loaded) {
            // SAVE (f29_0dc6): the header at the playing rate, then the selection.
            std::string out;
            if (nameDialog(0xAE, 0x2E, &name, ext, &out)) {
                std::ofstream f(out, std::ios::binary);
                if (!f) logLine("Rock and Bach: s.c: Error when creating file");
                std::vector<uint8_t> header = s.header;
                for (int off : {0x18, 0x1C})
                    for (int k = 0; k < 4 && off + k < static_cast<int>(header.size()); ++k)
                        header[off + k] = static_cast<uint8_t>(static_cast<uint32_t>(s.rate) >> (8 * k));
                f.write(reinterpret_cast<const char*>(header.data()), static_cast<std::streamsize>(header.size()));
                const long from = std::clamp(s.start, 0L, s.len), to = std::clamp(s.end, from, s.len);
                f.write(reinterpret_cast<const char*>(s.a.data() + from), to - from);
            }
        }
        if (r == 5) {
            // DELETE (f29_0f18): only from the game's folder, after a yes.
            std::string victim;
            if (fileList(0x73, 0x2B, options_.saveDir + "/", false, ext, &victim, 1, dataString(0x28FC), true)) {
                const auto slash = victim.find_last_of("/\\");
                std::string shown = victim.substr(slash == std::string::npos ? 0 : slash + 1);
                shown = shown.substr(0, shown.find('.'));
                if (messageBox(0xA0, 0x50, {dataString(0x286A), "WAVE FILE: " + shown}, {0, 1}) == 1) {
                    std::error_code ec;
                    std::filesystem::remove(victim, ec);
                }
            }
        }
        if (r >= 27 && r <= 31 && loaded) {
            if (r == 27) {
                sfxToggle(0, on);
                if (!on[0]) looping = false;
                sfxTip(on[0] ? 5 : 0);
            } else {
                if (r == 28) sfxTip(!on[1] ? 4 : 0);
                sfxToggle(r - 27, on);
                const std::array<int, 2> old = resetHandles();
                sfxWave(old[0], old[1]);
            }
            auto lights = [&](bool lit, std::initializer_list<int> which, int tip) {
                sfxTip(lit ? tip : 0);
                for (int i : which) {
                    Widget& x = w[i];
                    x.bitmapUp = x.bitmapDown = lit ? 0x2252 : 0x2251;
                    x.overlayUp = x.overlayDown = lit ? 0x2253 : 0x2254;
                    if (lit) x.flags &= ~Widget::kHidden;
                    placeSlider(x, false);
                    if (!lit) x.flags |= Widget::kHidden;
                }
            };
            if (r == 29) lights(on[2], {33, 34}, 1);
            if (r == 30) lights(on[3], {35, 36}, 2);
            if (r == 31) lights(on[4], {37}, 3);
        }
        if (r >= 32 && r <= 37 && loaded) {
            const int v = sfxSliders_[r - 32].value;
            bool redo = false;
            if (r == 32) s.rate = s.fileRate + (200 - v) * 20 - 2000;
            if (r == 33 && on[2]) s.echoDelay = v * 10 + 300, redo = true;
            if (r == 34 && on[2]) s.echoGain = v + 20, redo = true;
            if (r == 35 && on[3]) s.reverbDelay = v * 5 + 5, redo = true;
            if (r == 36 && on[3]) s.reverbGain = v + 20, redo = true;
            if (r == 37 && on[4]) s.filterFreq = (200 - v) * 12 + 100, redo = true;
            if (redo) sfxProcess();
            const std::array<int, 2> old = resetHandles();
            sfxWave(old[0], old[1]);
        }
    }
    ctx_.platform.stopWav();
    freeArea(sfxArea_[0]);
    freeArea(sfxArea_[1]);
    freeArea(sfxWaveArea_);
    return 0;
}

}  // namespace edison
