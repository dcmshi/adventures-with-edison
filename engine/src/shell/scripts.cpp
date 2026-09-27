#include "shell/scripts.h"

#include <cstdio>
#include <string>

#include "shell/anims.h"
#include "shell/shell_context.h"

namespace edison {
namespace {

uint16_t u16(const std::vector<uint8_t>& d, size_t at) {
    return at + 1 < d.size() ? static_cast<uint16_t>(d[at] | d[at + 1] << 8) : 0;
}

// Script commands: the order of the table at EDISON.EXE DS:0848.
enum Command : uint16_t {
    kShowLogo, kShowCLogo, kCall, kShowFScreen, kStartAnim, kStopAnim, kAddButtons, kKillScript,
    kSetTextFont, kStartSound, kDrawLine, kDrawPoint, kSetDrawPen, kSetTextPen, kCopyFScreen,
    kWorkScreen, kSleep, kRestart, kCopyArea, kClearButtons, kDrawText, kSetPalette, kFade,
    kUnique, kMouse, kTell, kCommandCount
};
const char* const kCommandNames[] = {
    "SHOWLOGO", "SHOWCLOGO", "CALL", "SHOWFSCREEN", "STARTANIM", "STOPANIM", "ADDBUTTONS",
    "KILLSCRIPT", "SETTEXTFONT", "STARTSOUND", "DRAWLINE", "DRAWPOINT", "SETDRAWPEN",
    "SETTEXTPEN", "COPYFSCREEN", "WORKSCREEN", "SLEEP", "RESTART", "COPYAREA", "CLEARBUTTONS",
    "DRAWTEXT", "SETPALETTE", "FADE", "UNIQUE", "MOUSE", "TELL"};

}  // namespace

const Script* Scripts::find(uint16_t archiveId, uint16_t number) {
    const uint32_t key = archiveId | static_cast<uint32_t>(number) << 16;
    if (auto it = loaded_.find(key); it != loaded_.end()) return it->second.get();

    // Loading a file registers every script in it (f06_00fe).
    std::vector<uint8_t> d;
    if (!ctx_.read(archiveId, d) || d.size() < 2) return nullptr;
    const uint16_t count = u16(d, 0);
    for (uint16_t k = 0; k < count; ++k) {
        const size_t rec = u16(d, 2 + 2u * k);
        auto s = std::make_unique<Script>();
        s->archiveId = archiveId;
        s->number = u16(d, rec);
        const uint16_t statements = u16(d, rec + 2);
        const size_t codeEnd = rec + 6 + 2u * statements + u16(d, rec + 4);
        std::vector<size_t> offsets;
        for (uint16_t i = 0; i < statements; ++i) offsets.push_back(u16(d, rec + 6 + 2u * i));
        for (size_t off : offsets) {
            size_t end = codeEnd;  // a statement runs to the next one in the file
            for (size_t o : offsets)
                if (o > off && o < end) end = o;
            if (off >= d.size() || end > d.size() || end < off + 4) break;
            s->statements.emplace_back(d.begin() + off, d.begin() + end);
        }
        const uint32_t k2 = archiveId | static_cast<uint32_t>(s->number) << 16;
        loaded_.emplace(k2, std::move(s));
    }
    auto it = loaded_.find(key);
    return it != loaded_.end() ? it->second.get() : nullptr;
}

ScriptContext* Scripts::start(uint16_t archiveId, uint16_t number) {
    const Script* script = find(archiveId, number);
    if (!script) {
        char buf[48];
        std::snprintf(buf, sizeof buf, "script %04x.%u not found", archiveId, number);
        shellWarn(buf);
        return nullptr;
    }
    auto c = std::make_unique<ScriptContext>();
    c->script = script;
    running_.push_front(std::move(c));
    return running_.front().get();
}

void Scripts::killAll() {
    for (auto& c : running_) anims_.stopWaitedOnBy(c.get());
    running_.clear();
}

int32_t Scripts::execute(ScriptContext& c, const std::vector<uint8_t>& st) {
    const uint16_t command = u16(st, 0);
    const uint16_t present = u16(st, 2);
    auto slot = [&](int k) { return u16(st, 4 + 2 * static_cast<size_t>(k)); };
    auto given = [&](int k) { return (present >> k & 1) != 0; };

    switch (command) {
    case kStartAnim: {  // g07_04da
        ScriptContext* waiter = nullptr;
        if (given(3)) {
            waiter = &c;
            c.flags |= ScriptContext::kWaiting;
        }
        const int x = given(1) ? static_cast<int16_t>(slot(1)) : -1;
        const int y = given(1) ? static_cast<int16_t>(slot(2)) : -1;
        anims_.start(slot(0), x, y, waiter, given(4) ? static_cast<uint8_t>(slot(4)) : 0,
                     given(5) ? static_cast<uint8_t>(slot(5)) : 0, given(6), given(7) ? slot(7) : 0);
        return 0;
    }
    case kStopAnim:  // g07_05fa: ALL, or one anim
        if (given(1)) anims_.stopAll();
        else anims_.stop(slot(0));
        return 0;
    case kKillScript: {  // g07_037c: every context running that script ends
        const Script* target = find(slot(0), slot(1));
        for (auto& other : running_)
            if (target && other->script == target) other->flags |= ScriptContext::kFinished;
        return 0;
    }
    case kSleep:  // g07_0142
        c.flags |= ScriptContext::kSleeping;
        c.wakeTick = ctx_.gameTicks + slot(0);
        return 0;
    case kCall: {  // not used by the shipped scripts; slot layout inferred
        ScriptContext* child = start(slot(1), slot(2));
        if (child && !given(0)) {
            child->parent = &c;
            c.flags |= ScriptContext::kWaiting;
        }
        return 0;
    }
    default:
        shellWarn(std::string("script command not implemented: ") +
                  (command < kCommandCount ? kCommandNames[command] : "unknown"));
        return 0;
    }
}

bool Scripts::run(ScriptEvent& event) {
    const uint32_t deadline = ctx_.gameTicks + 2;
    for (;;) {
        // Report (and remove) the first finished script.
        for (auto it = running_.begin(); it != running_.end(); ++it) {
            ScriptContext& c = **it;
            if (!(c.flags & ScriptContext::kFinished)) continue;
            if (c.parent) c.parent->flags &= ~ScriptContext::kWaiting;
            event = ScriptEvent{};
            event.type = ScriptEvent::kFinished;
            event.archiveId = c.script->archiveId;
            event.number = c.script->number;
            anims_.stopWaitedOnBy(&c);
            for (auto& other : running_)
                if (other->parent == &c) other->parent = nullptr;
            running_.erase(it);
            return true;
        }

        // One statement per runnable script per pass.
        bool ranAny = false;
        for (auto& ptr : running_) {
            ScriptContext& c = *ptr;
            if (c.flags & ScriptContext::kSleeping) {
                if (c.wakeTick > ctx_.gameTicks) continue;
                c.flags &= ~ScriptContext::kSleeping;
            }
            if (c.flags & (ScriptContext::kWaiting | ScriptContext::kFinished)) continue;
            if (c.next >= c.script->statements.size()) {
                c.flags |= ScriptContext::kFinished;
                continue;
            }
            ranAny = true;
            const int32_t result = execute(c, c.script->statements[c.next++]);
            if (result != 0) {
                event = ScriptEvent{};
                event.type = ScriptEvent::kCommandResult;
                event.result = result;
                return true;
            }
            if (ctx_.gameTicks > deadline) break;
        }
        if (ctx_.gameTicks > deadline || !ranAny) {
            anims_.tick();
            return false;
        }
    }
}

}  // namespace edison
