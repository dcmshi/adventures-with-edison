#pragma once

#include <cstdint>
#include <list>
#include <map>
#include <memory>
#include <vector>

namespace edison {

struct GameContext;
class Anims;

// A compiled script (archive group 03, see tools/scripts.py).
struct Script {
    uint16_t archiveId = 0;  // key: archiveId | number << 16
    uint16_t number = 0;
    std::vector<std::vector<uint8_t>> statements;  // u16 command, u16 present, slots
};

// A running script (EDISON.EXE's 0x13-byte context).
struct ScriptContext {
    enum Flags : uint8_t { kSleeping = 1, kFinished = 2, kWaiting = 4 };
    uint8_t flags = 0;
    uint32_t wakeTick = 0;
    ScriptContext* parent = nullptr;
    const Script* script = nullptr;
    size_t next = 0;  // statement index
};

// What run() reports, like the event record f06_0404 fills in.
struct ScriptEvent {
    enum Type { kNone = 0, kFinished = 4, kCommandResult = 6 } type = kNone;
    uint16_t archiveId = 0;  // of the script that finished
    uint16_t number = 0;
    int32_t result = 0;
};

// The cooperative script scheduler (EDISON.EXE segment 6, handlers in 7).
class Scripts {
public:
    Scripts(GameContext& ctx, Anims& anims) : ctx_(ctx), anims_(anims) {}

    // StartScript (f06_033e): loads archive entry `archiveId` if needed.
    ScriptContext* start(uint16_t archiveId, uint16_t number);
    // One scheduler pass (f06_0404): reports at most one event, then ticks
    // the anims. Returns true if `event` was filled in.
    bool run(ScriptEvent& event);
    void killAll();

private:
    const Script* find(uint16_t archiveId, uint16_t number);
    int32_t execute(ScriptContext& c, const std::vector<uint8_t>& statement);

    GameContext& ctx_;
    Anims& anims_;
    std::map<uint32_t, std::unique_ptr<Script>> loaded_;
    std::list<std::unique_ptr<ScriptContext>> running_;  // newest first
};

}  // namespace edison
