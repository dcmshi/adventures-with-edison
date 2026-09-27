#include "shell/shell_context.h"

#include <cstdio>
#include <set>

namespace edison {

void shellWarn(const std::string& message) {
    static std::set<std::string> seen;
    if (seen.insert(message).second) std::fprintf(stderr, "warning: %s\n", message.c_str());
}

const Bitmap& ShellContext::bitmap(uint16_t id) {
    auto& slot = bitmaps_[id];
    if (!slot) {
        slot = std::make_unique<Bitmap>();
        std::vector<uint8_t> file;
        std::string error;
        if (!archive.read(id, file, &error) || !decodeBmp(file, *slot, &error)) {
            char buf[32];
            std::snprintf(buf, sizeof buf, "bitmap %04x: ", id);
            shellWarn(buf + error);
        }
    }
    return *slot;
}

bool ShellContext::read(uint16_t id, std::vector<uint8_t>& out) {
    std::string error;
    if (archive.read(id, out, &error)) return true;
    shellWarn(error);
    return false;
}

void ShellContext::playWav(uint16_t id) {
    std::vector<uint8_t> wav;
    if (read(id, wav)) platform.playWav(wav);
}

void ShellContext::markFinished(uint16_t anim) {
    for (int& slot : finishedAnims_) {
        if (slot == anim) return;
        if (slot < 0) {
            slot = anim;
            return;
        }
    }
}

bool ShellContext::finished(uint16_t anim) const {
    for (int slot : finishedAnims_)
        if (slot == anim) return true;
    return false;
}

void ShellContext::clearFinished(uint16_t anim) {
    for (int& slot : finishedAnims_)
        if (slot == anim) {
            slot = -1;
            return;
        }
}

}  // namespace edison
