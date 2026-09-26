#include "audio/artech_fm_driver.h"

#include <algorithm>
#include <cstring>

#include "formats/ne_file.h"

namespace edison {

namespace {

// Find `pattern` in `code`, where 0x100 entries are wildcards. Returns the
// offset of the first match or -1.
long findPattern(const std::vector<uint8_t>& code, std::initializer_list<int> pattern) {
    const std::vector<int> p(pattern);
    if (code.size() < p.size()) return -1;
    for (size_t i = 0; i + p.size() <= code.size(); ++i) {
        size_t k = 0;
        while (k < p.size() && (p[k] == 0x100 || code[i + k] == p[k])) ++k;
        if (k == p.size()) return static_cast<long>(i);
    }
    return -1;
}

uint16_t le16(const std::vector<uint8_t>& d, long at) {
    return static_cast<uint16_t>(d[at] | (d[at + 1] << 8));
}

constexpr int X = 0x100;  // wildcard byte

// Shifts as the 386+ CPU the original ran on performs them: count & 31,
// and a 16-bit operand shifted by 16 or more becomes 0.
uint16_t shl16(uint16_t v, uint8_t count) {
    const unsigned n = count & 31u;
    return n >= 16 ? 0 : static_cast<uint16_t>(v << n);
}

uint16_t shr16(uint16_t v, uint8_t count) {
    const unsigned n = count & 31u;
    return n >= 16 ? 0 : static_cast<uint16_t>(v >> n);
}

// Clamp used by the operator level routines (1879 / 18B1).
uint8_t clampLevel(uint8_t v) {
    if (v <= 0x3F) return v;
    return static_cast<int8_t>(v) < 0 ? 0 : 0x3F;
}

}  // namespace

// --- loading -----------------------------------------------------------------

bool ArtechFmDriver::loadDll(const std::string& path, std::string* error) {
    NeFile ne;
    if (!ne.load(path, error)) return false;
    const std::vector<uint8_t> code = ne.segment(1);
    const std::vector<uint8_t> data = ne.segment(2);
    if (code.empty() || data.empty()) {
        if (error) *error = path + ": missing code or data segment";
        return false;
    }

    // SWITCHSOUNDTABLE: shl bx,1 / mov bx,[bx+TABLES] / mov [SOUNDTABLEPTR],bx
    const long tables = findPattern(code, {0xD1, 0xE3, 0x8B, 0x9F, X, X, 0x89, 0x1E, 0x0A, 0x02});
    // STUFFPATCH: mov cl,[YAMOFF] / sub bx,bx / mov bl,ah / shl bx,1 / mov si,[bx+PATCH]
    const long patch = findPattern(code, {0x8A, 0x0E, 0xF1, 0x01, 0x2B, 0xDB, 0x8A, 0xDC, 0xD1, 0xE3, 0x8B, 0xB7, X, X});
    // MOTORON: mov bx,[bx+MOTORTABLES] / mov [DURTABLEPTR],bx
    const long motor = findPattern(code, {0x8B, 0x9F, X, X, 0x89, 0x1E, 0x06, 0x02});
    // op 50: inc byte [GE_FLG] / xor bx,bx / mov bl,[GQ_W]  (absent in some builds)
    const long geflag = findPattern(code, {0xFE, 0x06, X, X, 0x33, 0xDB, 0x8A, 0x1E});
    if (tables < 0 || patch < 0 || motor < 0) {
        if (error) *error = path + ": not an Artech ADLIB-family driver";
        return false;
    }
    layout_.soundTables = le16(code, tables + 4);
    layout_.patchTable = le16(code, patch + 12);
    layout_.motorTables = le16(code, motor + 2);
    layout_.eventFlag = geflag >= 0 ? le16(code, geflag + 2) : 0;

    pristine_.fill(0);
    std::copy(data.begin(), data.end(), pristine_.begin());
    mem_ = pristine_;
    unknownOpcodes_ = 0;
    return true;
}

// --- exported API ------------------------------------------------------------

void ArtechFmDriver::outAdlib() {
    if (opl_) opl_(ah, al);
    setDx(0x389);
}

void ArtechFmDriver::init() {  // 1B94
    setBx(0);
    setBx(m16(bx() + layout_.soundTables));
    w16(SOUNDTABLEPTR, bx());
    resetSound();
    w16(BUFFTAIL, 0);
    w16(BUFFHEAD, 0);
}

void ArtechFmDriver::remove() { resetSound(); }  // 1BC7

void ArtechFmDriver::sendSound(uint16_t id) {  // 0A13: word written at a byte index
    setBx(m16(BUFFHEAD));
    w16(static_cast<uint16_t>(BUFF + bx()), id);
    setBx(static_cast<uint16_t>((bx() + 1) & 0xF));
    w16(BUFFHEAD, bx());
}

void ArtechFmDriver::switchSoundTable(uint16_t table) {  // 0AED
    w16(SOUNDTABLEPTR, m16(static_cast<uint16_t>(layout_.soundTables + table * 2)));
}

void ArtechFmDriver::setMotor(uint16_t value) {  // 0AA4
    w16(NOISEINDEX, value);
    setBx(value & 0x7FFF);
    si = m16(DURTABLEPTR);
    m8(MOTORDUR) = m8(static_cast<uint16_t>(bx() + si));
    si = m16(PITCHTABLEPTR);
    al = m8(static_cast<uint16_t>(bx() + si));
    ah = 0xA0;
    outAdlib();
}

uint8_t ArtechFmDriver::nextEvent() {  // 0B0B
    ah = 0;
    setBx(0);
    bl = m8(GQ_R);
    al = m8(static_cast<uint16_t>(GQ + bl));
    if (al != 0) {
        m8(static_cast<uint16_t>(GQ + bl)) = 0;
        bl = static_cast<uint8_t>(bl + 1);
        if (bl == 0x20) bl = 0;
        m8(GQ_R) = bl;
    }
    return al;
}

void ArtechFmDriver::flushEvents() {  // 0B3B
    // The original's clear loop never advances DI (it rewrites GQ_W 32
    // times), so a flush only resets the read/write indexes.
    m8(GQ_R) = 0;
    m8(GQ_W) = 0;
}

void ArtechFmDriver::update() {  // 1BD2  UPDATE_ADLIB
    if (m8(MOTORFLAG) != 0) {
        if (--m8(MOTORDCNT) == 0) {
            const uint16_t reg28 = SPRAM1 + 0x28;
            uint8_t v = m8(reg28) & 0x1F;
            m8(STATE) ^= 0xFF;
            v |= m8(STATE) & 0x20;
            m8(reg28) = v;
            ah = 0xB0;
            al = v;
            outAdlib();
            m8(MOTORDCNT) = m8(MOTORDUR);
        }
    }
    if (--m8(SOUNDSYSTICK) == 0) {
        m8(SOUNDSYSTICK) = 1;
        startQueued();  // 09FF
        tickChannels();
        syncTick();
    }
}

bool ArtechFmDriver::idle() const {
    for (int ch = 0; ch < 10; ++ch) {
        const uint16_t chan = m16(static_cast<uint16_t>(SCHNLPTR + 2 * ch));
        if (mem_[static_cast<uint16_t>(chan + 5)] != 0 && m16(static_cast<uint16_t>(chan + 3)) != 0)
            return false;
    }
    return true;
}

ArtechFmDriver::SoundList ArtechFmDriver::listSounds() const {
    // The table has no stored length: it ends at the first entry pointing
    // below the driver variables, or where the earliest pointed-at data begins.
    SoundList list;
    const uint16_t table = m16(SOUNDTABLEPTR);
    uint32_t bound = 0x8000;
    for (uint32_t i = 0; i < bound; ++i) {
        const uint16_t p = m16(static_cast<uint16_t>(table + 2 * i));
        if (p <= layout_.soundTables) break;
        if (p > table) bound = std::min<uint32_t>(bound, (p - table) / 2u);
        (mem_[p] <= 9 ? list.sounds : list.snippets).push_back(static_cast<uint16_t>(i));
    }
    return list;
}

// --- core routines -----------------------------------------------------------

void ArtechFmDriver::resetSound() {  // 0B9D
    m8(DO_SOUND) = 0;
    w16(SEED, 0x1234);
    setAx(0x0120); outAdlib();
    setAx(0x0800); outAdlib();
    setAx(0xBD00); outAdlib();
    m8(MOTORFLAG) = 0;
    setCx(10);
    bh = 0;
    do {
        bl = static_cast<uint8_t>(cl - 1);
        if (bl != 9) {
            ah = static_cast<uint8_t>(m8(static_cast<uint16_t>(bx() + OPOFFSETS)) + 0x40);
            al = 0x3F;
            outAdlib();
            ah = static_cast<uint8_t>(m8(static_cast<uint16_t>(bx() + OPOFFSETS)) + 0x43);
            al = 0x3F;
            outAdlib();
        }
        bl = static_cast<uint8_t>(bl << 1);
        di = m16(static_cast<uint16_t>(bx() + SCHNLPTR));
        clearSpram();
        setCx(static_cast<uint16_t>(cx() - 1));
    } while (cx() != 0);
}

void ArtechFmDriver::clearSpram() {  // 18EF
    for (uint16_t i = 0; i < 0x40; ++i) m8(static_cast<uint16_t>(di + 3 + i)) = 0;
    m8(static_cast<uint16_t>(di + 0x25)) = 0xFF;
    m8(static_cast<uint16_t>(di + 8)) = 0;
    w16(static_cast<uint16_t>(di + 0x29), NOVECTOR);
    w16(static_cast<uint16_t>(di + 0x2B), NOVECTOR);
    m8(static_cast<uint16_t>(di + 0x35)) = 1;
}

void ArtechFmDriver::startQueued() {  // 0A37
    const uint16_t saveSi = si, saveDi = di;
    for (;;) {
        setBx(m16(BUFFTAIL));
        if (bx() == m16(BUFFHEAD)) break;
        setBx(m16(static_cast<uint16_t>(bx() + BUFF)));
        bh = 0;
        setBx(static_cast<uint16_t>(bx() << 1));
        setBx(static_cast<uint16_t>(bx() + m16(SOUNDTABLEPTR)));
        si = m16(bx());
        setBx(m16(si));
        if (bl == 0) m8(MOTORFLAG) = 0;
        al = bh;
        cl = bl;
        bh = 0;
        setBx(static_cast<uint16_t>(bx() << 1));
        di = m16(static_cast<uint16_t>(bx() + SCHNLPTR));
        if (static_cast<int8_t>(al) >= static_cast<int8_t>(m8(static_cast<uint16_t>(di + 8)))) {
            clearSpram();
            m8(static_cast<uint16_t>(di + 8)) = al;
            si = static_cast<uint16_t>(si + 2);
            w16(static_cast<uint16_t>(di + 3), si);
            m8(static_cast<uint16_t>(di + 0x25)) = 0xFF;
            m8(static_cast<uint16_t>(di + 0x26)) = 0xFF;
            m8(static_cast<uint16_t>(di + 5)) = 1;
            m8(DO_SOUND) = 1;
            if (bl != 0x12) silenceVoice();  // bl = channel * 2: channel 9 skipped
        }
        w16(BUFFTAIL, static_cast<uint16_t>((m16(BUFFTAIL) + 1) & 0xF));
    }
    si = saveSi;
    di = saveDi;
}

void ArtechFmDriver::syncTick() {  // 17E0
    const unsigned sum = m8(PRESYNC) + m8(GLOBALTEMPO);
    m8(PRESYNC) = static_cast<uint8_t>(sum);
    if (sum < 0x100) return;
    if (--m8(NOTECNTR) != 0) return;
    m8(NOTECNTR) = m8(NOTESYNC);
    ++m8(SYNCBYTE);
}

void ArtechFmDriver::tickChannels() {  // 0BF1
    if (m8(DO_SOUND) == 0) return;
    m8(CURCHANNEL) = 9;
    for (;;) {
        bl = m8(CURCHANNEL);
        bh = 0;
        bl = static_cast<uint8_t>(bl << 1);
        di = m16(static_cast<uint16_t>(bx() + SCHNLPTR));
        al = m8(static_cast<uint16_t>(di + 5));
        const Flow flow = al != 0 ? channelStep() : Flow::NextChannel;
        if (flow == Flow::Effects) {
            callVector(m16(static_cast<uint16_t>(di + 0x29)));
            callVector(m16(static_cast<uint16_t>(di + 0x2B)));
        }
        if (static_cast<int8_t>(--m8(CURCHANNEL)) < 0) return;
    }
}

ArtechFmDriver::Flow ArtechFmDriver::channelStep() {  // 0C14
    setBx(OPOFFSETS);
    bl = static_cast<uint8_t>(bl + m8(CURCHANNEL));
    al = m8(bx());
    m8(YAMOFF) = al;
    if (m8(static_cast<uint16_t>(di + 0x40)) != 0) {
        al = m8(GLOBALTEMPO);
        m8(static_cast<uint16_t>(di + 0x25)) = al;
    }
    const unsigned sum = m8(static_cast<uint16_t>(di + 0x26)) + m8(static_cast<uint16_t>(di + 0x25));
    al = static_cast<uint8_t>(sum);
    m8(static_cast<uint16_t>(di + 0x26)) = al;
    if (sum < 0x100) return Flow::Effects;

    if (--m8(static_cast<uint16_t>(di + 5)) == 0) return parse();
    al = m8(static_cast<uint16_t>(di + 5));
    if (al == m8(static_cast<uint16_t>(di + 0x21))) keyOff();  // may clobber AL
    if (al == m8(static_cast<uint16_t>(di + 0x35))) {           // CHECK_GATE
        if (m8(CURCHANNEL) != 9) keyOff();
    }
    return Flow::Effects;
}

ArtechFmDriver::Flow ArtechFmDriver::parse() {  // 0C5B
    si = m16(static_cast<uint16_t>(di + 3));
    for (;;) {
        setAx(lodsw());  // PARSE_OP
        if ((al & 0x80) == 0) {
            // NOTE
            setFrequency();
            keyOn();
            setDuration();
            if (m16(static_cast<uint16_t>(di + 0x32)) != 0 || m16(static_cast<uint16_t>(di + 0x33)) != 0) {
                // VELOCITYON
                al = lodsb();
                m8(static_cast<uint16_t>(di + 0x34)) = al;
                carrierLevel();
                setDx(0x388);
                ah = static_cast<uint8_t>(m8(YAMOFF) + 0x43);
                outAdlib();
                modulatorLevel();
                setDx(0x388);
                ah = static_cast<uint8_t>(m8(YAMOFF) + 0x40);
                outAdlib();
            }
            if (m8(LASTDUR) == 0) continue;
            w16(static_cast<uint16_t>(di + 3), si);
            return Flow::Effects;
        }
        al &= 0x7F;  // OP_CODE
        const uint8_t op = al;
        bl = static_cast<uint8_t>(al << 1);
        bh = 0;
        Flow flow;
        if (op > 0x52) {
            // 0x53..0x70 index past the jump table into code; > 0x70 is EOS.
            if (op <= 0x70) ++unknownOpcodes_;
            flow = dispatch(0x08);
        } else {
            flow = dispatch(op);
        }
        if (flow != Flow::Parse) return flow;
    }
}

void ArtechFmDriver::callVector(uint16_t routine) {
    switch (routine) {
        case NOVECTOR: return;
        case DOVIBRATO: doVibrato(); return;
        case TBLMOD: tableMod(); return;
        case DOPITCHDELTA: doPitchDelta(); return;
        default: return;  // zeroed channel memory: never reached in practice
    }
}

// --- note helpers ------------------------------------------------------------

void ArtechFmDriver::setFrequency() {  // 14F0
    const uint16_t saved = ax();
    m8(static_cast<uint16_t>(di + 0x41)) = al;
    ch = al & 0xF0;
    bl = m8(static_cast<uint16_t>(di + 7));
    ch = static_cast<uint8_t>(ch + bl);
    al &= 0x0F;
    bl = m8(static_cast<uint16_t>(di + 0x12));
    al = static_cast<uint8_t>(al + bl);
    if (static_cast<int8_t>(al) >= 12) {
        al = static_cast<uint8_t>(al - 12);
        ch = static_cast<uint8_t>(ch + 0x10);
    } else if (static_cast<int8_t>(al) < 0) {
        al = static_cast<uint8_t>(al + 12);
        ch = static_cast<uint8_t>(ch - 0x10);
    }
    bl = static_cast<uint8_t>(al << 1);
    bh = 0;
    setAx(m16(static_cast<uint16_t>(bx() + FREQTABLE)));
    setDx(0);
    dl = m8(static_cast<uint16_t>(di + 0x24));
    setAx(static_cast<uint16_t>(ax() + dl));
    ch = static_cast<uint8_t>(((ch >> 2) & 0x1C) | ah);
    cl = al;
    const uint8_t bend = m8(static_cast<uint16_t>(di + 0x42));
    if (bend != 0) {
        const uint8_t semi = m8(static_cast<uint16_t>(di + 0x41)) & 0x0F;
        if (static_cast<int8_t>(bend) > 0) {
            setBx(static_cast<uint16_t>((semi + 2) << 1));
            const uint16_t tbl = m16(bx());
            setBx(bend);
            al = m8(static_cast<uint16_t>(bx() + tbl));
            ah = 0;
            setCx(static_cast<uint16_t>(cx() + ax()));
        } else {
            setBx(static_cast<uint16_t>(semi << 1));
            const uint16_t tbl = m16(bx());
            setBx(static_cast<uint8_t>((bend ^ 0xFF) + 1));
            al = m8(static_cast<uint16_t>(bx() + tbl));
            ah = 0;
            setCx(static_cast<uint16_t>(cx() - ax()));
        }
    }
    // NO_PITCHBEND
    al = m8(static_cast<uint16_t>(di + 0x28)) & 0x20;
    ch |= al;
    m8(static_cast<uint16_t>(di + 0x28)) = ch;
    m8(static_cast<uint16_t>(di + 0x27)) = cl;
    ah = static_cast<uint8_t>(0xA0 + m8(CURCHANNEL));
    al = cl;
    outAdlib();
    ah = static_cast<uint8_t>(0xB0 + m8(CURCHANNEL));
    al = ch;
    outAdlib();
    setAx(saved);
}

void ArtechFmDriver::keyOn() {  // 15A5
    const uint16_t saved = ax();
    ah = static_cast<uint8_t>(0xB0 + m8(CURCHANNEL));
    al = m8(static_cast<uint16_t>(di + 0x28)) | 0x20;
    m8(static_cast<uint16_t>(di + 0x28)) = al;
    outAdlib();
    cl = m8(static_cast<uint16_t>(di + 0x19));
    al = static_cast<uint8_t>(9 - cl);
    cl = al;
    setAx(m16(static_cast<uint16_t>(di + 0x27)) & 0x3FF);
    setAx(shr16(ax(), cl));
    ah = 0;
    w16(static_cast<uint16_t>(di + 0x17), ax());
    cl = m8(static_cast<uint16_t>(di + 0x1C));
    m8(static_cast<uint16_t>(di + 0x1F)) = cl;
    setAx(saved);
}

void ArtechFmDriver::keyOff() {  // 15D5
    if (m8(DRUMMASK) != 0 && static_cast<int8_t>(m8(CURCHANNEL)) >= 6) return;
    if (m8(CURCHANNEL) == 9) return;
    ah = static_cast<uint8_t>(0xB0 + m8(CURCHANNEL));
    al = m8(static_cast<uint16_t>(di + 0x28)) & 0xDF;
    m8(static_cast<uint16_t>(di + 0x28)) = al;
    outAdlib();
}

void ArtechFmDriver::setDuration() {  // 15FD
    m8(LASTDUR) = ah;
    bl = ah;
    if (m8(static_cast<uint16_t>(di + 0x36)) != 0) {
        random();
        al &= m8(static_cast<uint16_t>(di + 0x36));
        bl = static_cast<uint8_t>(bl + al);
        m8(static_cast<uint16_t>(di + 5)) = bl;
        return;
    }
    if (m8(static_cast<uint16_t>(di + 0x2D)) != 0) {
        bl = ah;
        ah = static_cast<uint8_t>(ah >> 3);
        cl = m8(static_cast<uint16_t>(di + 0x2D));
        ch = 0;
        al = 0;
        for (uint16_t n = cx(); n != 0; --n) al = static_cast<uint8_t>(al + ah);
        setCx(0);
        m8(static_cast<uint16_t>(di + 0x21)) = al;
    }
    m8(static_cast<uint16_t>(di + 5)) = bl;
}

void ArtechFmDriver::random() {  // 1ADE
    uint16_t v = static_cast<uint16_t>(m16(SEED) + 0x9248);
    v = static_cast<uint16_t>((v >> 3) | (v << 13));
    w16(SEED, v);
    setAx(v);
}

void ArtechFmDriver::carrierLevel() {  // 1879
    al = m8(static_cast<uint16_t>(di + 0x2F)) & 0x3F;
    al = static_cast<uint8_t>(al + m8(static_cast<uint16_t>(di + 0x20)));
    al = static_cast<uint8_t>(al + m8(static_cast<uint16_t>(di + 0x30)));
    al = static_cast<uint8_t>(al + m8(di));
    if (m8(static_cast<uint16_t>(di + 0x33)) != 0) {
        cl = static_cast<uint8_t>(m8(static_cast<uint16_t>(di + 0x33)) + 1);
        setBx(m8(static_cast<uint16_t>(di + 0x34)));
        setBx(shl16(bx(), cl));
        al = static_cast<uint8_t>(al - bh);
    }
    al = clampLevel(al);
    bl = m8(static_cast<uint16_t>(di + 0x2F)) & 0xC0;
    al |= bl;
}

void ArtechFmDriver::modulatorLevel() {  // 18B1
    al = m8(static_cast<uint16_t>(di + 0x2E)) & 0x3F;
    if (m8(static_cast<uint16_t>(di + 0x31)) != 0) {
        al = static_cast<uint8_t>(al + m8(static_cast<uint16_t>(di + 0x20)));
        al = static_cast<uint8_t>(al + m8(static_cast<uint16_t>(di + 0x30)));
        al = static_cast<uint8_t>(al + m8(di));
    }
    if (m8(static_cast<uint16_t>(di + 0x32)) != 0) {
        cl = static_cast<uint8_t>(m8(static_cast<uint16_t>(di + 0x32)) + 1);
        setBx(m8(static_cast<uint16_t>(di + 0x34)));
        setBx(shl16(bx(), cl));
        al = static_cast<uint8_t>(al - bh);
    }
    al = clampLevel(al);
    bl = m8(static_cast<uint16_t>(di + 0x2E)) & 0xC0;
    al |= bl;
}

void ArtechFmDriver::updateLevels() {  // 184A
    carrierLevel();
    setBx(OPOFFSETS);
    bl = static_cast<uint8_t>(bl + m8(CURCHANNEL));
    cl = m8(bx());
    ah = static_cast<uint8_t>(cl + 0x43);
    outAdlib();
    if (m8(static_cast<uint16_t>(di + 0x31)) != 0) {
        modulatorLevel();
        setBx(OPOFFSETS);
        bl = static_cast<uint8_t>(bl + m8(CURCHANNEL));
        cl = m8(bx());
        ah = static_cast<uint8_t>(cl + 0x40);
        outAdlib();
    }
}

void ArtechFmDriver::writePatch() {  // 1922: si = patch, cl = operator offset
    ah = static_cast<uint8_t>(0x20 + cl); al = lodsb(); outAdlib();
    ah = static_cast<uint8_t>(0x23 + cl); al = lodsb(); outAdlib();
    ah = static_cast<uint8_t>(0xC0 + m8(CURCHANNEL)); al = lodsb(); outAdlib();
    al &= 1;
    m8(static_cast<uint16_t>(di + 0x31)) = al;
    ah = static_cast<uint8_t>(0xE0 + cl); al = lodsb(); outAdlib();
    ah = static_cast<uint8_t>(0xE3 + cl); al = lodsb(); outAdlib();
    ah = static_cast<uint8_t>(0x40 + cl);
    al = lodsb();
    m8(static_cast<uint16_t>(di + 0x2E)) = al;
    modulatorLevel();  // may clobber CL, which the writes below then use
    outAdlib();
    ah = static_cast<uint8_t>(0x43 + cl);
    al = lodsb();
    m8(static_cast<uint16_t>(di + 0x2F)) = al;
    carrierLevel();
    outAdlib();
    ah = static_cast<uint8_t>(0x60 + cl); al = lodsb(); outAdlib();
    ah = static_cast<uint8_t>(0x63 + cl); al = lodsb(); outAdlib();
    ah = static_cast<uint8_t>(0x80 + cl); al = lodsb(); outAdlib();
    ah = static_cast<uint8_t>(0x83 + cl); al = lodsb(); outAdlib();
}

void ArtechFmDriver::silenceVoice() {  // 198E: cl = channel
    if (m8(DRUMMASK) != 0 && static_cast<int8_t>(cl) >= 6) return;
    setBx(OPOFFSETS);
    ch = cl;
    bl = static_cast<uint8_t>(bl + cl);
    cl = m8(bx());
    al = 0xFF;
    ah = static_cast<uint8_t>(0x60 + cl); outAdlib();
    ah = static_cast<uint8_t>(0x63 + cl); outAdlib();
    ah = static_cast<uint8_t>(0x80 + cl); outAdlib();
    ah = static_cast<uint8_t>(0x83 + cl); outAdlib();
    ah = static_cast<uint8_t>(0xB0 + ch);
    al = 0;
    outAdlib();
    al = 0x20;
    outAdlib();
}

// --- per-tick effects --------------------------------------------------------

void ArtechFmDriver::doVibrato() {  // 19D8
    uint8_t& delay = m8(static_cast<uint16_t>(di + 0x1F));
    if (delay != 0) {
        --delay;
        return;
    }
    const unsigned sum = m8(static_cast<uint16_t>(di + 0x1E)) + m8(static_cast<uint16_t>(di + 0x1D));
    cl = static_cast<uint8_t>(sum);
    m8(static_cast<uint16_t>(di + 0x1E)) = cl;
    if (sum < 0x100) return;
    setBx(m16(static_cast<uint16_t>(di + 0x17)));
    if (--m8(static_cast<uint16_t>(di + 0x1A)) == 0) {
        setBx(static_cast<uint16_t>((bx() ^ 0xFFFF) + 1));
        w16(static_cast<uint16_t>(di + 0x17), bx());
        al = m8(static_cast<uint16_t>(di + 0x1B));
        m8(static_cast<uint16_t>(di + 0x1A)) = al;
    }
    setAx(static_cast<uint16_t>((m16(static_cast<uint16_t>(di + 0x27)) & 0x3FF) + bx()));
    m8(static_cast<uint16_t>(di + 0x27)) = al;
    al = static_cast<uint8_t>((m8(static_cast<uint16_t>(di + 0x28)) & 0xFC) | ah);
    m8(static_cast<uint16_t>(di + 0x28)) = al;
    ah = static_cast<uint8_t>(0xA0 + m8(CURCHANNEL));
    al = m8(static_cast<uint16_t>(di + 0x27));
    outAdlib();
    ah = static_cast<uint8_t>(0xB0 + m8(CURCHANNEL));
    al = m8(static_cast<uint16_t>(di + 0x28));
    outAdlib();
}

void ArtechFmDriver::tableMod() {  // 1A31
    const unsigned sum = m8(static_cast<uint16_t>(di + 0x3A)) + m8(static_cast<uint16_t>(di + 0x39));
    al = static_cast<uint8_t>(sum);
    m8(static_cast<uint16_t>(di + 0x3A)) = al;
    if (sum < 0x100) return;
    if (static_cast<int8_t>(--m8(static_cast<uint16_t>(di + 0x3C))) < 0) {
        al = m8(static_cast<uint16_t>(di + 0x3B));
        m8(static_cast<uint16_t>(di + 0x3C)) = al;
    }
    ah = static_cast<uint8_t>(m8(static_cast<uint16_t>(di + 0x3D)) + m8(YAMOFF));
    al = m8(static_cast<uint16_t>(di + 0x3C));
    setBx(m16(static_cast<uint16_t>(di + 0x3E)));
    al = m8(static_cast<uint16_t>(bx() + al));  // xlat
    outAdlib();
}

void ArtechFmDriver::doPitchDelta() {  // 1A5A
    const unsigned sum = m8(static_cast<uint16_t>(di + 0x14)) + m8(static_cast<uint16_t>(di + 0x13));
    cl = static_cast<uint8_t>(sum);
    m8(static_cast<uint16_t>(di + 0x14)) = cl;
    if (sum < 0x100) return;
    bl = m8(static_cast<uint16_t>(di + 0x27));
    bh = m8(static_cast<uint16_t>(di + 0x28)) & 3;
    dl = m8(static_cast<uint16_t>(di + 0x28));
    dh = dl;
    dl &= 0x1C;
    dh &= 0x20;
    setCx(m16(static_cast<uint16_t>(di + 0x15)));
    if (static_cast<int16_t>(cx()) >= 0) {
        setBx(static_cast<uint16_t>(bx() + cx()));
        if (static_cast<int16_t>(bx()) >= 0x2DE) {  // octave up
            setBx(static_cast<uint16_t>(bx() >> 1));
            dl = static_cast<uint8_t>((dl + 4) & 0x1C);
        }
    } else {
        setBx(static_cast<uint16_t>(bx() + cx()));
        if (static_cast<int16_t>(bx()) <= 0x184) {  // octave down
            setBx(static_cast<uint16_t>(bx() << 1));
            dl = static_cast<uint8_t>((dl - 4) & 0x1C);
        }
    }
    setBx(bx() & 0x3FF);
    setCx(static_cast<uint16_t>(dl | (dh << 8)));
    ah = static_cast<uint8_t>(0xA0 + m8(CURCHANNEL));
    al = bl;
    outAdlib();
    m8(static_cast<uint16_t>(di + 0x27)) = al;
    ah = static_cast<uint8_t>(0xB0 + m8(CURCHANNEL));
    bh = static_cast<uint8_t>(bh | cl | ch);
    al = bh;
    outAdlib();
    m8(static_cast<uint16_t>(di + 0x28)) = al;
}

// --- opcodes -----------------------------------------------------------------

void ArtechFmDriver::pushEvent(uint8_t value) {  // tail of ops 50-52
    setBx(0);
    bl = m8(GQ_W);
    m8(static_cast<uint16_t>(GQ + bl)) = value;
    bl = static_cast<uint8_t>(bl + 1);
    if (bl == 0x20) bl = 0;
    m8(GQ_W) = bl;
}

// DRUMATTN (mode 0), DRUMFADE (mode 1), DRUMMASTERATTN (mode 2) for one drum.
void ArtechFmDriver::drumLevel(uint8_t mask, uint8_t bit, uint8_t reg, uint16_t shadow,
                               uint16_t master, uint16_t attn, int mode) {
    const uint8_t value = al;
    if ((mask & bit) == 0) return;
    unsigned level;
    if (mode == 0) {
        m8(attn) = value;
        level = static_cast<uint8_t>(value + m8(shadow) + m8(master) + m8(attn));
    } else if (mode == 1) {
        level = static_cast<uint8_t>(value + m8(shadow) + m8(master) + m8(attn));
    } else {
        m8(master) = value;
        level = static_cast<uint8_t>(value + m8(shadow) + m8(attn));
    }
    al = static_cast<uint8_t>(level > 0x3F ? 0x3F : level);  // 17CF
    if (mode == 1) m8(master) = al;
    ah = reg;
    outAdlib();
}

ArtechFmDriver::Flow ArtechFmDriver::dispatch(uint8_t op) {
    auto chan = [this](int off) -> uint8_t& { return m8(static_cast<uint16_t>(di + off)); };
    auto channelPtr = [this](uint8_t channel) {
        return m16(static_cast<uint16_t>(SCHNLPTR + channel * 2));
    };
    auto durationFlow = [this]() {
        w16(static_cast<uint16_t>(di + 3), si);
        return m8(LASTDUR) == 0 ? Flow::Parse : Flow::Effects;
    };

    switch (op) {
    case 0x00: chan(6) = ah; return Flow::Parse;  // SETLOOP
    case 0x01:  // TESTLOOP
        if (--chan(6) != 0) return dispatch(0x04);
        ++si;
        return Flow::Parse;
    case 0x02: {  // STARTVOICE
        const uint16_t saveSi = si, saveDi = di, saveBx = bx();
        bl = ah;
        if (bl != 0xFF) {
            bh = 0;
            setBx(static_cast<uint16_t>((bx() << 1) + m16(SOUNDTABLEPTR)));
            si = m16(bx());
            setBx(m16(si));
            al = bh;
            cl = bl;
            bh = 0;
            setBx(static_cast<uint16_t>(bx() << 1));
            di = m16(static_cast<uint16_t>(bx() + SCHNLPTR));
            if (static_cast<int8_t>(al) >= static_cast<int8_t>(chan(8))) {
                clearSpram();
                chan(8) = al;
                si = static_cast<uint16_t>(si + 2);
                w16(static_cast<uint16_t>(di + 3), si);
                chan(0x25) = 0xFF;
                chan(0x26) = 0xFF;
                chan(5) = 1;
                m8(DO_SOUND) = 1;
                silenceVoice();
            }
        }
        setBx(saveBx);
        di = saveDi;
        si = saveSi;
        return Flow::Parse;
    }
    case 0x03: chan(0x35) = ah; return Flow::Parse;  // SETGATETHRESHOLD
    case 0x04: {  // BRANCH
        cl = ah;
        al = lodsb();
        si = static_cast<uint16_t>(cl | (al << 8));
        setAx(si);
        return Flow::Parse;
    }
    case 0x05: {  // CALLSTRING
        --si;
        setBx(static_cast<uint16_t>(static_cast<uint8_t>(chan(9) << 1)));
        w16(static_cast<uint16_t>(bx() + di + 0x0A), static_cast<uint16_t>(si + 2));
        ++chan(9);
        si = lodsw();
        return Flow::Parse;
    }
    case 0x06: {  // STRINGRETURN
        --si;
        --chan(9);
        setBx(static_cast<uint16_t>(static_cast<uint8_t>(chan(9) << 1)));
        si = m16(static_cast<uint16_t>(bx() + di + 0x0A));
        return Flow::Parse;
    }
    case 0x07: chan(7) = ah; return Flow::Parse;  // OCTAVEOFFSET
    case 0x08:  // EOS (also every undefined opcode)
        chan(8) = 0;
        w16(static_cast<uint16_t>(di + 3), 0);
        if (m8(CURCHANNEL) != 9) keyOff();
        return Flow::NextChannel;
    case 0x09:  // REST
        setDuration();
        w16(static_cast<uint16_t>(di + 3), si);
        if (m8(CURCHANNEL) != 9) keyOff();
        return m8(LASTDUR) == 0 ? Flow::Parse : Flow::Effects;
    case 0x0A: al = lodsb(); outAdlib(); return Flow::Parse;  // SETOPLREG
    case 0x0B:  // NEWNOTE (legato)
        std::swap(al, ah);
        setFrequency();
        al = lodsb();
        std::swap(al, ah);
        setDuration();
        return durationFlow();
    case 0x0C: chan(0x12) = ah; return Flow::Parse;  // NOTEOFFSET
    case 0x0D:  // TABLEMODOP
        chan(0x3A) = ah;
        chan(0x39) = ah;
        al = lodsb();
        chan(0x3B) = al;
        chan(0x3C) = al;
        al = lodsb();
        chan(0x3D) = al;
        w16(static_cast<uint16_t>(di + 0x3E), lodsw());
        w16(static_cast<uint16_t>(di + 0x2B), TBLMOD);
        return Flow::Parse;
    case 0x0E: {  // STOPPARSE
        const uint16_t target = channelPtr(ah);
        m8(static_cast<uint16_t>(target + 5)) = 0;
        m8(static_cast<uint16_t>(target + 8)) = 0;
        w16(static_cast<uint16_t>(target + 3), 0);
        setBx(static_cast<uint16_t>(ah << 1));
        return Flow::Parse;
    }
    case 0x10: {  // STUFFPATCH
        const uint16_t saveSi = si;
        cl = m8(YAMOFF);
        setBx(static_cast<uint16_t>(ah << 1));
        si = m16(static_cast<uint16_t>(bx() + layout_.patchTable));
        writePatch();
        si = saveSi;
        return Flow::Parse;
    }
    case 0x11:  // PITCHDELTA
        chan(0x13) = ah;
        al = lodsb();
        std::swap(al, ah);
        al = lodsb();
        w16(static_cast<uint16_t>(di + 0x15), ax());
        w16(static_cast<uint16_t>(di + 0x29), DOPITCHDELTA);
        chan(0x14) = 0xFF;
        return Flow::Parse;
    case 0x12:  // CLEARPITCHDELTA
        --si;
        w16(static_cast<uint16_t>(di + 0x29), NOVECTOR);
        w16(static_cast<uint16_t>(di + 0x15), 0);
        return Flow::Parse;
    case 0x13: chan(0x24) = ah; return Flow::Parse;  // FRACPITCH
    case 0x15:  // VIBRATO
        chan(0x1D) = ah;
        al = lodsb();
        chan(0x19) = al;
        al = lodsb();
        chan(0x1A) = static_cast<uint8_t>(al + 1);
        al = static_cast<uint8_t>(al << 1);
        chan(0x1B) = al;
        al = lodsb();
        chan(0x1C) = al;
        w16(static_cast<uint16_t>(di + 0x29), DOVIBRATO);
        return Flow::Parse;
    case 0x1A: chan(8) = ah; return Flow::Parse;  // SETPRIORITY
    case 0x1C:  // INITSYNC
        ah = static_cast<uint8_t>(ah >> 1);
        m8(NOTESYNC) = ah;
        m8(NOTECNTR) = ah;
        m8(PRESYNC) = 0xFF;
        ah = 0;
        m8(SYNCBYTE) = 0;
        m8(SYNCHI) = 0;
        return Flow::Parse;
    case 0x1D: {  // WAITSYNC
        bool proceed = false;
        if (m8(SYNCHI) == 0) {
            ah &= m8(SYNCBYTE);
            if (ah == 0) ++m8(SYNCHI);
        } else {
            ah &= m8(SYNCBYTE);
            if (ah != 0) {
                m8(SYNCHI) = 0;
                proceed = true;
            }
        }
        if (proceed) return Flow::Parse;
        si = static_cast<uint16_t>(si - 2);
        chan(5) = 1;
        w16(static_cast<uint16_t>(di + 3), si);
        return Flow::NextChannel;
    }
    case 0x1E: chan(0x20) = ah; updateLevels(); return Flow::Parse;  // SETACCENTATTN
    case 0x20: setDuration(); return durationFlow();                   // STAY
    case 0x21: setDuration(); keyOn(); return durationFlow();          // RETRIGGER
    case 0x24: chan(0x2D) = ah & 7; return Flow::Parse;                // NOTEPERCENT
    case 0x26: m8(GLOBALTEMPO) = ah; return Flow::Parse;               // SETMUSICTEMPO
    case 0x27:  // KILLVECTOR2
        --si;
        w16(static_cast<uint16_t>(di + 0x2B), NOVECTOR);
        return Flow::Parse;
    case 0x29: chan(0x25) = ah; return Flow::Parse;  // SETTEMPO
    case 0x2B: chan(0x30) = ah; return Flow::Parse;  // SETCHANNELATTN
    case 0x2C:    // SETSYSATTN
    case 0x2D: {  // SETSYSATTNDELTA
        const uint16_t saveAx = ax(), saveDi = di;
        const uint8_t saveChannel = m8(CURCHANNEL);
        al = lodsb();
        m8(CURCHANNEL) = ah;
        setBx(static_cast<uint16_t>(ah << 1));
        di = m16(static_cast<uint16_t>(bx() + SCHNLPTR));
        if (op == 0x2D) al = static_cast<uint8_t>(al + m8(di));
        m8(di) = al;
        updateLevels();
        di = saveDi;
        setAx(static_cast<uint16_t>((saveAx & 0xFF00) | saveChannel));
        m8(CURCHANNEL) = saveChannel;
        return Flow::Parse;
    }
    case 0x2E:    // SETAMDEPTH
    case 0x2F: {  // SETVIBDEPTH
        const uint8_t shift = op == 0x2E ? 7 : 6;
        ah = static_cast<uint8_t>((ah & 1) << shift);
        bl = static_cast<uint8_t>((m8(SHADOWBD) & ~(1u << shift)) | ah);
        m8(SHADOWBD) = bl;
        ah = 0xBD;
        al = bl;
        outAdlib();
        return Flow::Parse;
    }
    case 0x30:  // SETSIGNEDATTN
        ah = static_cast<uint8_t>(ah + chan(0x20));
        chan(0x20) = ah;
        updateLevels();
        return Flow::Parse;
    case 0x33: {  // KILLCHANNEL
        const uint16_t saveAx = ax(), saveDi = di;
        const uint8_t saveChannel = m8(CURCHANNEL);
        if (ah == 0) m8(MOTORFLAG) = 0;
        m8(CURCHANNEL) = ah;
        setBx(static_cast<uint16_t>(ah << 1));
        di = m16(static_cast<uint16_t>(bx() + SCHNLPTR));
        chan(5) = 0;
        chan(8) = 0;
        w16(static_cast<uint16_t>(di + 3), 0);
        chan(0) = 0;
        if (m8(CURCHANNEL) != 9) {
            setBx(ah);
            cl = m8(static_cast<uint16_t>(bx() + OPOFFSETS));
            const uint8_t channel = m8(CURCHANNEL);
            ah = static_cast<uint8_t>(0xC0 + channel); al = 0; outAdlib();
            ah = static_cast<uint8_t>(0x43 + cl); al = 0x3F; outAdlib();
            ah = static_cast<uint8_t>(0x83 + cl); al = 0xFF; outAdlib();
            ah = static_cast<uint8_t>(0xB0 + channel); al = 0; outAdlib();
        }
        di = saveDi;
        setAx(static_cast<uint16_t>((saveAx & 0xFF00) | saveChannel));
        m8(CURCHANNEL) = saveChannel;
        return Flow::Parse;
    }
    case 0x35: {  // RANDOMPITCH
        al = lodsb();
        setBx(ax());
        random();
        setAx(ax() & bx());
        cl = chan(0x28);
        bh = cl;
        cl &= 0x20;
        bl = chan(0x27);
        bh &= 0x1F;
        setBx(static_cast<uint16_t>(bx() + ax()));
        bh |= cl;
        ah = static_cast<uint8_t>(0xA0 + m8(CURCHANNEL)); al = bl; outAdlib();
        ah = static_cast<uint8_t>(0xB0 + m8(CURCHANNEL)); al = bh; outAdlib();
        return Flow::Parse;
    }
    case 0x36:  // KILLVECTOR1
        --si;
        w16(static_cast<uint16_t>(di + 0x29), NOVECTOR);
        return Flow::Parse;
    case 0x39: {  // SETPITCHBEND: re-derive the current note with a new bend
        chan(0x42) = ah;
        al = chan(0x41);
        const uint16_t saved = ax();
        ch = al & 0xF0;
        ch = static_cast<uint8_t>(ch + chan(7));
        al &= 0x0F;
        al = static_cast<uint8_t>(al + chan(0x12));
        if (static_cast<int8_t>(al) >= 12) {
            al = static_cast<uint8_t>(al - 12);
            ch = static_cast<uint8_t>(ch + 0x10);
        } else if (static_cast<int8_t>(al) < 0) {
            al = static_cast<uint8_t>(al + 12);
            ch = static_cast<uint8_t>(ch - 0x10);
        }
        bl = static_cast<uint8_t>(al << 1);
        bh = 0;
        setAx(m16(static_cast<uint16_t>(bx() + FREQTABLE)));
        setDx(0);
        dl = chan(0x24);
        setAx(static_cast<uint16_t>(ax() + dl));
        ch = static_cast<uint8_t>(((ch >> 2) & 0x1C) | ah);
        cl = al;
        const uint8_t bend = chan(0x42);
        const uint8_t semi = chan(0x41) & 0x0F;
        if (static_cast<int8_t>(bend) >= 0) {  // no zero check here, unlike 14F0
            setBx(static_cast<uint16_t>((semi + 2) << 1));
            const uint16_t tbl = m16(bx());
            setBx(bend);
            al = m8(static_cast<uint16_t>(bx() + tbl));
            ah = 0;
            setCx(static_cast<uint16_t>(cx() + ax()));
        } else {
            setBx(static_cast<uint16_t>(semi << 1));
            const uint16_t tbl = m16(bx());
            setBx(static_cast<uint8_t>((bend ^ 0xFF) + 1));
            al = m8(static_cast<uint16_t>(bx() + tbl));
            ah = 0;
            setCx(static_cast<uint16_t>(cx() - ax()));
        }
        al = chan(0x28) & 0x20;
        ch |= al;
        chan(0x28) = ch;
        chan(0x27) = cl;
        ah = static_cast<uint8_t>(0xA0 + m8(CURCHANNEL)); al = cl; outAdlib();
        ah = static_cast<uint8_t>(0xB0 + m8(CURCHANNEL)); al = ch; outAdlib();
        setAx(saved);
        return Flow::Parse;
    }
    case 0x3A: --si; chan(0x25) = m8(GLOBALTEMPO); return Flow::Parse;  // GETMUSICTEMPO
    case 0x3B: --si; return Flow::Parse;                                // SNOP
    case 0x3C: chan(0x36) = ah; return Flow::Parse;                     // RANDOMDURATION
    case 0x3D: {  // TEMPODELTA
        const uint8_t tempo = chan(0x25);
        const unsigned sum = static_cast<unsigned>(ah) + tempo;
        if (static_cast<int8_t>(ah) >= 0) {
            ah = sum >= 0x100 ? 0xFF : static_cast<uint8_t>(sum);
        } else {
            ah = static_cast<uint8_t>(sum);
            if (ah >= tempo) ah = 1;
        }
        chan(0x25) = ah;
        return Flow::Parse;
    }
    case 0x3F: {  // MOTORON
        m8(MOTORFLAG) = ah;
        al = lodsb();
        const uint16_t saveSi = si;
        setBx(static_cast<uint16_t>(al << 1));
        setBx(m16(static_cast<uint16_t>(bx() + layout_.motorTables)));
        w16(DURTABLEPTR, bx());
        ++al;
        setBx(static_cast<uint16_t>(al << 1));
        setBx(m16(static_cast<uint16_t>(bx() + layout_.motorTables)));
        w16(PITCHTABLEPTR, bx());
        if (ah == 2) {
            setBx(0);
            w16(NOISEINDEX, 0);
            si = m16(DURTABLEPTR);
            m8(MOTORDUR) = m8(si);
            si = m16(PITCHTABLEPTR);
            al = m8(si);
            ah = 0xA0;
            outAdlib();
        }
        si = saveSi;
        return Flow::Parse;
    }
    case 0x40: --si; m8(MOTORFLAG) = 0; return Flow::Parse;  // MOTOROFF
    case 0x41: {  // DRUMSETUP: patches for channels 6-8, then B6/A6..B8/A8
        const uint8_t saveChannel = m8(CURCHANNEL);
        const uint8_t saveYamoff = m8(YAMOFF);
        const uint8_t patches[3] = {ah, lodsb(), lodsb()};
        const uint16_t afterPatches = si;
        static const uint16_t kShadowA[3] = {0x1F2, 0x1F4, 0x1F5};
        static const uint16_t kShadowB[3] = {0, 0x1F3, 0x1F6};
        for (int k = 0; k < 3; ++k) {
            m8(CURCHANNEL) = static_cast<uint8_t>(6 + k);
            setBx(OPOFFSETS);
            bl = static_cast<uint8_t>(bl + 6 + k);
            m8(YAMOFF) = m8(bx());
            setDx(0x388);
            cl = m8(YAMOFF);
            setBx(static_cast<uint16_t>(patches[k] << 1));
            si = m16(static_cast<uint16_t>(bx() + layout_.patchTable));
            if (k == 0) {
                m8(kShadowA[0]) = m8(static_cast<uint16_t>(si + 6));
            } else {
                m8(kShadowA[k]) = m8(static_cast<uint16_t>(si + 5));
                m8(kShadowB[k]) = m8(static_cast<uint16_t>(si + 6));
            }
            writePatch();
        }
        si = afterPatches;
        static const uint8_t kRegs[3][2] = {{0xB6, 0xA6}, {0xB7, 0xA7}, {0xB8, 0xA8}};
        static const uint16_t kB0Shadow[3] = {0x3F3, 0x436, 0x479};
        for (int k = 0; k < 3; ++k) {
            al = lodsb() & 0x2F;
            m8(kB0Shadow[k]) = al;
            ah = kRegs[k][0];
            outAdlib();
            al = lodsb();
            ah = kRegs[k][1];
            outAdlib();
        }
        m8(DRUMMASK) = 0x20;
        m8(YAMOFF) = saveYamoff;
        m8(CURCHANNEL) = saveChannel;
        return Flow::Parse;
    }
    case 0x42: {  // DODRUM: two writes to BD after one register latch
        const uint8_t bits = ah;
        m8(DRUMBITS) = bits & 0x1F;
        ah = 0xBD;
        al = static_cast<uint8_t>(((m8(DRUMBITS) ^ 0xFF) & m8(DRUMMASK)) | 0x20);
        outAdlib();
        al = static_cast<uint8_t>(bits | m8(DRUMMASK));
        m8(DRUMMASK) = al;
        al = static_cast<uint8_t>(al | m8(SHADOWBD) | 0x20);
        outAdlib();
        ah = 0xBD;
        return Flow::Parse;
    }
    case 0x43:  // DRUMOFF
        --si;
        al = m8(SHADOWBD);
        m8(DRUMMASK) = 0;
        al &= 0xC0;
        ah = 0xBD;
        outAdlib();
        return Flow::Parse;
    case 0x44:    // DRUMATTN
    case 0x45:    // DRUMFADE
    case 0x46: {  // DRUMMASTERATTN
        const int mode = op - 0x44;
        al = lodsb();
        setBx(ax());
        const uint8_t mask = bh, value = bl;
        struct Drum { uint8_t bit, reg; uint16_t shadow, master, attn; };
        static const Drum kDrums[5] = {
            {0x01, 0x51, 0x1F4, 0x1F9, 0x1FE},  // hi-hat
            {0x02, 0x55, 0x1F6, 0x1FB, 0x200},  // ride
            {0x04, 0x52, 0x1F5, 0x1FA, 0x1FF},  // tom
            {0x08, 0x54, 0x1F3, 0x1F8, 0x1FD},  // snare
            {0x10, 0x53, 0x1F2, 0x1F7, 0x1FC},  // kick
        };
        for (const Drum& d : kDrums) {
            setAx(bx());
            al = value;
            drumLevel(mask, d.bit, d.reg, d.shadow, d.master, d.attn, mode);
        }
        return Flow::Parse;
    }
    case 0x48: chan(0x40) = ah; return Flow::Parse;  // SETAUTOTEMPO
    case 0x49:  // SETVELOCITY
        chan(0x32) = ah;
        chan(0x33) = lodsb();
        return Flow::Parse;
    case 0x50:  // game event
        if (layout_.eventFlag) ++m8(layout_.eventFlag);
        if (ah != 0) pushEvent(ah);
        return Flow::Parse;
    case 0x51:  // channel-marker event
        if (layout_.eventFlag) ++m8(layout_.eventFlag);
        --si;
        pushEvent(static_cast<uint8_t>((m8(CURCHANNEL) & 0x0F) | 0xA0));
        return Flow::Parse;
    case 0x52:  // drum-marker event
        if (layout_.eventFlag) ++m8(layout_.eventFlag);
        --si;
        pushEvent(static_cast<uint8_t>((m8(DRUMBITS) & 0x1F) | 0x80));
        return Flow::Parse;
    default:
        return dispatch(0x08);  // unassigned table slots all point at EOS
    }
}

}  // namespace edison
