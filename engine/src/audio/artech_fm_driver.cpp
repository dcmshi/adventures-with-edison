#include "audio/artech_fm_driver.h"

#include <algorithm>

#include "formats/ne_file.h"

namespace edison {

namespace {

constexpr int X = 0x100;  // wildcard byte in code patterns

// Offset of the first match of `pattern` in `code` (0x100 = any byte), or -1.
long findPattern(const std::vector<uint8_t>& code, std::initializer_list<int> pattern) {
    const std::vector<int> p(pattern);
    for (size_t i = 0; i + p.size() <= code.size(); ++i) {
        size_t k = 0;
        while (k < p.size() && (p[k] == X || code[i + k] == p[k])) ++k;
        if (k == p.size()) return static_cast<long>(i);
    }
    return -1;
}

uint16_t le16(const std::vector<uint8_t>& d, long at) {
    return static_cast<uint16_t>(d[at] | (d[at + 1] << 8));
}

// 16-bit shifts as the 386+ the game ran on performs them (count & 31).
uint16_t shl16(uint16_t v, uint8_t count) {
    const unsigned n = count & 31u;
    return n >= 16 ? 0 : static_cast<uint16_t>(v << n);
}
uint16_t shr16(uint16_t v, uint8_t count) {
    const unsigned n = count & 31u;
    return n >= 16 ? 0 : static_cast<uint16_t>(v >> n);
}

// acc += step (8-bit); true on carry. The driver's fractional timers.
bool accumulate(uint8_t& acc, uint8_t step) {
    const unsigned sum = acc + step;
    acc = static_cast<uint8_t>(sum);
    return sum > 0xFF;
}

// Operator levels (6-bit attenuation) clamp: overflow past 63 -> 63,
// wrap below zero -> 0.
uint8_t clampLevel(uint8_t v) {
    if (v <= 0x3F) return v;
    return static_cast<int8_t>(v) < 0 ? 0 : 0x3F;
}

uint8_t u8(unsigned v) { return static_cast<uint8_t>(v); }

}  // namespace

// --- loading -----------------------------------------------------------------

bool ArtechFmDriver::loadDll(const std::string& path, std::string* error) {
    NeFile ne;
    if (!ne.load(path, error)) return false;
    // The driver's code: the segment with its tick (0BF1: mov byte [CURCHANNEL],9 /
    // mov bl,[CURCHANNEL] / mov bh,0 / shl bl,1), its data the next one. The
    // ADLIB family has them in 1 and 2; SADLIB has Borland C++ wrappers in 1.
    std::vector<uint8_t> code, data;
    long tick = -1;
    for (int s = 1; s < ne.segmentCount() && tick < 0; ++s) {
        code = ne.segment(s);
        tick = findPattern(code, {0xC6, 0x06, X, X, 0x09, 0x8A, 0x1E, X, X, 0xB7, 0x00, 0xD0, 0xE3});
        if (tick >= 0) data = ne.segment(s + 1);
    }
    if (tick < 0 || data.empty()) {
        if (error) *error = path + ": not an Artech ADLIB-family driver";
        return false;
    }
    relocate(static_cast<uint16_t>(le16(code, tick + 2) - 0x1E3));
    const auto lo = [](uint16_t a) { return a & 0xFF; };
    const auto hi = [](uint16_t a) { return a >> 8; };

    // SWITCHSOUNDTABLE: shl bx,1 / mov bx,[bx+TABLES] / mov [SOUNDTABLEPTR],bx
    const long tables = findPattern(code, {0xD1, 0xE3, 0x8B, 0x9F, X, X, 0x89, 0x1E, lo(SOUNDTABLEPTR), hi(SOUNDTABLEPTR)});
    // STUFFPATCH: mov cl,[YAMOFF] / sub bx,bx / mov bl,ah / shl bx,1 / mov si,[bx+PATCH]
    const long patch = findPattern(code, {0x8A, 0x0E, lo(YAMOFF), hi(YAMOFF), 0x2B, 0xDB, 0x8A, 0xDC, 0xD1, 0xE3, 0x8B, 0xB7, X, X});
    // MOTORON: mov bx,[bx+MOTORTABLES] / mov [DURTABLEPTR],bx
    const long motor = findPattern(code, {0x8B, 0x9F, X, X, 0x89, 0x1E, lo(DURTABLEPTR), hi(DURTABLEPTR)});
    // op 50: inc byte [GE_FLG] / xor bx,bx / mov bl,[GQ_W]  (absent in some builds)
    const long geflag = findPattern(code, {0xFE, 0x06, X, X, 0x33, 0xDB, 0x8A, 0x1E});
    // Effect routine addresses, from the code that installs them:
    // CLEARSPRAM: mov byte [di+25],FF / mov byte [di+8],0 / mov word [di+29],NOVECTOR
    const long noVector = findPattern(code, {0xC6, 0x45, 0x25, 0xFF, 0xC6, 0x45, 0x08, 0x00, 0xC7, 0x45, 0x29, X, X});
    // VIBRATO: mov [di+1C],al / mov word [di+29],DOVIBRATO
    const long vibrato = findPattern(code, {0x88, 0x45, 0x1C, 0xC7, 0x45, 0x29, X, X});
    // TABLEMODOP: mov [di+3E],ax / mov word [di+2B],TBLMOD
    const long tableMod = findPattern(code, {0x89, 0x45, 0x3E, 0xC7, 0x45, 0x2B, X, X});
    // PITCHDELTA: mov word [di+29],DOPITCHDELTA / mov byte [di+14],FF
    const long pitchDelta = findPattern(code, {0xC7, 0x45, 0x29, X, X, 0xC6, 0x45, 0x14, 0xFF});
    // Tick loop: call [di+29] / call [di+2B]
    const long effectCall = findPattern(code, {0xFF, 0x55, 0x29, 0xFF, 0x55, 0x2B});
    if (tables < 0 || patch < 0 || motor < 0 || noVector < 0 || vibrato < 0 || tableMod < 0 ||
        pitchDelta < 0 || effectCall < 0) {
        if (error) *error = path + ": not an Artech ADLIB-family driver";
        return false;
    }
    layout_.noVector = le16(code, noVector + 11);
    layout_.vibrato = le16(code, vibrato + 6);
    layout_.tableMod = le16(code, tableMod + 6);
    layout_.pitchDelta = le16(code, pitchDelta + 3);
    layout_.effectReturn = static_cast<uint16_t>(effectCall + 3);
    layout_.soundTables = le16(code, tables + 4);
    layout_.patchTable = le16(code, patch + 12);
    layout_.motorTables = le16(code, motor + 2);
    layout_.eventFlag = geflag >= 0 ? le16(code, geflag + 2) : 0;

    mem_.fill(0);
    std::copy(data.begin(), data.end(), mem_.begin());
    unknownOpcodes_ = 0;
    return true;
}

void ArtechFmDriver::relocate(uint16_t base) {
    const uint16_t by = static_cast<uint16_t>(base - base_);
    for (uint16_t* v : {&BENDTABLES, &LASTDUR, &CURCHANNEL, &DO_SOUND, &DRUMMASK, &DRUMBITS, &GLOBALTEMPO,
                        &MOTORFLAG, &MOTORDUR, &MOTORDCNT, &NOISEINDEX, &SEED, &STATE, &SHADOWBD, &YAMOFF,
                        &NOTESYNC, &NOTECNTR, &PRESYNC, &SYNCBYTE, &SYNCHI, &DURTABLEPTR, &PITCHTABLEPTR,
                        &SOUNDTABLEPTR, &SOUNDSYSTICK, &BUFFHEAD, &BUFFTAIL, &BUFF, &SPRAM1, &SCHNLPTR,
                        &OPOFFSETS, &FREQTABLE, &GQ, &GQ_W, &GQ_R})
        *v = static_cast<uint16_t>(*v + by);
    base_ = base;
}

// --- exported API --------------------------------------------------------------

void ArtechFmDriver::init() {  // 1B94
    setWord(SOUNDTABLEPTR, word(layout_.soundTables));
    resetSound();
    setWord(BUFFTAIL, 0);
    setWord(BUFFHEAD, 0);
}

void ArtechFmDriver::remove() { resetSound(); }  // 1BC7

void ArtechFmDriver::sendSound(uint16_t id) {  // 0A13
    // Quirk: the id is stored as a word at a *byte* index, so each entry
    // overlaps the next one's low byte. Only low bytes are read back.
    const uint16_t head = word(BUFFHEAD);
    setWord(static_cast<uint16_t>(BUFF + head), id);
    setWord(BUFFHEAD, static_cast<uint16_t>((head + 1) & 0xF));
}

void ArtechFmDriver::switchSoundTable(uint16_t table) {  // 0AED
    setWord(SOUNDTABLEPTR, word(static_cast<uint16_t>(layout_.soundTables + table * 2)));
}

void ArtechFmDriver::setMotor(uint16_t value) {  // 0AA4
    setWord(NOISEINDEX, value);
    const uint16_t index = value & 0x7FFF;
    byte(MOTORDUR) = byte(static_cast<uint16_t>(word(DURTABLEPTR) + index));
    writeReg(0xA0, byte(static_cast<uint16_t>(word(PITCHTABLEPTR) + index)));
}

uint8_t ArtechFmDriver::nextEvent() {  // 0B0B
    const uint8_t read = byte(GQ_R);
    const uint8_t value = byte(static_cast<uint16_t>(GQ + read));
    if (value != 0) {
        byte(static_cast<uint16_t>(GQ + read)) = 0;
        byte(GQ_R) = read + 1 == 0x20 ? 0 : u8(read + 1);
    }
    return value;
}

void ArtechFmDriver::flushEvents() {  // 0B3B
    // The original's clear loop never advances its index (it rewrites GQ_W
    // 32 times), so a flush only resets the read/write positions.
    byte(GQ_R) = 0;
    byte(GQ_W) = 0;
}

uint16_t ArtechFmDriver::getAddr(int which) const {  // 0B5F (0 is the data segment itself)
    switch (which) {
    case 1: case 2: return word(SOUNDTABLEPTR);
    case 3: return layout_.patchTable;
    case 4: return layout_.motorTables;
    default: return 0;
    }
}

uint8_t ArtechFmDriver::channelStatus(uint8_t channel) {  // 180A
    return this->channel(channel).ticks();
}

void ArtechFmDriver::installPatch(uint8_t channel, uint8_t patch) {  // 171C
    Channel c = this->channel(channel);
    writePatch(c, word(static_cast<uint16_t>(layout_.patchTable + patch * 2)),
               byte(static_cast<uint16_t>(OPOFFSETS + channel)));
}

void ArtechFmDriver::directDrumOut(uint8_t drums) {  // 1749
    // Keyed-off first (without the depth bits), then on.
    writeReg(0xBD, u8((~(drums & 0x1F) & byte(DRUMMASK)) | 0x20));
    byte(DRUMMASK) = u8(byte(DRUMMASK) | drums);
    writeReg(0xBD, u8(byte(DRUMMASK) | byte(SHADOWBD) | 0x20));
}

void ArtechFmDriver::playInstrument(uint8_t channel) {  // 1787
    Channel c = this->channel(channel);
    c.blockKey() = u8(c.blockKey() & 0xDF);
    writeReg(u8(0xB0 + channel), c.blockKey());
    c.blockKey() = u8(c.blockKey() | 0x20);
    writeReg(u8(0xB0 + channel), c.blockKey());
}

void ArtechFmDriver::update() {  // 1BD2  UPDATE_ADLIB
    if (byte(MOTORFLAG) != 0 && --byte(MOTORDCNT) == 0) {
        // "Motor" effect: toggle channel 0's key bit at MOTORDUR ticks.
        uint8_t& b0 = byte(SPRAM1 + 0x28);
        byte(STATE) ^= 0xFF;
        b0 = u8((b0 & 0x1F) | (byte(STATE) & 0x20));
        writeReg(0xB0, b0);
        byte(MOTORDCNT) = byte(MOTORDUR);
    }
    if (--byte(SOUNDSYSTICK) == 0) {  // divider; stays at 1 after the first tick
        byte(SOUNDSYSTICK) = 1;
        startQueued();  // 09FF
        tickChannels();
        syncTick();
    }
}

bool ArtechFmDriver::idle() const {
    for (int ch = 0; ch < 10; ++ch) {
        const uint16_t base = word(static_cast<uint16_t>(SCHNLPTR + 2 * ch));
        if (mem_[static_cast<uint16_t>(base + 5)] != 0 && word(static_cast<uint16_t>(base + 3)) != 0) return false;
    }
    return true;
}

ArtechFmDriver::SoundList ArtechFmDriver::listSounds() const {
    // The table has no stored length: it ends at the first entry pointing
    // below the driver variables, or where the earliest pointed-at data begins.
    SoundList list;
    const uint16_t table = word(SOUNDTABLEPTR);
    uint32_t bound = 0x8000;
    for (uint32_t i = 0; i < bound; ++i) {
        const uint16_t p = word(static_cast<uint16_t>(table + 2 * i));
        if (p <= layout_.soundTables) break;
        if (p > table) bound = std::min<uint32_t>(bound, (p - table) / 2u);
        (mem_[p] <= 9 ? list.sounds : list.snippets).push_back(static_cast<uint16_t>(i));
    }
    return list;
}

// --- starting and stopping ----------------------------------------------------------

void ArtechFmDriver::Channel::clear() {  // 18EF  CLEARSPRAM
    for (uint16_t off = 0x03; off < 0x43; ++off) at(off) = 0;
    tempo() = 0xFF;
    priority() = 0;
    setPitchEffect(d_.layout_.noVector);
    setRegisterEffect(d_.layout_.noVector);
    earlyOff() = 1;
}

void ArtechFmDriver::resetSound() {  // 0B9D
    byte(DO_SOUND) = 0;
    setWord(SEED, 0x1234);
    writeReg(0x01, 0x20);  // enable waveform select
    writeReg(0x08, 0x00);
    writeReg(0xBD, 0x00);
    byte(MOTORFLAG) = 0;
    for (int ch = 9; ch >= 0; --ch) {
        if (ch != 9) {
            writeReg(u8(opOffset(u8(ch)) + 0x40), 0x3F);
            writeReg(u8(opOffset(u8(ch)) + 0x43), 0x3F);
        }
        channel(u8(ch)).clear();
    }
}

void ArtechFmDriver::startQueued() {  // 0A37
    while (word(BUFFTAIL) != word(BUFFHEAD)) {
        startSound(byte(static_cast<uint16_t>(BUFF + word(BUFFTAIL))), true);
        setWord(BUFFTAIL, static_cast<uint16_t>((word(BUFFTAIL) + 1) & 0xF));
    }
}

void ArtechFmDriver::startSound(uint16_t id, bool fromQueue) {
    const uint16_t header = word(static_cast<uint16_t>(word(SOUNDTABLEPTR) + id * 2));
    const uint8_t index = byte(header);
    const uint8_t priority = byte(static_cast<uint16_t>(header + 1));
    if (fromQueue && index == 0) byte(MOTORFLAG) = 0;
    Channel target(*this, word(static_cast<uint16_t>(SCHNLPTR + index * 2)));
    if (static_cast<int8_t>(priority) < static_cast<int8_t>(target.priority())) return;
    target.clear();
    target.priority() = priority;
    target.setPtr(static_cast<uint16_t>(header + 2));
    target.tempo() = 0xFF;
    target.tempoAcc() = 0xFF;
    target.ticks() = 1;
    byte(DO_SOUND) = 1;
    // SENDSND leaves channel 9 (the control channel) alone; STARTVOICE doesn't check.
    if (!fromQueue || index != 9) silenceVoice(index);
}

void ArtechFmDriver::silenceVoice(uint8_t index) {  // 198E
    if (byte(DRUMMASK) != 0 && static_cast<int8_t>(index) >= 6) return;
    const uint8_t op = opOffset(index);
    for (uint8_t reg : {0x60, 0x63, 0x80, 0x83}) writeReg(u8(reg + op), 0xFF);
    writeReg(u8(0xB0 + index), 0x00);
    writeReg(u8(0xB0 + index), 0x20);  // sic: the original keys the voice on again
}

// --- sequencer tick ------------------------------------------------------------------

void ArtechFmDriver::syncTick() {  // 17E0: bar counter for INITSYNC/WAITSYNC
    if (!accumulate(byte(PRESYNC), byte(GLOBALTEMPO))) return;
    if (--byte(NOTECNTR) != 0) return;
    byte(NOTECNTR) = byte(NOTESYNC);
    ++byte(SYNCBYTE);
}

void ArtechFmDriver::tickChannels() {  // 0BF1
    if (byte(DO_SOUND) == 0) return;
    byte(CURCHANNEL) = 9;
    do {
        Channel c = channel(currentChannel());
        const Flow flow = c.ticks() != 0 ? stepChannel(c) : Flow::NextChannel;
        if (flow == Flow::Effects) {
            // call [di+29] / call [di+2B]: each call instruction is 3 bytes.
            if (runEffect(c, c.pitchEffect(), layout_.effectReturn)) return;
            if (runEffect(c, c.registerEffect(), static_cast<uint16_t>(layout_.effectReturn + 3))) return;
        }
    } while (static_cast<int8_t>(--byte(CURCHANNEL)) >= 0);
}

ArtechFmDriver::Flow ArtechFmDriver::stepChannel(Channel& c) {  // 0C14
    byte(YAMOFF) = opOffset(currentChannel());
    if (c.autoTempo()) c.tempo() = byte(GLOBALTEMPO);
    if (!accumulate(c.tempoAcc(), c.tempo())) return Flow::Effects;
    if (--c.ticks() == 0) return parse(c);

    uint8_t remaining = c.ticks();
    if (remaining == c.gateTick()) {
        // Quirk: the original compares AL again below, and a key-off in
        // between has overwritten AL with the B0 value it wrote.
        const int written = keyOff(c);
        if (written >= 0) remaining = u8(written);
    }
    if (remaining == c.earlyOff() && currentChannel() != 9) keyOff(c);
    return Flow::Effects;
}

ArtechFmDriver::Flow ArtechFmDriver::parse(Channel& c) {  // 0C5B
    pc_ = c.ptr();
    for (;;) {
        const uint8_t lo = nextByte();
        const uint8_t hi = nextByte();
        Flow flow;
        if ((lo & 0x80) == 0) {
            flow = playNote(c, lo, hi);
        } else {
            const uint8_t op = lo & 0x7F;
            if (op > 0x52) {
                // 0x53..0x70 index past the jump table into unrelated code in
                // the original; > 0x70 is EOS. Songs that use them still match.
                if (op <= 0x70) ++unknownOpcodes_;
                flow = endOfStream(c);
            } else {
                ++opcodeCounts_[op];
                flow = execute(c, op, hi);
            }
        }
        if (flow != Flow::Parse) return flow;
    }
}

ArtechFmDriver::Flow ArtechFmDriver::playNote(Channel& c, uint8_t note, uint8_t duration) {
    setFrequency(c, note, false);
    keyOn(c);
    setDuration(c, duration);
    if (c.expectsVelocity()) {  // VELOCITYON
        c.velocity() = nextByte();
        uint8_t unused;
        writeReg(u8(byte(YAMOFF) + 0x43), carrierLevel(c, unused));
        writeReg(u8(byte(YAMOFF) + 0x40), modulatorLevel(c, unused));
    }
    if (byte(LASTDUR) == 0) return Flow::Parse;  // zero duration: keep reading
    c.setPtr(pc_);
    return Flow::Effects;
}

ArtechFmDriver::Flow ArtechFmDriver::endOfStream(Channel& c) {  // EOS
    c.priority() = 0;
    c.setPtr(0);
    if (currentChannel() != 9) keyOff(c);
    return Flow::NextChannel;
}

// --- notes ---------------------------------------------------------------------------

// Note byte: high nibble octave, low nibble semitone. Writes A0/B0 (no key-on).
// SETPITCHBEND re-derives the current note and, unlike the note path,
// applies the bend table even when the bend is 0.
void ArtechFmDriver::setFrequency(Channel& c, uint8_t note, bool bendCommand) {  // 14F0
    c.lastNote() = note;
    uint8_t block = u8((note & 0xF0) + c.octave());
    uint8_t semitone = u8((note & 0x0F) + c.semitone());
    if (static_cast<int8_t>(semitone) >= 12) {
        semitone = u8(semitone - 12);
        block = u8(block + 0x10);
    } else if (static_cast<int8_t>(semitone) < 0) {
        semitone = u8(semitone + 12);
        block = u8(block - 0x10);
    }
    const uint16_t fnum = static_cast<uint16_t>(word(u8(semitone << 1) + FREQTABLE) + c.detune());
    uint16_t freq = static_cast<uint16_t>((((block >> 2) & 0x1C) | (fnum >> 8)) << 8 | (fnum & 0xFF));

    const uint8_t bend = c.bend();
    if (bend != 0 || bendCommand) {
        // Per-semitone bend tables: pointers at BENDTABLES (up uses the entry
        // two semitones higher).
        const uint8_t semi = c.lastNote() & 0x0F;
        if (static_cast<int8_t>(bend) >= 0) {
            const uint16_t table = word(static_cast<uint16_t>(BENDTABLES + ((semi + 2) << 1)));
            freq = static_cast<uint16_t>(freq + byte(static_cast<uint16_t>(table + bend)));
        } else {
            const uint16_t table = word(static_cast<uint16_t>(BENDTABLES + (semi << 1)));
            freq = static_cast<uint16_t>(freq - byte(static_cast<uint16_t>(table + u8(-bend))));
        }
    }
    const uint8_t keyBit = c.blockKey() & 0x20;
    c.blockKey() = u8((freq >> 8) | keyBit);
    c.fnumLow() = u8(freq);
    writeReg(u8(0xA0 + currentChannel()), c.fnumLow());
    writeReg(u8(0xB0 + currentChannel()), c.blockKey());
}

void ArtechFmDriver::keyOn(Channel& c) {  // 15A5
    c.blockKey() |= 0x20;
    writeReg(u8(0xB0 + currentChannel()), c.blockKey());
    // Vibrato step = F-number >> (9 - depth); only the low byte is kept.
    c.setVibratoDelta(u8(shr16(c.fnum(), u8(9 - c.vibratoDepth()))));
    c.vibratoWait() = c.vibratoDelay();
}

int ArtechFmDriver::keyOff(Channel& c) {  // 15D5
    if (byte(DRUMMASK) != 0 && static_cast<int8_t>(currentChannel()) >= 6) return -1;
    if (currentChannel() == 9) return -1;
    c.blockKey() &= 0xDF;
    writeReg(u8(0xB0 + currentChannel()), c.blockKey());
    return c.blockKey();
}

void ArtechFmDriver::setDuration(Channel& c, uint8_t duration) {  // 15FD
    byte(LASTDUR) = duration;
    if (c.randomDuration() != 0) {
        c.ticks() = u8(duration + (random() & c.randomDuration()));
        return;  // (the gate tick is not updated in this case)
    }
    if (c.gatePercent() != 0) c.gateTick() = u8((duration >> 3) * c.gatePercent());
    c.ticks() = duration;
}

uint16_t ArtechFmDriver::random() {  // 1ADE
    uint16_t v = static_cast<uint16_t>(word(SEED) + 0x9248);
    v = static_cast<uint16_t>((v >> 3) | (v << 13));
    setWord(SEED, v);
    return v;
}

// --- operator levels -------------------------------------------------------------------
//
// level = patch TL + accent + channel + system attenuation - velocity scaling.
// Velocity scaling shifts the velocity left by (shift + 1) and subtracts the
// high byte. `opOut` receives that shift count: the original computes it in
// CL, which WRITEPATCH is also using for the operator offset (see there).

uint8_t ArtechFmDriver::carrierLevel(Channel& c, uint8_t& opOut) {  // 1879
    uint8_t level = u8((c.carLevel() & 0x3F) + c.accentAttn() + c.channelAttn() + c.sysAttn());
    if (c.velocityCarShift() != 0) {
        opOut = u8(c.velocityCarShift() + 1);
        level = u8(level - (shl16(c.velocity(), opOut) >> 8));
    }
    return u8(clampLevel(level) | (c.carLevel() & 0xC0));
}

uint8_t ArtechFmDriver::modulatorLevel(Channel& c, uint8_t& opOut) {  // 18B1
    uint8_t level = c.modLevel() & 0x3F;
    if (c.additive() != 0) level = u8(level + c.accentAttn() + c.channelAttn() + c.sysAttn());
    if (c.velocityModShift() != 0) {
        opOut = u8(c.velocityModShift() + 1);
        level = u8(level - (shl16(c.velocity(), opOut) >> 8));
    }
    return u8(clampLevel(level) | (c.modLevel() & 0xC0));
}

void ArtechFmDriver::updateLevels(Channel& c) {  // 184A
    uint8_t unused;
    writeReg(u8(opOffset(currentChannel()) + 0x43), carrierLevel(c, unused));
    if (c.additive() != 0) writeReg(u8(opOffset(currentChannel()) + 0x40), modulatorLevel(c, unused));
}

// 11-byte patch: 20 23 C0 E0 E3 40 43 60 63 80 83.
void ArtechFmDriver::writePatch(Channel& c, uint16_t patch, uint8_t op) {  // 1922
    auto next = [&]() { return byte(patch++); };
    writeReg(u8(0x20 + op), next());
    writeReg(u8(0x23 + op), next());
    const uint8_t connection = next();
    writeReg(u8(0xC0 + currentChannel()), connection);
    c.additive() = connection & 1;
    writeReg(u8(0xE0 + op), next());
    writeReg(u8(0xE3 + op), next());
    // Quirk: with velocity scaling on, the level routines overwrite the
    // operator offset (CL) with their shift count, and every register after
    // that point goes to the wrong operator.
    const uint8_t modReg = u8(0x40 + op);
    c.modLevel() = next();
    writeReg(modReg, modulatorLevel(c, op));
    const uint8_t carReg = u8(0x43 + op);
    c.carLevel() = next();
    writeReg(carReg, carrierLevel(c, op));
    writeReg(u8(0x60 + op), next());
    writeReg(u8(0x63 + op), next());
    writeReg(u8(0x80 + op), next());
    writeReg(u8(0x83 + op), next());
}

// --- per-tick effects --------------------------------------------------------------------

bool ArtechFmDriver::runEffect(Channel& c, uint16_t routine, uint16_t returnAddress) {
    if (routine == layout_.vibrato) doVibrato(c);
    else if (routine == layout_.tableMod) tableMod(c);
    else if (routine == layout_.pitchDelta) return doPitchDelta(c, returnAddress);
    // else NOVECTOR (a plain RET)
    return false;
}

void ArtechFmDriver::doVibrato(Channel& c) {  // 19D8
    if (c.vibratoWait() != 0) {
        --c.vibratoWait();
        return;
    }
    if (!accumulate(c.vibratoAcc(), c.vibratoRate())) return;
    uint16_t delta = c.vibratoDelta();
    if (--c.vibratoCount() == 0) {  // swing the other way
        delta = static_cast<uint16_t>(-delta);
        c.setVibratoDelta(delta);
        c.vibratoCount() = c.vibratoPeriod();
    }
    const uint16_t freq = static_cast<uint16_t>(c.fnum() + delta);
    c.fnumLow() = u8(freq);
    c.blockKey() = u8((c.blockKey() & 0xFC) | (freq >> 8));
    writeReg(u8(0xA0 + currentChannel()), c.fnumLow());
    writeReg(u8(0xB0 + currentChannel()), c.blockKey());
}

void ArtechFmDriver::tableMod(Channel& c) {  // 1A31: step a register through a table
    if (!accumulate(c.lfoAcc(), c.lfoRate())) return;
    if (static_cast<int8_t>(--c.lfoIndex()) < 0) c.lfoIndex() = c.lfoLength();
    writeReg(u8(c.lfoRegister() + byte(YAMOFF)), byte(static_cast<uint16_t>(c.lfoTable() + c.lfoIndex())));
}

// Pitch slide across octaves.
//
// Bug in the original, reproduced: when the octave shift leaves an F-number
// of 0, the routine pops one word too many off the stack. That word is its
// own return address, which then becomes the new F-number, and its RET goes
// straight back to the tick routine's caller: the rest of the tick
// (remaining effect, lower channels) is skipped. Returns true in that case.
bool ArtechFmDriver::doPitchDelta(Channel& c, uint16_t returnAddress) {  // 1A5A
    if (!accumulate(c.slideAcc(), c.slideSpeed())) return false;
    uint16_t freq = static_cast<uint16_t>(c.fnumLow() | ((c.blockKey() & 3) << 8));
    uint8_t block = c.blockKey() & 0x1C;
    const uint8_t keyBit = c.blockKey() & 0x20;
    const uint16_t delta = c.slideDelta();
    bool abortTick = false;
    freq = static_cast<uint16_t>(freq + delta);
    if (static_cast<int16_t>(delta) >= 0) {
        if (static_cast<int16_t>(freq) >= 0x2DE) {
            freq = static_cast<uint16_t>(freq >> 1);
            if ((freq & 0x3FF) == 0) {
                freq = returnAddress;
                abortTick = true;
            }
            block = u8((block + 4) & 0x1C);
        }
    } else if (static_cast<int16_t>(freq) <= 0x184) {
        freq = static_cast<uint16_t>(freq << 1);
        if ((freq & 0x3FF) == 0) {
            freq = returnAddress;
            abortTick = true;
        }
        block = u8((block - 4) & 0x1C);
    }
    freq &= 0x3FF;
    c.fnumLow() = u8(freq);
    writeReg(u8(0xA0 + currentChannel()), c.fnumLow());
    c.blockKey() = u8((freq >> 8) | block | keyBit);
    writeReg(u8(0xB0 + currentChannel()), c.blockKey());
    return abortTick;
}

// --- opcodes -------------------------------------------------------------------------------

void ArtechFmDriver::pushEvent(uint8_t value) {
    const uint8_t write = byte(GQ_W);
    byte(static_cast<uint16_t>(GQ + write)) = value;
    byte(GQ_W) = write + 1 == 0x20 ? 0 : u8(write + 1);
}

// Drum levels for DRUMATTN (mode 0), DRUMFADE (1) and DRUMMASTERATTN (2).
// Each drum has shadow (patch), master and attenuation bytes.
void ArtechFmDriver::drumLevels(uint8_t mask, uint8_t value, int mode) {
    struct Drum { uint8_t bit, reg; uint16_t shadow, master, attn; };
    static const Drum kDrums[5] = {
        {0x01, 0x51, 0x1F4, 0x1F9, 0x1FE},  // hi-hat
        {0x02, 0x55, 0x1F6, 0x1FB, 0x200},  // cymbal
        {0x04, 0x52, 0x1F5, 0x1FA, 0x1FF},  // tom
        {0x08, 0x54, 0x1F3, 0x1F8, 0x1FD},  // snare
        {0x10, 0x53, 0x1F2, 0x1F7, 0x1FC},  // kick
    };
    for (Drum d : kDrums) {
        if ((mask & d.bit) == 0) continue;
        d.shadow = static_cast<uint16_t>(d.shadow + base_);
        d.master = static_cast<uint16_t>(d.master + base_);
        d.attn = static_cast<uint16_t>(d.attn + base_);
        uint8_t level;
        if (mode == 0) {
            byte(d.attn) = value;
            level = u8(value + byte(d.shadow) + byte(d.master) + byte(d.attn));
        } else if (mode == 1) {
            level = u8(value + byte(d.shadow) + byte(d.master) + byte(d.attn));
        } else {
            byte(d.master) = value;
            level = u8(value + byte(d.shadow) + byte(d.attn));
        }
        level = std::min<uint8_t>(level, 0x3F);
        if (mode == 1) byte(d.master) = level;
        writeReg(d.reg, level);
    }
}

// Commands are a word: opcode | 0x80, then a parameter byte. "One-byte"
// commands have no parameter and step back over it.
ArtechFmDriver::Flow ArtechFmDriver::execute(Channel& c, uint8_t op, uint8_t param) {
    const uint8_t ch = currentChannel();
    auto oneByte = [this]() { --pc_; };
    auto waitFor = [&](uint8_t duration) {  // after a duration-setting command
        setDuration(c, duration);
        c.setPtr(pc_);
    };

    switch (op) {
    case 0x00: c.loopCount() = param; return Flow::Parse;  // SETLOOP
    case 0x01:                                           // TESTLOOP: loop back while count > 0
        if (--c.loopCount() != 0) pc_ = static_cast<uint16_t>(param | (nextByte() << 8));
        else ++pc_;
        return Flow::Parse;
    case 0x02: {  // STARTVOICE: start another sound
        if (param != 0xFF) {
            const uint16_t savedPc = pc_;
            startSound(param, false);
            pc_ = savedPc;
        }
        return Flow::Parse;
    }
    case 0x03: c.earlyOff() = param; return Flow::Parse;  // SETGATETHRESHOLD
    case 0x04: pc_ = static_cast<uint16_t>(param | (nextByte() << 8)); return Flow::Parse;  // BRANCH
    case 0x05: {  // CALLSTRING: u16 target follows the opcode byte
        oneByte();
        setWord(c.returnSlot(c.callDepth()), static_cast<uint16_t>(pc_ + 2));
        ++c.callDepth();
        pc_ = nextWord();
        return Flow::Parse;
    }
    case 0x06:  // STRINGRETURN
        oneByte();
        --c.callDepth();
        pc_ = word(c.returnSlot(c.callDepth()));
        return Flow::Parse;
    case 0x07: c.octave() = param; return Flow::Parse;  // OCTAVEOFFSET
    case 0x08: return endOfStream(c);                     // EOS
    case 0x09:  // REST
        waitFor(param);
        if (ch != 9) keyOff(c);
        return byte(LASTDUR) == 0 ? Flow::Parse : Flow::Effects;
    case 0x0A: writeReg(param, nextByte()); return Flow::Parse;  // SETOPLREG
    case 0x0B:  // NEWNOTE: change pitch without re-keying
        setFrequency(c, param, false);
        waitFor(nextByte());
        return byte(LASTDUR) == 0 ? Flow::Parse : Flow::Effects;
    case 0x0C: c.semitone() = param; return Flow::Parse;  // NOTEOFFSET
    case 0x0D:  // TABLEMODOP: rate, length, register, u16 table
        c.lfoAcc() = param;
        c.lfoRate() = param;
        c.lfoLength() = nextByte();
        c.lfoIndex() = c.lfoLength();
        c.lfoRegister() = nextByte();
        c.setLfoTable(nextWord());
        c.setRegisterEffect(layout_.tableMod);
        return Flow::Parse;
    case 0x0E: {  // STOPPARSE: stop another channel
        Channel target = channel(param);
        target.ticks() = 0;
        target.priority() = 0;
        target.setPtr(0);
        return Flow::Parse;
    }
    case 0x10:  // STUFFPATCH
        writePatch(c, word(static_cast<uint16_t>(layout_.patchTable + param * 2)), byte(YAMOFF));
        return Flow::Parse;
    case 0x11: {  // PITCHDELTA: speed, s16 big-endian delta
        c.slideSpeed() = param;
        const uint8_t high = nextByte();
        c.setSlideDelta(static_cast<uint16_t>(high << 8 | nextByte()));
        c.setPitchEffect(layout_.pitchDelta);
        c.slideAcc() = 0xFF;
        return Flow::Parse;
    }
    case 0x12:  // CLEARPITCHDELTA
        oneByte();
        c.setPitchEffect(layout_.noVector);
        c.setSlideDelta(0);
        return Flow::Parse;
    case 0x13: c.detune() = param; return Flow::Parse;  // FRACPITCH
    case 0x15: {  // VIBRATO: rate, depth, half period, delay
        c.vibratoRate() = param;
        c.vibratoDepth() = nextByte();
        const uint8_t half = nextByte();
        c.vibratoCount() = u8(half + 1);
        c.vibratoPeriod() = u8(half << 1);
        c.vibratoDelay() = nextByte();
        c.setPitchEffect(layout_.vibrato);
        return Flow::Parse;
    }
    case 0x1A: c.priority() = param; return Flow::Parse;  // SETPRIORITY
    case 0x1C:  // INITSYNC
        byte(NOTESYNC) = u8(param >> 1);
        byte(NOTECNTR) = u8(param >> 1);
        byte(PRESYNC) = 0xFF;
        byte(SYNCBYTE) = 0;
        byte(SYNCHI) = 0;
        return Flow::Parse;
    case 0x1D: {  // WAITSYNC: hold until the sync counter's masked bits change
        const bool set = (param & byte(SYNCBYTE)) != 0;
        if (byte(SYNCHI) == 0) {
            if (!set) ++byte(SYNCHI);  // armed: now wait for the bits to come on
        } else if (set) {
            byte(SYNCHI) = 0;
            return Flow::Parse;
        }
        pc_ = static_cast<uint16_t>(pc_ - 2);
        c.ticks() = 1;
        c.setPtr(pc_);
        return Flow::NextChannel;
    }
    case 0x1E: c.accentAttn() = param; updateLevels(c); return Flow::Parse;  // SETACCENTATTN
    case 0x20:  // STAY: wait without key-off
        waitFor(param);
        return byte(LASTDUR) == 0 ? Flow::Parse : Flow::Effects;
    case 0x21:  // RETRIGGER: re-key the current pitch
        setDuration(c, param);
        keyOn(c);
        c.setPtr(pc_);
        return byte(LASTDUR) == 0 ? Flow::Parse : Flow::Effects;
    case 0x24: c.gatePercent() = param & 7; return Flow::Parse;  // NOTEPERCENT
    case 0x26: byte(GLOBALTEMPO) = param; return Flow::Parse;    // SETMUSICTEMPO
    case 0x27: oneByte(); c.setRegisterEffect(layout_.noVector); return Flow::Parse;  // KILLVECTOR2
    case 0x29: c.tempo() = param; return Flow::Parse;        // SETTEMPO
    case 0x2B: c.channelAttn() = param; return Flow::Parse;  // SETCHANNELATTN
    case 0x2C:    // SETSYSATTN: channel, value
    case 0x2D: {  // SETSYSATTNDELTA: channel, delta
        const uint8_t value = nextByte();
        byte(CURCHANNEL) = param;  // updateLevels works on "the current channel"
        Channel target = channel(param);
        target.sysAttn() = op == 0x2C ? value : u8(target.sysAttn() + value);
        updateLevels(target);
        byte(CURCHANNEL) = ch;
        return Flow::Parse;
    }
    case 0x2E:    // SETAMDEPTH
    case 0x2F: {  // SETVIBDEPTH
        const uint8_t bit = op == 0x2E ? 0x80 : 0x40;
        byte(SHADOWBD) = u8((byte(SHADOWBD) & ~bit) | ((param & 1) ? bit : 0));
        writeReg(0xBD, byte(SHADOWBD));
        return Flow::Parse;
    }
    case 0x30:  // SETSIGNEDATTN
        c.accentAttn() = u8(c.accentAttn() + param);
        updateLevels(c);
        return Flow::Parse;
    case 0x33: {  // KILLCHANNEL: stop and silence a channel
        if (param == 0) byte(MOTORFLAG) = 0;
        Channel target = channel(param);
        target.ticks() = 0;
        target.priority() = 0;
        target.setPtr(0);
        target.sysAttn() = 0;
        if (param != 9) {
            const uint8_t targetOp = byte(static_cast<uint16_t>(OPOFFSETS + param));
            writeReg(u8(0xC0 + param), 0x00);
            writeReg(u8(0x43 + targetOp), 0x3F);
            writeReg(u8(0x83 + targetOp), 0xFF);
            writeReg(u8(0xB0 + param), 0x00);
        }
        return Flow::Parse;
    }
    case 0x35: {  // RANDOMPITCH: u16 mask (param = high byte); shadows not updated
        const uint16_t mask = static_cast<uint16_t>(param << 8 | nextByte());
        const uint16_t offset = random() & mask;
        const uint16_t freq = static_cast<uint16_t>(((c.blockKey() & 0x1F) << 8 | c.fnumLow()) + offset);
        writeReg(u8(0xA0 + ch), u8(freq));
        writeReg(u8(0xB0 + ch), u8((freq >> 8) | (c.blockKey() & 0x20)));
        return Flow::Parse;
    }
    case 0x36: oneByte(); c.setPitchEffect(layout_.noVector); return Flow::Parse;  // KILLVECTOR1
    case 0x39:  // SETPITCHBEND: re-derive the current note with a new bend
        c.bend() = param;
        setFrequency(c, c.lastNote(), true);
        return Flow::Parse;
    case 0x3A: oneByte(); c.tempo() = byte(GLOBALTEMPO); return Flow::Parse;  // GETMUSICTEMPO
    case 0x3B: oneByte(); return Flow::Parse;                                 // SNOP
    case 0x3C: c.randomDuration() = param; return Flow::Parse;                // RANDOMDURATION
    case 0x3D: {  // TEMPODELTA: signed, clamped to 1..255
        const uint8_t tempo = c.tempo();
        const unsigned sum = static_cast<unsigned>(param) + tempo;
        if (static_cast<int8_t>(param) >= 0) {
            c.tempo() = sum > 0xFF ? 0xFF : u8(sum);
        } else {
            c.tempo() = u8(sum) >= tempo ? 1 : u8(sum);
        }
        return Flow::Parse;
    }
    case 0x3F: {  // MOTORON: mode, table index
        byte(MOTORFLAG) = param;
        const uint8_t index = nextByte();
        setWord(DURTABLEPTR, word(static_cast<uint16_t>(layout_.motorTables + index * 2)));
        setWord(PITCHTABLEPTR, word(static_cast<uint16_t>(layout_.motorTables + u8(index + 1) * 2)));
        if (param == 2) {
            setWord(NOISEINDEX, 0);
            byte(MOTORDUR) = byte(word(DURTABLEPTR));
            writeReg(0xA0, byte(word(PITCHTABLEPTR)));
        }
        return Flow::Parse;
    }
    case 0x40: oneByte(); byte(MOTORFLAG) = 0; return Flow::Parse;  // MOTOROFF
    case 0x41: {  // DRUMSETUP: patches for channels 6-8, then B6/A6 B7/A7 B8/A8
        const uint8_t savedOp = byte(YAMOFF);
        const uint8_t patches[3] = {param, nextByte(), nextByte()};
        static const uint16_t kShadowMod[3] = {0x1F2, 0x1F4, 0x1F5};  // kick, hi-hat, tom
        static const uint16_t kShadowCar[3] = {0, 0x1F3, 0x1F6};      // -, snare, cymbal
        for (int k = 0; k < 3; ++k) {
            const uint8_t voice = u8(6 + k);
            byte(CURCHANNEL) = voice;
            byte(YAMOFF) = opOffset(voice);
            const uint16_t patch = word(static_cast<uint16_t>(layout_.patchTable + patches[k] * 2));
            if (k == 0) {
                byte(static_cast<uint16_t>(kShadowMod[0] + base_)) = byte(static_cast<uint16_t>(patch + 6));
            } else {
                byte(static_cast<uint16_t>(kShadowMod[k] + base_)) = byte(static_cast<uint16_t>(patch + 5));
                byte(static_cast<uint16_t>(kShadowCar[k] + base_)) = byte(static_cast<uint16_t>(patch + 6));
            }
            // Note: level bookkeeping uses the calling channel, as in the original.
            writePatch(c, patch, byte(YAMOFF));
        }
        static const uint16_t kB0Shadow[3] = {0x3F3, 0x436, 0x479};  // channels 6-8, +28
        for (int k = 0; k < 3; ++k) {
            const uint8_t b0 = nextByte() & 0x2F;
            byte(static_cast<uint16_t>(kB0Shadow[k] + base_)) = b0;
            writeReg(u8(0xB6 + k), b0);
            writeReg(u8(0xA6 + k), nextByte());
        }
        byte(DRUMMASK) = 0x20;  // rhythm mode on
        byte(YAMOFF) = savedOp;
        byte(CURCHANNEL) = ch;
        return Flow::Parse;
    }
    case 0x42: {  // DODRUM: release the drums in `param`, then key them
        // Two writes to BD after a single register latch in the original.
        byte(DRUMBITS) = param & 0x1F;
        writeReg(0xBD, u8(((byte(DRUMBITS) ^ 0xFF) & byte(DRUMMASK)) | 0x20));
        byte(DRUMMASK) = u8(param | byte(DRUMMASK));
        writeReg(0xBD, u8(byte(DRUMMASK) | byte(SHADOWBD) | 0x20));
        return Flow::Parse;
    }
    case 0x43:  // DRUMOFF
        oneByte();
        byte(DRUMMASK) = 0;
        writeReg(0xBD, byte(SHADOWBD) & 0xC0);
        return Flow::Parse;
    case 0x44:  // DRUMATTN
    case 0x45:  // DRUMFADE
    case 0x46:  // DRUMMASTERATTN
        drumLevels(param, nextByte(), op - 0x44);
        return Flow::Parse;
    case 0x48: c.autoTempo() = param; return Flow::Parse;  // SETAUTOTEMPO
    case 0x49:  // SETVELOCITY: modulator shift, carrier shift
        c.velocityModShift() = param;
        c.velocityCarShift() = nextByte();
        return Flow::Parse;
    case 0x50:  // game event
        if (layout_.eventFlag) ++byte(layout_.eventFlag);
        if (param != 0) pushEvent(param);
        return Flow::Parse;
    case 0x51:  // channel-marker event
        if (layout_.eventFlag) ++byte(layout_.eventFlag);
        oneByte();
        pushEvent(u8((ch & 0x0F) | 0xA0));
        return Flow::Parse;
    case 0x52:  // drum-marker event
        if (layout_.eventFlag) ++byte(layout_.eventFlag);
        oneByte();
        pushEvent(u8((byte(DRUMBITS) & 0x1F) | 0x80));
        return Flow::Parse;
    default:
        return endOfStream(c);  // unassigned opcodes all point at EOS
    }
}

}  // namespace edison
