#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "formats/archive.h"
#include "formats/bitmap.h"
#include "shell/platform.h"
#include "shell/screens.h"

namespace edison {

// State shared by the launcher's subsystems: resources, drawing surfaces
// and the game clock.
struct ShellContext {
    explicit ShellContext(Platform& p) : platform(p) {}

    Platform& platform;
    Archive archive;
    Screens screens;
    // The library's game tick counter (DS:0000), advanced at 16 Hz.
    uint32_t gameTicks = 0;

    // Archive bitmap by id, decoded once. Missing ids give an empty bitmap.
    const Bitmap& bitmap(uint16_t id);
    bool read(uint16_t id, std::vector<uint8_t>& out);
    void playWav(uint16_t id);

    // Anims that have run to the end (the 256-entry table at DS:6DF2).
    void markFinished(uint16_t anim);
    bool finished(uint16_t anim) const;
    void clearFinished(uint16_t anim);
    void resetFinished() { finishedAnims_.assign(256, -1); }

private:
    std::map<uint16_t, std::unique_ptr<Bitmap>> bitmaps_;
    std::vector<int> finishedAnims_ = std::vector<int>(256, -1);
};

// Logs a problem once per distinct message (to stderr).
void shellWarn(const std::string& message);

}  // namespace edison
