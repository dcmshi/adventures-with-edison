#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace edison {

// Native port of Artech's FM sound driver (ADLIB*.DLL, CADLIB, MADLIB).
//
// This is a faithful translation of the original 16-bit assembly: CPU
// registers are modelled explicitly and all state lives in a copy of the
// DLL's 64 KB data segment at the original offsets, because song data uses
// absolute addresses into that segment and the music depends on the
// original's register-clobbering quirks. See docs/SEQUENCER.md; routine and
// variable names come from the CodeView symbols shipped in the DLLs.
//
// Output is a stream of OPL2 register writes, to be fed to an OPL emulator.
class ArtechFmDriver {
public:
    using OplWrite = std::function<void(uint8_t reg, uint8_t value)>;

    // Addresses that differ between the DLL builds (detected from code).
    struct Layout {
        uint16_t soundTables = 0;  // table of sound-table pointers
        uint16_t patchTable = 0;   // PATCH table (11-byte instruments)
        uint16_t motorTables = 0;  // motor duration/pitch table pointers
        uint16_t eventFlag = 0;    // GE_FLG counter (0 if this build lacks it)
    };

    struct SoundList {
        std::vector<uint16_t> sounds;    // ids startable with sendSound()
        std::vector<uint16_t> snippets;  // phrase entries used by the instrument player
    };

    bool loadDll(const std::string& path, std::string* error = nullptr);
    void setOplWrite(OplWrite fn) { opl_ = std::move(fn); }

    // Exported driver API (INIT_ADLIB, REMOVE_ADLIB, UPDATE_ADLIB, ...).
    void init();
    void remove();
    void update();  // one timer tick
    void sendSound(uint16_t id);
    void switchSoundTable(uint16_t table);
    void setMotor(uint16_t value);
    uint8_t nextEvent();  // GEVENT: 0 when the game-event queue is empty
    void flushEvents();   // GFLUSH

    void setGlobalTempo(uint8_t tempo) { mem_[GLOBALTEMPO] = tempo; }
    bool idle() const;
    SoundList listSounds() const;

    const Layout& layout() const { return layout_; }
    uint8_t peek(uint16_t addr) const { return mem_[addr]; }
    int unknownOpcodes() const { return unknownOpcodes_; }

private:
    // Fixed data-segment addresses (identical in every build).
    enum : uint16_t {
        LASTDUR = 0x1E0, CURCHANNEL = 0x1E3, DO_SOUND = 0x1E4, DRUMMASK = 0x1E5,
        DRUMBITS = 0x1E6, GLOBALTEMPO = 0x1E7, MOTORFLAG = 0x1E8, MOTORDUR = 0x1E9,
        MOTORDCNT = 0x1EA, NOISEINDEX = 0x1EB, SEED = 0x1ED, STATE = 0x1EF,
        SHADOWBD = 0x1F0, YAMOFF = 0x1F1, NOTESYNC = 0x201, NOTECNTR = 0x202,
        PRESYNC = 0x203, SYNCBYTE = 0x204, SYNCHI = 0x205, DURTABLEPTR = 0x206,
        PITCHTABLEPTR = 0x208, SOUNDTABLEPTR = 0x20A, SOUNDSYSTICK = 0x214,
        BUFFHEAD = 0x225, BUFFTAIL = 0x227, BUFF = 0x229, SPRAM1 = 0x239,
        SCHNLPTR = 0x4D7, OPOFFSETS = 0x4EB, FREQTABLE = 0x4F4, GQ = 0x50C,
        GQ_W = 0x52C, GQ_R = 0x52D,
    };

    // Per-channel effect routines are stored in channel memory as the
    // original code addresses; these are ADLIB.DLL's values.
    enum : uint16_t {
        NOVECTOR = 0x19D7, DOVIBRATO = 0x19D8, TBLMOD = 0x1A31, DOPITCHDELTA = 0x1A5A,
    };

    enum class Flow { Parse, Effects, NextChannel };

    // --- memory and registers ------------------------------------------------
    uint8_t& m8(uint16_t a) { return mem_[a]; }
    uint16_t m16(uint16_t a) const {
        return static_cast<uint16_t>(mem_[a] | (mem_[static_cast<uint16_t>(a + 1)] << 8));
    }
    void w16(uint16_t a, uint16_t v) {
        mem_[a] = static_cast<uint8_t>(v);
        mem_[static_cast<uint16_t>(a + 1)] = static_cast<uint8_t>(v >> 8);
    }
    uint8_t lodsb() { return mem_[si++]; }
    uint16_t lodsw() { uint16_t v = m16(si); si += 2; return v; }

    uint16_t ax() const { return static_cast<uint16_t>(al | (ah << 8)); }
    uint16_t bx() const { return static_cast<uint16_t>(bl | (bh << 8)); }
    uint16_t cx() const { return static_cast<uint16_t>(cl | (ch << 8)); }
    void setAx(uint16_t v) { al = static_cast<uint8_t>(v); ah = static_cast<uint8_t>(v >> 8); }
    void setBx(uint16_t v) { bl = static_cast<uint8_t>(v); bh = static_cast<uint8_t>(v >> 8); }
    void setCx(uint16_t v) { cl = static_cast<uint8_t>(v); ch = static_cast<uint8_t>(v >> 8); }
    void setDx(uint16_t v) { dl = static_cast<uint8_t>(v); dh = static_cast<uint8_t>(v >> 8); }

    void outAdlib();  // OUTADLIB: register AH <- AL

    // --- driver routines (ADLIB.DLL addresses) -------------------------------
    void resetSound();        // 0B9D
    void startQueued();       // 0A37  drain SENDSND queue
    void tickChannels();      // 0BF1  sequencer tick, channels 9..0
    Flow channelStep();       // 0C14
    Flow parse();             // 0C5B  PARSE
    Flow dispatch(uint8_t op);
    void syncTick();          // 17E0
    void setFrequency();      // 14F0
    void keyOn();             // 15A5
    void keyOff();            // 15D5
    void setDuration();       // 15FD
    void random();            // 1ADE  RANDOM
    void carrierLevel();      // 1879
    void modulatorLevel();    // 18B1
    void updateLevels();      // 184A
    void clearSpram();        // 18EF  CLEARSPRAM
    void writePatch();        // 1922  WRITEPATCH
    void silenceVoice();      // 198E
    void callVector(uint16_t routine);
    void doVibrato();         // 19D8  DOVIBRATO
    void tableMod();          // 1A31  TBLMOD
    void doPitchDelta();      // 1A5A  DOPITCHDELTA
    void pushEvent(uint8_t value);
    void drumLevel(uint8_t mask, uint8_t bit, uint8_t reg, uint16_t shadow,
                   uint16_t master, uint16_t attn, int mode);

    std::array<uint8_t, 0x10000> mem_{};
    std::array<uint8_t, 0x10000> pristine_{};
    Layout layout_;
    OplWrite opl_;
    uint8_t al = 0, ah = 0, bl = 0, bh = 0, cl = 0, ch = 0, dl = 0, dh = 0;
    uint16_t si = 0, di = 0;
    int unknownOpcodes_ = 0;
};

}  // namespace edison
