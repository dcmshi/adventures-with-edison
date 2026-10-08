// WMAIN.EXE: playing a room: the arcade's loop (f32_0c1d: a tick every
// 20 ms from the 50 Hz timer, mouse events when the mouse moves or its
// button is down or was pressed), the mouse handed out as the player does
// (f31_241a), and the panel's and columns' controls (segment 30). See
// docs/SCIENCE.md.

#include <algorithm>
#include <tuple>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "science/science.h"

namespace edison {

int Science::playRoom(int room) {
    // The room built, then the game's loop till a hole sends the ball to
    // another room (event 9: that room) or Escape (-1).
    enterRoom(room);
    if (const char* hole = std::getenv("SCI_HOLE"); hole && hasBall_) {
        // (Testing: SCI_HOLE=n, the first room's hole that leads to room n
        // takes the ball at once, as f28_14a5 does: for its dialogs and
        // exits without a measured shot. Once a run.)
        static bool used = false;
        for (Object& o : table_.objects)
            if (!used && o.type == 8 && o.args[0] == std::atoi(hole)) {
                used = true;
                int s[4];
                holeSphere(o, s);
                Ball& b = ball_;
                b.cx = s[0], b.cy = s[1], b.cz = s[2];
                for (int k = 0; k < 3; ++k) b.v[k] = 0, b.rem[k] = 0, b.kick[k] = 0, b.push[k] = 0;
                b.kickTicks = 0;
                b.hidden = true, shadowShown_ = false;
                o.swallow = 1, o.spit = 0, o.swallowDone = false;
            }
    }
    if (const char* search = std::getenv("SCI_AIMSEARCH")) {
        if (hasBall_) aimSearch(search);
        else logLine("SCI_AIMSEARCH: room " + std::to_string(room) + " has no ball to shoot");
        std::exit(0);  // (done: the log has the aims)
    }
    Mouse last;
    ctx_.platform.mouse(&last.x, &last.y, &last.held);
    bool lastPressed = false;  // [27D2]: the last event had a press
    uint64_t next = ctx_.platform.milliseconds() + 20;
    for (;;) {
        ctx_.pump();
        if (escapePressed()) return -1;
        if (exitRoom_) return exitRoom_;
        // f32_09a0: an event when the button is down or was pressed since
        // the last ([6EC5]), or the mouse moved, or (with it up) the last
        // event had a press. So a button let go without the mouse moving,
        // after an event without a press, isn't seen (as in the original).
        Mouse m;
        ctx_.platform.mouse(&m.x, &m.y, &m.held);
        int cx, cy;
        // (A press is seen with the button down, [6EC4] bit 0, even if it's
        // up already: the original takes one window message a loop, so its
        // release comes after the press's event. Room 96's greeting's second
        // click aims.)
        if (ctx_.platform.takeClick(&cx, &cy)) m.click = true, m.held = true, m.x = cx, m.y = cy;
        if ((lastPressed && !m.held) || m.held || m.click || m.x != last.x || m.y != last.y) {
            lastPressed = m.click;
            mouseEvent(m);
            last = m;
        }
        if (const int key = ctx_.platform.takeKey()) keyEvent(key);
        heartbeat("room " + std::to_string(currentRoom_) + " tick " + std::to_string(timerTicks_) +
                  (hasBall_ ? " ball " + std::to_string(ball_.cx) + "," + std::to_string(ball_.cy) + "," + std::to_string(ball_.cz) : ""));
        // The 50 Hz timer's tick (event 4).
        const uint64_t now = ctx_.platform.milliseconds();
        if (now >= next) {
            next += 20;
            if (next + 100 < now) next = now;  // fallen behind (a stall): no catching up
            tickRoom();
            if (shotHeld_) shoot();
            flushRoom();
            if (const char* dir = std::getenv("SCI_TICKSHOTS")) saveTickShot(dir);
        }
        flushRoom(false);
    }
}

void Science::aimSearch(const char* spec) {
    // (Testing: SCI_AIMSEARCH=to,power,x0,x1,y0,y1,step. From the room as
    // built, every aim in the grid (screen points, as a click there aims)
    // at the given power (-1: the room's own): the shot played tick by tick without drawing till
    // a hole takes the ball (or 750 ticks); the aims that reach the hole
    // leading to room `to` are logged ("aim x,y power p: hole to at tick t"),
    // for a shot to replay in the original; a negative `to`, the first
    // point target of kind -to hit (-100: the ball near a magnet; -101: a
    // switch turned over; -103: a type 11 met; -104: a type 0 ball or a
    // block moved; -105: a block met; -106: a pulling hole (type 9) took
    // the ball;
    // -107: a body into room 96's pit (+FC0); -108: the room's own code
    // ending it (a bin, a goal); -109: room 55's +FA2 or +FA4 set;
    // -102: the ball broken (heated by a fan, zapped by
    // an electromagnet) or caught by one). An
    // eighth number, the ball type; a ninth, ticks played before each
    // shot. Then the game ends.)
    int to = 0, power = 5, x0 = 0, x1 = 0, y0 = 0, y1 = 0, step = 1, type = -1, wait = 0;
    if (std::sscanf(spec, "%d,%d,%d,%d,%d,%d,%d,%d,%d", &to, &power, &x0, &x1, &y0, &y1, &step, &type, &wait) < 6 || step < 1) {
        logLine("SCI_AIMSEARCH: to,power,x0,x1,y0,y1[,step[,ball type[,wait]]]");
        return;
    }
    if (type >= 0) panel_.ballType = type, ball_.kind = type;
    // (SCI_AIMSEARCH_GRAVITY / _FRICTION: those sliders set too, unless the
    // room locks them, as the player couldn't.)
    for (const auto& [name, c, flags] : {std::tuple{"SCI_AIMSEARCH_GRAVITY", Control::Gravity, panel_.gravityFlags},
                                         std::tuple{"SCI_AIMSEARCH_FRICTION", Control::Friction, panel_.frictionFlags}}) {
        const char* v = std::getenv(name);
        if (!v) continue;
        if (flags != 0) logLine(std::string("SCI_AIMSEARCH: ") + name + " ignored: the room locks that slider");
        else setSlider(c, std::atoi(v));
    }
    if (power >= 0 && panel_.powerFlags != 0) logLine("SCI_AIMSEARCH: the room locks the power (set all the same)");
    const Ball ball = ball_;
    const std::vector<Object> objects = table_.objects;
    const PanelState panel = panel_;
    const int shots = shots_, roomTicks = roomTicks_, timerTicks = timerTicks_;
    const long score = score_, total = totalScore_;
    const bool shadow = shadowShown_, moving = ballMoving_;
    // (The target too: an aim whose line of sight misses the ground leaves
    // it where it was, as the original does, not at the last aim's.)
    const int targetX = targetX_, targetY = targetY_;
    const bool targetMoved = targetMoved_;
    decltype(roomVar_) roomVars;
    const int gameTicks = gameTicks_;
    std::copy(std::begin(roomVar_), std::end(roomVar_), roomVars);
    int found = 0;
    for (int y = y0; y <= y1; y += step)
        for (int x = x0; x <= x1; x += step) {
            ball_ = ball, table_.objects = objects, panel_ = panel;
            shots_ = shots, roomTicks_ = roomTicks, timerTicks_ = timerTicks, score_ = score, totalScore_ = total;
            shadowShown_ = shadow, ballMoving_ = moving, exitRoom_ = 0, exitNextTick_ = 0, levelBonus_ = 0, roomBusy_ = false, fuseBusy_ = false;
            targetX_ = targetX, targetY_ = targetY, targetMoved_ = targetMoved;
            std::copy(std::begin(roomVars), std::end(roomVars), roomVar_);
            gameTicks_ = gameTicks;
            for (int k = 0; k < 3; ++k) lastCentre_[k] = shadowSeen_[k] = (k == 0 ? ball_.cx : k == 1 ? ball_.cy : ball_.cz);
            if (power >= 0) setSlider(Control::Power, power);
            // (SCI_AIMSEARCH_SWITCHES=i,j,...: those objects of the room's
            // list (switches) turned over first, as clicks would.)
            if (const char* sw = std::getenv("SCI_AIMSEARCH_SWITCHES"))
                for (const char* p = sw; *p;) {
                    const size_t i = std::strtoul(p, const_cast<char**>(&p), 10);
                    if (i < table_.objects.size() && table_.objects[i].type == 7)
                        switchTurn(table_.objects[i], table_.objects[i].state ? 0 : 1);
                    if (*p == ',') ++p;
                    else break;
                }
            for (int k = 0; k < wait; ++k) tickRoom();
            Mouse m;
            m.x = x, m.y = y, m.held = true, m.click = true;
            aim(m);
            shoot();
            for (int t = 0; t < 750; ++t) {
                tickRoom();
                const Object* taken = nullptr;
                for (const Object& o : table_.objects)
                    if ((o.type == 8 || (to == -106 && o.type == 9)) && o.swallow) taken = &o;
                if (to == -100) {
                    // (The ball within 60 of a magnet's centre instead.)
                    bool near = false;
                    for (const Object& o : table_.objects)
                        if (o.type == 4 || o.type == 5 || o.type == 6 || o.type == 15) {
                            const int dx = o.body.cx - ball_.cx, dy = o.body.cy - ball_.cy, dz = o.body.cz - ball_.cz;
                            near |= dx * dx + dy * dy + dz * dz < 60 * 60;
                        }
                    if (near) {
                        logLine("aim " + std::to_string(x) + "," + std::to_string(y) + " power " + std::to_string(panel_.power) + ": near a magnet at tick " +
                                std::to_string(t));
                        ++found;
                        break;
                    }
                } else if (to == -102) {
                    // (An electromagnet catching the ball or breaking it.)
                    bool met = ball_.state != 0;
                    for (const Object& o : table_.objects) met |= o.type == 12 && o.emCaught;
                    if (met) {
                        logLine("aim " + std::to_string(x) + "," + std::to_string(y) + " power " + std::to_string(panel_.power) + ": " +
                                (ball_.state ? (ball_.heated ? "heated" : ball_.zapped ? "zapped" : "broken") : "caught") + " at tick " + std::to_string(t));
                        ++found;
                        break;
                    }
                } else if (to == -108) {
                    // (The room's own code ending it: a bin, a goal.)
                    if (exitRoom_ || exitNextTick_) {
                        logLine("aim " + std::to_string(x) + "," + std::to_string(y) + " power " + std::to_string(panel_.power) + ": to room " +
                                std::to_string(exitRoom_ ? exitRoom_ : exitNextTick_) + " with " + std::to_string(levelBonus_) + " at tick " + std::to_string(t));
                        ++found;
                        break;
                    }
                } else if (to == -109) {
                    // (Room 55's +FA2 or +FA4 set: a magnetic ball onto its box.)
                    if (roomVar_[0] || roomVar_[1]) {
                        logLine("aim " + std::to_string(x) + "," + std::to_string(y) + " power " + std::to_string(panel_.power) + ": +FA2 " +
                                std::to_string(roomVar_[0]) + " +FA4 " + std::to_string(roomVar_[1]) + " at tick " + std::to_string(t));
                        ++found;
                        break;
                    }
                } else if (to == -107) {
                    // (The room's +FC0 set: room 96's pit, room 2's circuit.)
                    if (roomVar_[3] > 0) {
                        logLine("aim " + std::to_string(x) + "," + std::to_string(y) + " power " + std::to_string(panel_.power) + ": +FC0 set at tick " +
                                std::to_string(t));
                        ++found;
                        break;
                    }
                } else if (to == -105) {
                    // (A block (type 16) met by the ball.)
                    bool met = false;
                    for (const Object& o : table_.objects) met |= o.type == 16 && o.ticks > 0;
                    if (met) {
                        logLine("aim " + std::to_string(x) + "," + std::to_string(y) + " power " + std::to_string(panel_.power) + ": a block at tick " +
                                std::to_string(t));
                        ++found;
                        break;
                    }
                } else if (to == -104) {
                    // (Another ball (type 0) or a block (16) set moving.)
                    bool moved = false;
                    for (size_t i = 0; i < table_.objects.size(); ++i)
                        moved |= (table_.objects[i].type == 0 || table_.objects[i].type == 2 || table_.objects[i].type == 16) && (table_.objects[i].body.cx != objects[i].body.cx || table_.objects[i].body.cy != objects[i].body.cy);
                    if (moved) {
                        logLine("aim " + std::to_string(x) + "," + std::to_string(y) + " power " + std::to_string(panel_.power) + ": a type 0 ball at tick " +
                                std::to_string(t));
                        ++found;
                        break;
                    }
                } else if (to == -103) {
                    // (A type 11 met: hit, or the Rubber ball it broke.)
                    bool met = false;
                    for (const Object& o : table_.objects) met |= o.type == 11 && (o.state == 3 || (o.state == 1 && ball_.state != 0));
                    if (met) {
                        logLine("aim " + std::to_string(x) + "," + std::to_string(y) + " power " + std::to_string(panel_.power) + ": type 11 met at tick " +
                                std::to_string(t));
                        ++found;
                        break;
                    }
                } else if (to == -101) {
                    // (A switch turned over instead.)
                    bool turned = false;
                    for (size_t i = 0; i < table_.objects.size(); ++i)
                        turned |= table_.objects[i].type == 7 && table_.objects[i].state != objects[i].state;
                    if (turned) {
                        logLine("aim " + std::to_string(x) + "," + std::to_string(y) + " power " + std::to_string(panel_.power) + ": a switch at tick " +
                                std::to_string(t));
                        ++found;
                        break;
                    }
                } else if (to < 0) {
                    // (A point target of kind -to hit instead.)
                    bool hit = false;
                    for (const Object& o : table_.objects)
                        if (o.type == 10 && o.kind == -to && o.hit) hit = true;
                    if (hit) {
                        logLine("aim " + std::to_string(x) + "," + std::to_string(y) + " power " + std::to_string(panel_.power) + ": kind " +
                                std::to_string(-to) + " at tick " + std::to_string(t));
                        ++found;
                        break;
                    }
                }
                if (taken) {
                    if (taken->args[0] == to || (to == -106 && taken->type == 9)) {
                        logLine("aim " + std::to_string(x) + "," + std::to_string(y) + " power " + std::to_string(panel_.power) + ": hole " +
                                std::to_string(to) + " at tick " + std::to_string(t));
                        ++found;
                    }
                    break;
                }
                if (exitRoom_ || ball_.state != 0) break;
            }
        }
    logLine("SCI_AIMSEARCH: " + std::to_string(found) + " aims reach hole " + std::to_string(to));
}

void Science::heartbeat(const std::string& where) {
    static const bool on = std::getenv("EDISON_LOG") != nullptr;
    if (!on) return;
    const uint64_t now = ctx_.platform.milliseconds();
    if (lastHeartbeat_ != 0 && now < lastHeartbeat_ + 2000) return;
    if (lastHeartbeat_ == 0) {
        lastHeartbeat_ = now;
        return;
    }
    lastHeartbeat_ = now;
    logLine("heartbeat " + std::to_string(now / 1000) + "s: " + where);
}

std::string Science::hexWord(unsigned v) {
    char s[8];
    std::snprintf(s, sizeof s, "%04X", v & 0xFFFF);
    return s;
}

void Science::saveTickShot(const std::string& dir) {
    // (Testing: the display after each tick, DIR/t<tick>.bmp, 24-bit, so a
    // run can be compared frame by frame whatever the machine's load.)
    const Screen& s = ctx_.screens[1];
    const int w = Screen::kWidth, h = Screen::kHeight, row = w * 3;
    std::vector<uint8_t> out(54 + static_cast<size_t>(row) * h);
    auto put32 = [&](size_t at, uint32_t v) { for (int i = 0; i < 4; ++i) out[at + i] = static_cast<uint8_t>(v >> (8 * i)); };
    out[0] = 'B', out[1] = 'M';
    put32(2, static_cast<uint32_t>(out.size())), put32(10, 54), put32(14, 40), put32(18, w), put32(22, h);
    out[26] = 1, out[28] = 24;
    put32(34, static_cast<uint32_t>(row) * h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const Rgb& c = ctx_.displayPalette[s.pixels[static_cast<size_t>(y) * w + x]];
            uint8_t* px = &out[54 + static_cast<size_t>(h - 1 - y) * row + 3 * x];
            px[0] = c.b, px[1] = c.g, px[2] = c.r;
        }
    if (FILE* f = std::fopen((dir + "/t" + std::to_string(timerTicks_) + ".bmp").c_str(), "wb")) {
        std::fwrite(out.data(), 1, out.size(), f);
        std::fclose(f);
    }
}

void Science::keyEvent(int key) {
    // f31_1d70, the player's method 3 (event 8, a key). With a room (+AE)
    // and nothing holding the keys (+B0):
    // - p: a pause, box 609 (face 1, style 1, narration 6181), 120 ticks,
    //   the music off, OK, the music on (music 25);
    // - q: "quit?" (face 0, 20B, buttons 202, sound 6016): yes, event 3;
    // - m: face 2, 613 (buttons 202, narration 618B), yes: 614 (618C),
    //   yes: face 0, 615 (style 1, sound 6018), event 9 to room 24.
    // Any time: s turns the sounds over ([27F6]; off: the WAV stopped and
    // the music off, f75_0000, f32_135d(0); on: music 25); S and two
    // digits (Borland's ctype, bit 2): event 9 to that room (1 outside
    // 1-110). Other keys go to the holder (+B0: none in play), else the
    // room (its +1C, f27_31fb in every room), else the panel. (No FM music
    // in this game yet.)
    const Rect& v = table_.view;
    auto box = [&](int face, int style, uint16_t message, uint16_t firstLine, int lines) {
        Dialog d;
        d.centreX = v.x + (v.w >> 1), d.centreY = v.y + (v.h >> 1);
        d.face = face, d.style = style, d.lines = lines, d.single = lines == 1, d.message = message, d.firstLine = firstLine;
        return d;
    };
    switch (key) {
    case 'p':
    case 'P': {
        Dialog d = box(1, 1, 0x609, 0, 1);
        dialogOpen(d);
        narration(0x6181, false);
        dialogWait(0x78);
        dialogRun(d);
        return;
    }
    case 'q':
    case 'Q': {
        Dialog d = box(0, 0, 0x20B, 0x202, 2);
        dialogOpen(d);
        sound(0x6016);
        if (dialogRun(d) != 0) exitRoom_ = -1;  // (event 3)
        return;
    }
    case 'm':
    case 'M': {
        Dialog first = box(2, 0, 0x613, 0x202, 2);
        dialogOpen(first);
        narration(0x618B, false);
        if (dialogRun(first) == 0) return;
        Dialog second = box(2, 0, 0x614, 0x202, 2);
        dialogOpen(second);
        narration(0x618C, false);
        if (dialogRun(second) == 0) return;
        Dialog third = box(0, 1, 0x615, 0, 1);
        dialogOpen(third);
        sound(0x6018);
        dialogRun(third);
        exitRoom_ = 24;
        return;
    }
    case 's':
        soundsOn_ = !soundsOn_;
        if (!soundsOn_) ctx_.platform.stopWav();
        return;
    case 'S': {
        // (f36_0000 till a key, twice.)
        int keys[2];
        for (int& k : keys)
            while ((k = ctx_.platform.takeKey()) == 0) ctx_.pump();
        if (keys[0] < '0' || keys[0] > '9' || keys[1] < '0' || keys[1] > '9') return;
        int room = (keys[0] - '0') * 10 + (keys[1] - '0');
        if (room < 1 || room > 110) room = 1;
        exitRoom_ = room;
        return;
    }
    case 'r':
        // f27_31fb, the room's +1C: with +F85 (always set, f27_03da), the
        // room not busy (+F6F), the ball not breaking (+7C) and neither
        // column waiting for its PUSH (+132): the ball slid from its centre
        // (f07_04c1, as its bottom) to the room's place (f27_293b), and 300
        // points off the score (f06_0208). Other keys: nothing.
        if (roomBusy_ || !hasBall_ || ball_.state != 0 || !columns_[0].ballOut || !columns_[1].ballOut) return;
        slideBall(ball_.cx, ball_.cy, ball_.cz);
        addScore(-300);
        return;
    default:
        return;
    }
}

void Science::mouseEvent(const Mouse& m) {
    // f31_241a: to whoever has the mouse ([27AC]); else the room if it's in
    // the view, the panel, the columns.
    if (captured_ == Control::Gravity || captured_ == Control::Friction || captured_ == Control::Power) {
        sliderClick(captured_, m);
        return;
    }
    if (captured_ == Control::BallType || captured_ == Control::Shoot) {
        buttonClick(captured_, m);
        return;
    }
    if (inside(table_.view, m.x, m.y)) {
        // f27_2d15: the objects first (the room's list, +18E, from its
        // start), then aiming.
        for (Object& o : table_.objects)
            if (thingClick(o, m)) return;
        aim(m);
        return;
    }
    const int panelTop = table_.view.y + table_.view.h;
    if (m.y >= panelTop) {
        if (panelBusy_) return;  // [22C8]: Edison is putting signs up
        // f30_1879: the first control whose area has the point (and that
        // isn't locked, +2E), its click (+8).
        struct Area {
            Control c;
            Rect r;
            int flags;
        };
        const Area areas[] = {
            {Control::Gravity, {0, 300, 0xAE, 100}, panel_.gravityFlags},
            {Control::Friction, {0xAE, 300, 0x68, 100}, panel_.frictionFlags},
            {Control::BallType, {0x116, 300, 0x74, 100}, panel_.ballTypeFlags},
            {Control::Power, {0x18A, 300, 0x6A, 100}, panel_.powerFlags},
            {Control::Shoot, {500, 300, 0x8C, 100}, 0},
        };
        for (const Area& a : areas) {
            if (!inside(a.r, m.x, m.y) || (a.flags & 3) != 0) continue;
            if (a.c == Control::BallType || a.c == Control::Shoot) buttonClick(a.c, m);
            else sliderClick(a.c, m);
            return;
        }
        return;
    }
    // The columns (f30_0496): their PUSH buttons.
    columnClick(false, m);
    columnClick(true, m);
}

bool Science::sliderClick(Control c, const Mouse& m) {
    // f30_306a: a press takes the mouse; while it's held, every 2 panel
    // ticks (7 after the first step) the value steps towards the pointer
    // (up when it's above the knob's line, f30_33c7); a release lets go.
    if (captured_ == Control::None) {
        if (!m.click) return false;
        captured_ = c;
        sliderRepeats_ = 0;
        panelTicks_ = 0;
        return true;
    }
    if (m.held) {
        if (panelTicks_ < (sliderRepeats_ == 1 ? 7 : 2)) return true;
        panelTicks_ = 0;
        int& value = c == Control::Gravity ? panel_.gravity : c == Control::Friction ? panel_.friction : panel_.power;
        const int min = c == Control::Gravity ? -16 : 0, max = c == Control::Gravity ? 4 : 16;
        const uint16_t frame0 = c == Control::Gravity ? 0x1188 : c == Control::Friction ? 0x1191 : 0x119A;
        const int x0 = c == Control::Gravity ? 0x50 : c == Control::Friction ? 0xC0 : 0x1A8;
        (void)x0;
        const Bitmap& frame = ctx_.bitmap(frame0);
        const int index = (value - min) * (9 - 1) / (max - min);
        const int line = frame.height - index * frame.height / 9 + 299 - 1;  // f30_2fdf
        int v = value;
        if (m.y < line - 2) ++sliderRepeats_, ++v, sound(0x602A);
        else if (m.y > line + 2) ++sliderRepeats_, --v, sound(0x6029);
        setSlider(c, v);
        return true;
    }
    captured_ = Control::None;  // f32_00cf
    sliderRepeats_ = 0;
    return true;
}

void Science::setSlider(Control c, int value) {
    // f30_3339: kept in the range, then the kind's own (+28): gravity
    // (f30_39fc: the room's gravity, f27_108d; its box -value / 4.0),
    // friction (f30_3bb2: the room's +F07), power (f30_3739: the shot's
    // speed, f27_26e3). The physics isn't ported yet: only the values.
    if (c == Control::Gravity) panel_.gravity = std::clamp(value, -16, 4);
    else if (c == Control::Friction) panel_.friction = std::clamp(value, 0, 16);
    else panel_.power = std::clamp(value, 0, 16);
    applyPanelPhysics();
    // f29_0313: its sprite's rectangle (its first frame's size) and its box.
    const uint16_t frame0 = c == Control::Gravity ? 0x1188 : c == Control::Friction ? 0x1191 : 0x119A;
    const int x0 = c == Control::Gravity ? 0x50 : c == Control::Friction ? 0xC0 : 0x1A8;
    const Bitmap& f = ctx_.bitmap(frame0);
    markPanel({x0, 299, f.width, f.height});
    const Rect box = c == Control::Gravity ? Rect{0x4A, 0x172, 0x28, 0x12} : c == Control::Friction ? Rect{200, 0x172, 0x20, 0x12} : Rect{0x1C6, 0x172, 0x20, 0x12};
    markPanel(box);
}

void Science::markPanel(const Rect& r) {
    // f29_0313: a changed rectangle of the panel, joined to the others.
    if (!panelDirty_) {
        panelDirtyRect_ = r;
    } else {
        const int x0 = std::min(panelDirtyRect_.x, r.x), y0 = std::min(panelDirtyRect_.y, r.y);
        const int x1 = std::max(panelDirtyRect_.x + panelDirtyRect_.w, r.x + r.w);
        const int y1 = std::max(panelDirtyRect_.y + panelDirtyRect_.h, r.y + r.h);
        panelDirtyRect_ = {x0, y0, x1 - x0, y1 - y0};
    }
    panelDirty_ = true;
}

void Science::markButton(Control c) {
    if (c == Control::BallType) {
        markPanel({0x124, 0x150, 80, 13});
        markPanel({0x134, 0x140, 0x4C, 0x12});
    } else {
        markPanel({0x1F0, 0x13B, 79, 27});
    }
}

void Science::applyPanelPhysics() {
    // Gravity (f30_39fc → f27_108d): p = (-value * [2360] (4) * 42) /
    // (50 * 2), the room's ratio p * -200 / 10. Friction (f30_3bb2): +F07 =
    // value * +F0B (255) * 8 / 256. Power (f30_3739 → f27_26e3): value *
    // (168 * 3 / 8) / 16 (2000 at the top), at most 7FFFh / 168, times 168.
    const int16_t p = static_cast<int16_t>(static_cast<int16_t>(-panel_.gravity * 4 * kTimerK) / (kTimerRate * 2));
    gravity_ = p * -200 / 10;
    frictionDen_ = 255;
    frictionNum_ = static_cast<int32_t>(panel_.friction) * frictionDen_ * 8 / 0x100;
    const int unit = kTimerK * 200 / kTimerRate;  // 168
    int speed = panel_.power >= 16 ? 2000 : static_cast<int16_t>(static_cast<int32_t>(panel_.power) * (unit * 3 / 8) / 0x10);
    speed = std::min(speed, 0x7FFF / unit);
    power_ = unit * speed;
}

void Science::shoot() {
    // f27_27a3 → f06_09a1: with a ball and its target, at the target's
    // middle (its box's x + w / 2, y + d / 2, its foot); sound 6003; a shot
    // more.
    if (!hasBall_ || ball_.hidden) return;  // (not while a hole has it)
    if (const char* when = std::getenv("SCI_SHOOT_WHEN")) {
        // (Testing: the shot held till the ball is in this state, "cx,cy,cz,
        // vx,vy,vz", the original's when its shot came, so a trace taken
        // while the ball moves can be compared from there.)
        const Ball& b = ball_;
        const std::string now = std::to_string(b.cx) + "," + std::to_string(b.cy) + "," + std::to_string(b.cz) + "," +
                                std::to_string(b.v[0]) + "," + std::to_string(b.v[1]) + "," + std::to_string(b.v[2]);
        shotHeld_ = now != when;
        if (shotHeld_) return;
    }
    const int z = heightUnder(targetX_, targetY_) + (targetMoved_ ? 1 : 0);
    if (std::getenv("SCI_DEBUG")) logLine("shoot at " + std::to_string(targetX_) + "," + std::to_string(targetY_) + "," + std::to_string(z) + " power " + std::to_string(power_) + " type " + std::to_string(ball_.kind) + " from " + std::to_string(ball_.cx) + "," + std::to_string(ball_.cy) + "," + std::to_string(ball_.cz));
    ballLaunch(targetX_, targetY_, z);
    sound(0x6003);
    ++shots_;
    viewDirty_ = true;
}

void Science::crackGlass() {
    // f27_0772: sound 6019; a mark on the glass (the room's five, +F3D
    // rectangles and +F65 stages: the first free one, else the first) at
    // the ball's rectangle's corner, the size of sprite 1164, at stage 1.
    sound(0x6019);
    int slot = 0;
    while (slot < 5 && crackStage_[slot] != 0) ++slot;
    if (slot == 5) slot = 0;
    const Ball& b = ball_;
    const Rect r = objectRect(b.cx - b.r, b.cy - b.r, b.cz - b.r, 2 * b.r + 1, 2 * b.r + 1, 2 * b.r + 1);
    const Bitmap& mark = ctx_.bitmap(0x1164);
    crackStage_[slot] = 1;
    crackRect_[slot] = {r.x, r.y, mark.width, mark.height};
    viewDirty_ = true;
}

bool Science::buttonClick(Control c, const Mouse& m) {
    // f30_28f1: a press (not while another control has the mouse, nor
    // [22C8]) takes the next state and shows the button down; a release
    // shows it up.
    if (captured_ == Control::None) {
        if (!m.click) return false;
        captured_ = c;
        if (c == Control::BallType) {
            ballTypePressed_ = true;
            setBallType((panel_.ballType + 1) % 6);
        } else {
            shootPressed_ = true;
            shoot();  // f30_281d → f27_27a3
        }
        markButton(c);
        return true;
    }
    if (!m.held) {
        captured_ = Control::None;
        ballTypePressed_ = shootPressed_ = false;
        markButton(c);
    }
    return false;
}

void Science::setBallType(int type) {
    // f30_2574: Magic (5) goes back to Ice; the ball told (its +8); the
    // name in the box; sound 6007.
    panel_.ballType = type == 5 ? 0 : type;
    ball_.kind = panel_.ballType;  // f07_05bf (its mass, its record)
    sound(0x6007);
    markButton(Control::BallType);
    viewDirty_ = true;  // the ball's look (f07_04d5)
}

void Science::columnClick(bool right, const Mouse& m) {
    // f30_0496: in the PUSH button (left (0, 16), right (604, 145), 35 x
    // 43), held, with no ball of its own on the table and not already
    // pushing: the top ball starts up the tube (FM sound 0).
    const Rect push = right ? Rect{0x25C, 0x91, 0x23, 0x2B} : Rect{0, 0x10, 0x23, 0x2B};
    if (!inside(push, m.x, m.y)) return;
    Column& col = columns_[right ? 1 : 0];
    if (col.ballOut || col.pushing || !m.held) return;
    fmSound(0);
    col.pushing = true;
    col.offset = 1;
    columnDirty_[right ? 1 : 0] = true;
}

void Science::aim(const Mouse& m) {
    // f27_2d15: with the button held and a target, along the line of
    // sight from the point at depth 0 (direction (-100, 282, -100), steps
    // 1, 5, 9, ... long, f11_02f7) to where it meets the ground (within 1),
    // and the target there (f08_056e: sound 6026 if it moved far), sound
    // 6023. Past depth 314h, nothing.
    if (!m.held || !hasBall_) return;
    const Rect& v = table_.view;
    const int bottom = v.y + v.h - 1;
    const int baseX = m.x - v.x - table_.scrollX, baseH = bottom - m.y;
    static const int kSight[3] = {-100, 282, -100};
    int unit[3];
    libNormalize(kSight, unit);  // f11_02f7
    const int ux = unit[0], uy = unit[1], uh = unit[2];
    int x = baseX, y = 0, h = baseH;
    for (int step = 1;; step += 4) {
        if (y > 0x314) return;
        x = baseX + static_cast<int>(static_cast<long>(ux) * step / 0x7FFE);
        y = static_cast<int>(static_cast<long>(uy) * step / 0x7FFE);
        h = baseH + static_cast<int>(static_cast<long>(uh) * step / 0x7FFE);
        const int d = h - heightUnder(x, y);
        if (std::getenv("SCI_AIMDEBUG")) logLine("aim step " + std::to_string(step) + " " + std::to_string(x) + "," + std::to_string(y) + "," + std::to_string(h) + " ground " + std::to_string(h - d) + " face " + std::to_string(faceUnder(x, y).type) + " faceHeight " + std::to_string(faceHeight(faceUnder(x, y), x, y)));
        if (d <= 1 && d >= -1) break;
    }
    const int dx = targetX_ - x, dy = targetY_ - y;
    if (dx * dx + dy * dy > 15 * 15) sound(0x6026);
    targetX_ = x, targetY_ = y;
    targetMoved_ = true;
    sound(0x6023);
    viewDirty_ = true;
}

int Science::libAtan2(int adjacent, int opposite) const {
    // f87_0804: the table (seg87:0000, 400h words) by the smaller over the
    // larger, in 10000h a turn; then the quadrant.
    auto table = [&](uint32_t ratio) {
        const size_t at = (ratio >> 5) & 0xFFFE;
        return static_cast<int>(static_cast<int16_t>(atans_[at] | atans_[at + 1] << 8));
    };
    const bool negX = adjacent < 0, negY = opposite < 0;
    const uint32_t x = static_cast<uint16_t>(negX ? -adjacent : adjacent);
    const uint32_t y = static_cast<uint16_t>(negY ? -opposite : opposite);
    int a;
    if (y == 0) {
        if (x == 0) return 0;
        a = 0;
    } else if (y < x) {
        a = table((y << 16) / x);
    } else if (x == y) {
        a = 0x2000;
    } else if (x == 0) {
        a = 0x4000;
    } else {
        a = 0x4000 - table((x << 16) / y);
    }
    if (negX) a = -0x8000 - a;
    if (negY && y != 0) a = -a;
    return static_cast<int16_t>(a);
}

std::pair<int, int> Science::libSinCos(int angle) const {
    // f86_1000: a quarter of a sine (seg86:0000, 800h words, 65534ths)
    // halved; the cosine from the other end.
    const uint16_t a = static_cast<uint16_t>(angle);
    const size_t at = (a >> 2) & 0xFFE;
    auto word = [&](size_t o) { return static_cast<int>(static_cast<uint16_t>(sines_[o] | sines_[o + 1] << 8) >> 1); };
    int s = word(at), c = word(0xFFE - at);
    if (a & 0x4000) std::swap(s, c);
    if (((a >> 14) ^ (a >> 15)) & 1) c = -c;
    if (a & 0x8000) s = -s;
    return {s, c};
}

void Science::libNormalize(const int v[3], int out[3]) const {
    // f84_0000: the heading of (x, y), the length across, the elevation;
    // the unit vector from their sines and cosines.
    const auto [s1, c1] = libSinCos(libAtan2(v[0], v[1]));
    const long across = static_cast<long>(c1) * v[0] + static_cast<long>(s1) * v[1];
    const int acrossHigh = static_cast<int>(static_cast<uint16_t>((static_cast<uint32_t>(across) >> 16) << 1) |
                                            (static_cast<int16_t>(across & 0xFFFF) < 0 ? 1 : 0));
    const auto [s2, c2] = libSinCos(libAtan2(v[2], static_cast<int16_t>(acrossHigh)));
    out[2] = c2;
    auto mulHigh = [](long p) {
        return static_cast<int>(static_cast<int16_t>(((static_cast<uint32_t>(p) >> 16) << 1) |
                                                     (static_cast<int16_t>(p & 0xFFFF) < 0 ? 1 : 0)));
    };
    out[0] = mulHigh(static_cast<long>(s2) * c1);
    out[1] = mulHigh(static_cast<long>(s1) * s2);
}

void Science::tickRoom() {
    // f31_1c79: the room's tick (f27_2434), the panel's (f30_1430: [8E4E]
    // counts), the columns' (f30_02d8: a pushed ball goes up 6 a tick,
    // sound 602A, till it's out: one ball fewer, and the ball dropped on
    // the table, f27_293b).
    ++panelTicks_;
    ++timerTicks_;
    ++gameTicks_;
    cycleStep();
    // (A room change a face hook queued last tick: after this one.)
    const int exitAfter = exitNextTick_;
    exitNextTick_ = 0;
    runnerTick();
    // f27_2434: each object's tick (method 0), the list (+18E) from its
    // end: so the ball's target and shadow see where the ball was, and
    // the holes before it in the list come after it.
    roomTick();
    for (size_t i = table_.objects.size(); i-- > 0;) {
        Object& o = table_.objects[i];
        if (o.type == 8) holeTick(o);
        else if (o.type == 9) pullTick(o);
        else if (o.type == 10 && o.kind == 6) sparkTick(o), suckholeTick(o);
        else if (o.type == 10) pointTick(o);
        else if (o.type == 11) creatureTick(o), pointTick(o);
        else if (o.type == 7 || o.type == 4 || o.type == 12 || o.type == 13 || o.type == 14 || o.type == 16) thingTick(o);
        else if (o.type == 0 || o.type == 2) ballTick(o.body);
        else if ((o.type == 1 || o.type == 3) && hasBall_) {
            targetTick();
            shadowTick();
            ballTick(ball_);
        }
        if (exitRoom_) return;
    }
    // [FFE]: the room ticks, from 0 past 1000; every 20 the glass's cracks
    // grow (f27_2434: +F65, up to 3).
    if (++roomTicks_ % 20 == 0) growCracks();
    if (roomTicks_ > 1000) roomTicks_ = 0;
    if (exitAfter && !exitRoom_) exitRoom_ = exitAfter;
    if (hasBall_ && std::getenv("SCI_DEBUG")) {
        // (Testing: where the ball's sprite is drawn, and which.)
        const Ball& b = ball_;
        const auto [bx, by] = objectCentre(b.cx - b.r, b.cy - b.r, b.cz - b.r, 2 * b.r + 1, 2 * b.r + 1, 2 * b.r + 1);
        logLine("t" + std::to_string(timerTicks_) + " sprite " + std::to_string(bx - 16) + "," + std::to_string(by - 10) + " f" +
                std::to_string(b.drawFrame >> 1) + " c " + std::to_string(b.cx) + "," + std::to_string(b.cy) + "," + std::to_string(b.cz) +
                " v " + std::to_string(b.v[0]) + "," + std::to_string(b.v[1]) + "," + std::to_string(b.v[2]) +
                " r " + std::to_string(b.rem[0]) + "," + std::to_string(b.rem[1]) + "," + std::to_string(b.rem[2]));
    }
    if (std::getenv("SCI_DEBUG")) {
        // (Testing: the loose magnets and other balls (type 0) with the
        // ball, for tracecmp.py: the ball's centre and velocity, then each
        // body's box corner and velocity, as the original's cores have them,
        // +6E and +62.)
        std::string line;
        for (const Object& o : table_.objects)
            if (o.type == 4 || o.type == 0 || o.type == 2 || o.type == 16) {
                int b[6];
                if (o.type != 0 && o.type != 2) thingBox(o, b);
                else b[0] = o.body.cx - o.body.r, b[1] = o.body.cy - o.body.r, b[2] = o.body.cz - o.body.r;
                line += " " + std::to_string(b[0]) + "," + std::to_string(b[1]) + "," + std::to_string(b[2]) + "," + std::to_string(o.body.v[0]) +
                        "," + std::to_string(o.body.v[1]) + "," + std::to_string(o.body.v[2]);
            }
        if (!line.empty() && hasBall_)
            logLine("bodies t" + std::to_string(timerTicks_) + " " + std::to_string(ball_.cx) + "," + std::to_string(ball_.cy) + "," + std::to_string(ball_.cz) + "," +
                    std::to_string(ball_.v[0]) + "," + std::to_string(ball_.v[1]) + "," + std::to_string(ball_.v[2]) + line);
    }
    for (int c = 0; c < 2; ++c) {
        Column& col = columns_[c];
        if (!col.pushing || col.offset == 0) {
            if (col.pushing) col.pushing = false, columnDirty_[c] = true;
            continue;
        }
        sound(0x602A);
        col.offset += 6;
        const int y = c ? 0x8C : 6, h = c ? 0x96 : 0xFA;
        const int count = c ? rightBalls_ : leftBalls_;
        const int topBall = y + h - 0x1D - 0x18 * (count - 1);
        columnDirty_[c] = true;
        if (col.offset < topBall - y) continue;
        col.offset = 0;
        if (c) rightBalls_ = std::max(rightBalls_ - 1, 0);
        else leftBalls_ = std::max(leftBalls_ - 1, 0);
        col.ballOut = true;
        if (hasBall_) dropBall(c == 1);
    }
}

void Science::shadowTick() {
    // f07_15ba: the shadow object follows the ball when its centre changed
    // (hidden or not): shown, the ball breaking (+7C), it's hidden; its
    // bottom within 1 of the ground under it, hidden; else put on the
    // ground below it (x, y, ground + 1), shown again if the ball is.
    // (Hiding the ball hides it too: f07_03be, f07_0381.)
    const Ball& b = ball_;
    if (shadowHeld_ && (b.cx != heldAt_[0] || b.cy != heldAt_[1] || b.cz != heldAt_[2])) shadowHeld_ = false, viewDirty_ = true;
    if (b.cx == shadowSeen_[0] && b.cy == shadowSeen_[1] && b.cz == shadowSeen_[2]) return;
    if (shadowShown_ && b.state != 0) {
        shadowShown_ = false, viewDirty_ = true;
        return;
    }
    const int ground = heightUnder(b.cx, b.cy);
    const int above = b.cz - b.r - ground;
    shadowSeen_[0] = b.cx, shadowSeen_[1] = b.cy, shadowSeen_[2] = b.cz;
    if (above > -2 && above < 2) {
        if (shadowShown_) shadowShown_ = false, viewDirty_ = true;
        return;
    }
    if (!shadowShown_ && !b.hidden) shadowShown_ = true;
    shadowX_ = b.cx, shadowY_ = b.cy, shadowZ_ = ground + 1;
    viewDirty_ = true;
}

void Science::targetTick() {
    // f06_0aa8: the ball still when slow (|speed| <= 100) or not moved
    // since the last tick; the target shown only then (its +60); a change
    // redraws.
    const Ball& b = ball_;
    const int32_t speed = libLength(b.v);
    bool still = (speed < 0 ? -speed : speed) <= 100;
    if (b.cx == lastCentre_[0] && b.cy == lastCentre_[1] && b.cz == lastCentre_[2]) still = true;
    lastCentre_[0] = b.cx, lastCentre_[1] = b.cy, lastCentre_[2] = b.cz;
    if (ballMoving_ == still) viewDirty_ = true;
    ballMoving_ = !still;
}

void Science::flushRoom(bool view) {
    // The view at the room's tick's end (f27_2434 → f29_0380): what changed
    // queued, the areas redrawn.
    if (view) {
        if (viewDirty_) noteChanges(), viewDirty_ = false;
        redrawAreas();
    }
    if (panelDirty_) drawPanel(&panelDirtyRect_), panelDirty_ = false;
    for (int c = 0; c < 2; ++c)
        if (columnDirty_[c]) drawColumn(c == 1), columnDirty_[c] = false;
}

}  // namespace edison
