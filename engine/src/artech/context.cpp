#include "artech/context.h"

#include <algorithm>
#include <cstdio>
#include <set>

namespace edison {

void warnOnce(const std::string& message) {
    static std::set<std::string> seen;
    if (seen.insert(message).second) std::fprintf(stderr, "warning: %s\n", message.c_str());
}

const Bitmap& GameContext::bitmap(uint16_t id) {
    auto& slot = bitmaps_[id];
    if (!slot) {
        slot = std::make_unique<Bitmap>();
        std::vector<uint8_t> file;
        std::string error;
        if (!archive.read(id, file, &error) || !decodeBmp(file, *slot, &error)) {
            char buf[32];
            std::snprintf(buf, sizeof buf, "bitmap %04x: ", id);
            warnOnce(buf + error);
        }
    }
    return *slot;
}

const Font& GameContext::font(uint16_t id) {
    auto& slot = fonts_[id];
    if (!slot) {
        slot = std::make_unique<Font>();
        std::vector<uint8_t> data;
        std::string error;
        if (!archive.read(id, data, &error) || !slot->load(data, &error)) {
            char buf[32];
            std::snprintf(buf, sizeof buf, "font %04x: ", id);
            warnOnce(buf + error);
        }
    }
    return *slot;
}

void GameContext::startTimer() {
    timer.setPeriodic(Timer::kSlots - 1, 10, [this] {
        for (int& c : countdown)
            if (c > 0) --c;
    });
    timer.reset(platform.milliseconds());
}

void GameContext::pump() {
    if (!platform.pumpEvents()) throw Closed{};
    timer.advance(platform.milliseconds());
    platform.present(screens[1], displayPalette);
}

void GameContext::blackout() {
    screens[2].clear();
    screens.copyAll(2, 1);
    displayPalette = {};
}

void GameContext::showFullScreen(uint16_t id, int screen) {
    const Bitmap& bmp = bitmap(id);
    Screen& s = screens[screen];
    s.clear();
    for (int y = 0; y < std::min(bmp.height, Screen::kHeight); ++y)
        for (int x = 0; x < std::min(bmp.width, Screen::kWidth); ++x)
            s.pixels[static_cast<size_t>(y) * Screen::kWidth + x] = bmp.at(x, y);
    s.palette = bmp.palette;
}

void GameContext::setDisplayPalette(int screen) {
    displayPalette = screens[screen].palette;
    displayPalette[0] = Rgb{0, 0, 0};
    displayPalette[255] = Rgb{255, 255, 255};
}

void GameContext::drawLogo(int screen, int x, int y, uint16_t id) {
    screens.drawSprite(screen, bitmap(id), x, y);
}

bool GameContext::read(uint16_t id, std::vector<uint8_t>& out) {
    std::string error;
    if (archive.read(id, out, &error)) return true;
    warnOnce(error);
    return false;
}

void GameContext::playWav(uint16_t id) {
    std::vector<uint8_t> wav;
    if (read(id, wav)) platform.playWav(wav);
}

void GameContext::markFinished(uint16_t anim) {
    for (int& slot : finishedAnims_) {
        if (slot == anim) return;
        if (slot < 0) {
            slot = anim;
            return;
        }
    }
}

bool GameContext::finished(uint16_t anim) const {
    for (int slot : finishedAnims_)
        if (slot == anim) return true;
    return false;
}

void GameContext::clearFinished(uint16_t anim) {
    for (int& slot : finishedAnims_)
        if (slot == anim) {
            slot = -1;
            return;
        }
}

}  // namespace edison
