#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace edison {

// Native port of Artech's FM sound driver (ADLIB*.DLL, CADLIB, MADLIB).
//
// All driver state lives in a copy of the DLL's 64 KB data segment at the
// original offsets: song data addresses it directly (jumps, calls, tables),
// and the games may peek and poke it. Behaviour is byte-exact with the
// original, including a few of its bugs that change the output; those are
// called out where they happen. tests/seqtest.cpp checks every sound against
// register logs recorded from the original driver. Routine and variable
// names follow the CodeView symbols shipped in the DLLs; see docs/SEQUENCER.md.
//
// Output is a stream of OPL2 register writes.
class ArtechFmDriver {
public:
    using OplWrite = std::function<void(uint8_t reg, uint8_t value)>;

    // Addresses that differ between the DLL builds (detected from code).
    struct Layout {
        uint16_t soundTables = 0;  // table of sound-table pointers
        uint16_t patchTable = 0;   // PATCH table (11-byte instruments)
        uint16_t motorTables = 0;  // motor duration/pitch table pointers
        uint16_t eventFlag = 0;    // GE_FLG counter (0 if this build lacks it)
        // Code addresses of the per-channel effect routines. Channels store
        // these in their data (+29/+2B), so they must match the build.
        uint16_t noVector = 0;     // plain RET: no effect
        uint16_t vibrato = 0;      // DOVIBRATO
        uint16_t tableMod = 0;     // TBLMOD
        uint16_t pitchDelta = 0;   // DOPITCHDELTA
        uint16_t effectReturn = 0; // return address of the first effect call (see doPitchDelta)
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
    // Rock and Bach's entries (ADLIB.DLL, ADLIB1-4.DLL).
    uint16_t getAddr(int which) const;              // GETADDR: 1/2 the sound table, 3 patches, 4 motor tables
    uint16_t getVar() const { return 0x52E; }       // GETVAR: the byte song data sets (the same in every build)
    uint8_t channelStatus(uint8_t channel);         // SSTATUS: the channel's ticks left (+05)
    void installPatch(uint8_t channel, uint8_t patch);  // INSTALL_PATCH (171C): WRITEPATCH on the channel
    void directDrumOut(uint8_t drums);              // DIRECTDRUMOUT (1749): key drums on in reg BD
    void playInstrument(uint8_t channel);           // PLAYINS (1787): key the channel off and on
    // The data-segment word, as the games read it through GETADDR's pointer.
    uint16_t peekWord(uint16_t addr) const { return word(addr); }
    void pokeWord(uint16_t addr, uint16_t value) { setWord(addr, value); }

    void setGlobalTempo(uint8_t tempo) { mem_[GLOBALTEMPO] = tempo; }
    bool idle() const;
    // Sounds sent but not started yet (the next update starts them).
    bool pending() const { return word(BUFFTAIL) != word(BUFFHEAD); }
    SoundList listSounds() const;

    // Data-segment address of a sound's header in the current sound table
    // (the address its debug-symbol name is attached to).
    uint16_t soundAddress(uint16_t id) const {
        return word(static_cast<uint16_t>(word(SOUNDTABLEPTR) + id * 2));
    }

    const Layout& layout() const { return layout_; }
    // Direct data-segment access, as the games have via GETADDR.
    uint8_t peek(uint16_t addr) const { return mem_[addr]; }
    void poke(uint16_t addr, uint8_t value) { mem_[addr] = value; }
    int unknownOpcodes() const { return unknownOpcodes_; }
    // How often each command opcode (0x00-0x52) has run; for test coverage.
    const std::array<uint32_t, 0x53>& opcodeCounts() const { return opcodeCounts_; }

private:
    // Data-segment variables at fixed addresses (identical in every build).
    enum : uint16_t {
        LASTDUR = 0x1E0,        // duration of the last note/rest parsed
        CURCHANNEL = 0x1E3,     // channel being processed
        DO_SOUND = 0x1E4,       // sequencer running
        DRUMMASK = 0x1E5,       // rhythm mode enabled / drums keyed (reg BD bits)
        DRUMBITS = 0x1E6,       // drums keyed by the last DODRUM
        GLOBALTEMPO = 0x1E7,
        MOTORFLAG = 0x1E8,
        MOTORDUR = 0x1E9,
        MOTORDCNT = 0x1EA,
        NOISEINDEX = 0x1EB,
        SEED = 0x1ED,
        STATE = 0x1EF,          // motor key toggle
        SHADOWBD = 0x1F0,       // AM/vibrato depth bits of reg BD
        YAMOFF = 0x1F1,         // operator offset of the current channel
        NOTESYNC = 0x201, NOTECNTR = 0x202, PRESYNC = 0x203, SYNCBYTE = 0x204, SYNCHI = 0x205,
        DURTABLEPTR = 0x206, PITCHTABLEPTR = 0x208,
        SOUNDTABLEPTR = 0x20A,
        SOUNDSYSTICK = 0x214,
        BUFFHEAD = 0x225, BUFFTAIL = 0x227, BUFF = 0x229,  // SENDSND queue
        SPRAM1 = 0x239,         // channel 0's block
        SCHNLPTR = 0x4D7,       // channel block pointers
        OPOFFSETS = 0x4EB,      // operator register offset per channel
        FREQTABLE = 0x4F4,      // 12 F-numbers
        GQ = 0x50C, GQ_W = 0x52C, GQ_R = 0x52D,  // game-event ring (32 entries)
    };

    // What the sequencer does after handling an event (0C5E / 0CC5 / 0CCB).
    enum class Flow { Parse, Effects, NextChannel };

    // View of one channel's 0x43-byte block in the data segment.
    class Channel {
    public:
        Channel(ArtechFmDriver& d, uint16_t base) : d_(d), base_(base) {}
        uint16_t base() const { return base_; }

        uint8_t& sysAttn() { return at(0x00); }        // SETSYSATTN (set from other channels)
        uint16_t ptr() const { return d_.word(addr(0x03)); }  // 0 = idle
        void setPtr(uint16_t v) { d_.setWord(addr(0x03), v); }
        uint8_t& ticks() { return at(0x05); }          // ticks left in the current event
        uint8_t& loopCount() { return at(0x06); }
        uint8_t& octave() { return at(0x07); }         // added to the note's octave nibble
        uint8_t& priority() { return at(0x08); }
        uint8_t& callDepth() { return at(0x09); }
        uint16_t returnSlot(uint8_t depth) const {     // stack of u16 at +0A
            return addr(static_cast<uint16_t>(0x0A + static_cast<uint8_t>(depth << 1)));
        }
        uint8_t& semitone() { return at(0x12); }
        uint8_t& slideSpeed() { return at(0x13); }
        uint8_t& slideAcc() { return at(0x14); }
        uint16_t slideDelta() const { return d_.word(addr(0x15)); }
        void setSlideDelta(uint16_t v) { d_.setWord(addr(0x15), v); }
        uint16_t vibratoDelta() const { return d_.word(addr(0x17)); }
        void setVibratoDelta(uint16_t v) { d_.setWord(addr(0x17), v); }
        uint8_t& vibratoDepth() { return at(0x19); }
        uint8_t& vibratoCount() { return at(0x1A); }   // ticks to the next direction change
        uint8_t& vibratoPeriod() { return at(0x1B); }
        uint8_t& vibratoDelay() { return at(0x1C); }
        uint8_t& vibratoRate() { return at(0x1D); }
        uint8_t& vibratoAcc() { return at(0x1E); }
        uint8_t& vibratoWait() { return at(0x1F); }    // delay countdown after key-on
        uint8_t& accentAttn() { return at(0x20); }
        uint8_t& gateTick() { return at(0x21); }       // key off when this many ticks remain
        uint8_t& detune() { return at(0x24); }
        uint8_t& tempo() { return at(0x25); }
        uint8_t& tempoAcc() { return at(0x26); }
        uint8_t& fnumLow() { return at(0x27); }        // shadow of reg A0+ch
        uint8_t& blockKey() { return at(0x28); }       // shadow of reg B0+ch
        uint16_t fnum() const { return d_.word(addr(0x27)) & 0x3FF; }
        uint16_t pitchEffect() const { return d_.word(addr(0x29)); }
        void setPitchEffect(uint16_t v) { d_.setWord(addr(0x29), v); }
        uint16_t registerEffect() const { return d_.word(addr(0x2B)); }
        void setRegisterEffect(uint16_t v) { d_.setWord(addr(0x2B), v); }
        uint8_t& gatePercent() { return at(0x2D); }    // key-off after n/8 of a note
        uint8_t& modLevel() { return at(0x2E); }       // patch KSL/TL, modulator
        uint8_t& carLevel() { return at(0x2F); }       // patch KSL/TL, carrier
        uint8_t& channelAttn() { return at(0x30); }
        uint8_t& additive() { return at(0x31); }       // connection bit of reg C0
        uint8_t& velocityModShift() { return at(0x32); }
        uint8_t& velocityCarShift() { return at(0x33); }
        uint8_t& velocity() { return at(0x34); }
        uint8_t& earlyOff() { return at(0x35); }       // SETGATETHRESHOLD
        uint8_t& randomDuration() { return at(0x36); }
        uint8_t& lfoRate() { return at(0x39); }
        uint8_t& lfoAcc() { return at(0x3A); }
        uint8_t& lfoLength() { return at(0x3B); }
        uint8_t& lfoIndex() { return at(0x3C); }
        uint8_t& lfoRegister() { return at(0x3D); }
        uint16_t lfoTable() const { return d_.word(addr(0x3E)); }
        void setLfoTable(uint16_t v) { d_.setWord(addr(0x3E), v); }
        uint8_t& autoTempo() { return at(0x40); }      // follow GLOBALTEMPO every tick
        uint8_t& lastNote() { return at(0x41); }
        uint8_t& bend() { return at(0x42); }

        // The original tests the words at +32 and +33, so velocity bytes are
        // expected whenever either shift or the last velocity is non-zero.
        bool expectsVelocity() const {
            return d_.word(addr(0x32)) != 0 || d_.word(addr(0x33)) != 0;
        }
        void clear();  // CLEARSPRAM

    private:
        uint16_t addr(uint16_t off) const { return static_cast<uint16_t>(base_ + off); }
        uint8_t& at(uint16_t off) { return d_.mem_[addr(off)]; }

        ArtechFmDriver& d_;
        uint16_t base_;
    };

    // --- data segment ------------------------------------------------------------
    uint8_t& byte(uint16_t a) { return mem_[a]; }
    uint16_t word(uint16_t a) const {
        return static_cast<uint16_t>(mem_[a] | (mem_[static_cast<uint16_t>(a + 1)] << 8));
    }
    void setWord(uint16_t a, uint16_t v) {
        mem_[a] = static_cast<uint8_t>(v);
        mem_[static_cast<uint16_t>(a + 1)] = static_cast<uint8_t>(v >> 8);
    }
    uint8_t currentChannel() const { return mem_[CURCHANNEL]; }
    Channel channel(uint8_t index) {  // index is doubled in 8 bits, as the original does
        return Channel(*this, word(static_cast<uint16_t>(SCHNLPTR + static_cast<uint8_t>(index << 1))));
    }
    uint8_t opOffset(uint8_t channelIndex) {
        return mem_[static_cast<uint16_t>(0x0400 | static_cast<uint8_t>(OPOFFSETS + channelIndex))];
    }
    uint8_t nextByte() { return mem_[pc_++]; }
    uint16_t nextWord() { const uint16_t v = word(pc_); pc_ = static_cast<uint16_t>(pc_ + 2); return v; }

    void writeReg(uint8_t reg, uint8_t value) {  // OUTADLIB
        if (opl_) opl_(reg, value);
    }

    // --- driver routines (ADLIB.DLL addresses) ------------------------------------
    void resetSound();                                  // 0B9D
    void startQueued();                                 // 0A37  drain the SENDSND queue
    void startSound(uint16_t id, bool fromQueue);       // 0A46 / STARTVOICE
    void tickChannels();                                // 0BF1
    Flow stepChannel(Channel& c);                       // 0C14
    Flow parse(Channel& c);                             // 0C5B  PARSE
    Flow playNote(Channel& c, uint8_t note, uint8_t duration);  // NOTE
    Flow execute(Channel& c, uint8_t op, uint8_t param);        // opcode handlers
    Flow endOfStream(Channel& c);                       // EOS
    void syncTick();                                    // 17E0
    void setFrequency(Channel& c, uint8_t note, bool bendCommand);  // 14F0 / SETPITCHBEND
    void keyOn(Channel& c);                             // 15A5
    int keyOff(Channel& c);                             // 15D5: returns value written or -1
    void setDuration(Channel& c, uint8_t duration);     // 15FD
    uint16_t random();                                  // 1ADE  RANDOM
    uint8_t carrierLevel(Channel& c, uint8_t& opOut);   // 1879
    uint8_t modulatorLevel(Channel& c, uint8_t& opOut); // 18B1
    void updateLevels(Channel& c);                      // 184A
    void writePatch(Channel& c, uint16_t patch, uint8_t op);  // 1922  WRITEPATCH
    void silenceVoice(uint8_t channelIndex);            // 198E
    // Effects return true when they abort the rest of the tick (see doPitchDelta).
    bool runEffect(Channel& c, uint16_t routine, uint16_t returnAddress);
    void doVibrato(Channel& c);                                   // 19D8  DOVIBRATO
    void tableMod(Channel& c);                                    // 1A31  TBLMOD
    bool doPitchDelta(Channel& c, uint16_t returnAddress);        // 1A5A  DOPITCHDELTA
    void pushEvent(uint8_t value);
    void drumLevels(uint8_t mask, uint8_t value, int mode);  // DRUMATTN / DRUMFADE / DRUMMASTERATTN

    std::array<uint8_t, 0x10000> mem_{};
    Layout layout_;
    OplWrite opl_;
    uint16_t pc_ = 0;  // event stream read position (SI in the original)
    int unknownOpcodes_ = 0;
    std::array<uint32_t, 0x53> opcodeCounts_{};
};

}  // namespace edison
