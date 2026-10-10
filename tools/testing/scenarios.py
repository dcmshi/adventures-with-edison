"""The scenarios scenario.py plays in the port and the original.

Each starts in a table room (the original gets there with S and the room's
two digits, origrun.py) and is a timeline in seconds after the room is up:
presses, drags, moves and typing, and the moments to take shots. Masks are
boxes (x0, y0, x1, y1) left out of the comparison: what is random in the
original too. Other differences the known counts allow for: things caught
at another moment of their animation (the lab's professor, room 21's
objects), Edison walking to put up an OUT OF ORDER sign, and a box's
backdrop (one of ten by rand, f24's dialogs: lesson 9's end).
"""

# The columns' balls: their frames come from Borland's rand, which the
# original seeds by the time (its own runs differ there).
COLUMNS = [(0, 0, 40, 400), (600, 0, 640, 400)]


def click(t, x, y):
    return (t, "click", x, y)


def drag(t, x0, y0, x1, y1, seconds):
    return (t, "drag", x0, y0, x1, y1, seconds)


def move(t, x, y):
    return (t, "move", x, y)


def typed(t, text):
    return (t, "type", text)


# Keys held by name: the original's virtual-key codes, the port's --key names.
VKEYS = {"shift": 16, "esc": 27}


def key(t, name, seconds=0.08):
    """A key held (no character: Shift)."""
    return (t, "key", name, seconds)


def shoot(t, x, y):
    """Aim at (x, y) (the mouse moved off after), then SHOOT a second on."""
    return [click(t, x, y), move(t + 0.18, x + 4, y), click(t + 1.0, 540, 340), move(t + 1.13, 544, 340)]


def every(first, last, step, prefix="t"):
    out, t = [], first
    while t <= last + 1e-9:
        out.append((round(t, 2), f"{prefix}{int(round(t * 10)):03d}"))
        t += step
    return out


# Power 16 (the POWER lever dragged up), as the lesson and game-won runs set it.
POWER16 = drag(0.0, 445, 325, 445, 250, 2.5)


def hole(aim):
    """Room 1: a shot at a hole, then shots every half second."""
    # (The ball rolling into the hole: its shots dense.)
    return {"room": 1, "events": shoot(1.0, *aim), "shots": every(2.5, 12, 0.5), "length": 13,
            "dense": ["t025", "t030", "t035"]}


def lesson(n):
    """Room 61's hole to lesson n (506-510) at power 16, then at each bubble
    MORE; lessons 6-8 first press the professor (the easter egg)."""
    aims = {6: (306, 176), 7: (240, 206), 8: (240, 212), 9: (132, 230), 10: (144, 242)}
    steps = {6: 6, 7: 3, 8: 3, 9: 3, 10: 4}
    events = [drag(3.0, 445, 325, 445, 250, 2.5), *shoot(6.0, *aims[n])]
    shots, t = [], 12.0
    for k in range(steps[n]):
        shots.append((t, f"s{k + 1}"))
        if k == 0 and n <= 8:
            events.append(drag(t + 0.5, 530, 170, 530, 170, 0.7))
            shots.append((round(t + 0.9, 2), "egg"))
            t += 2.0
        events.append(click(t + 1.0, 550, 380))
        t += 3.0
    shots.append((t + 1.0, "end"))
    # (The end is the next table: its columns masked, as everywhere.)
    s = {"room": 61, "events": events, "shots": shots, "length": t + 3,
         "dense": [n for _, n in shots if n in ("egg", "end")]}
    if n == 9:
        # Room 50's greeting box: its picture is one of five by rand()
        # (f24_193e), whose calls in the original change from run to run
        # (the columns' redraws): the port takes the one its last run drew.
        s["env"] = {"SCI_DIALOGPIC": "4"}
    return s


def game_over():
    """Room 32: all eight balls (one on the table, seven in the left column)
    shot into its lava, a PUSH after each but the last; the high scores."""
    events, t = [click(0.0, 300, 225)], 2.0
    for k in range(8):
        events += shoot(t, 310, 242)
        if k < 7:
            events.append(click(t + 4.0, 15, 45))
        t += 6.0
    return {"room": 32, "events": events, "shots": [(t + 3.0, "hs")], "length": t + 5}


def game_won():
    """Room 65 at power 16 into its hole to 510: the bonus box, lesson 10
    (MORE four times), the game won and the high scores."""
    events = [POWER16, *shoot(3.0, 224, 160), click(11.2, 320, 150)]
    events += [click(t, 550, 380) for t in (15.3, 17.3, 19.3, 21.3)]
    return {"room": 65, "events": events, "shots": [(2.5, "rest"), (11.1, "bonus"), (15.2, "lesson"), (25.5, "hs")],
            "length": 27}


def game_won_shift():
    """game_won with Shift alone where a key or click ends a wait ([9558]
    is set for any key down): the bonus box (f38_020f), the lesson's first
    MORE (f24_193e: a key other than y is the first button), and the high
    scores (f40_0000), closed."""
    events = [POWER16, *shoot(3.0, 224, 160), key(11.2, "shift")]
    events += [key(15.3, "shift")] + [click(t, 550, 380) for t in (17.3, 19.3, 21.3)] + [key(26.0, "shift")]
    return {"room": 65, "events": events,
            "shots": [(11.1, "bonus"), (12.0, "bonus2"), (15.2, "lesson"), (16.2, "more"), (25.5, "hs"), (27.5, "closed")],
            "length": 29}


def lesson_keys():
    """Lesson 6 from room 61, its lines gone on by keys (the lesson's key
    method, g15_0b28): Shift alone, then "a" (MORE, +44), then Esc (its
    end, +48: room 50)."""
    events = [drag(3.0, 445, 325, 445, 250, 2.5), *shoot(6.0, 306, 176), key(13.0, "shift"), typed(16.0, "a"),
              key(19.0, "esc")]
    shots = [(12.0, "s1"), (14.0, "s2"), (17.0, "s3"), (21.0, "end"), (23.0, "end2")]
    # (Room 50's Edison comes in from a random side to put up the sign
    # (f30_0910, rand()): from the left in the original's last run.)
    return {"room": 61, "events": events, "shots": shots, "length": 25, "env": {"SCI_RUNNERSIDE": "left"},
            "dense": ["end"]}


SCENARIOS = {
    # Room 1 at rest: the view, the panel's controls and boxes.
    "rest-1": {"room": 1, "events": [], "shots": [(1.0, "rest"), (2.0, "rest2")], "length": 3},
    # The lab's burner and bubbles: their last frame comes from how many of
    # their 7/50 s waits fit in each walking frame's countdown, which in the
    # original changes from run to run (the two timer slots' phases); the
    # reference run is one of the usual ones, as the port's. (At 4.5 s the
    # original may still show the black display it builds the lab on: left
    # out.)
    "hole-lab": {**hole((280, 156)), "shots": [x for x in every(2.5, 12, 0.5) if x[1] != "t045"]},
    "hole-play": hole((260, 148)),
    # Room 21's targets step by [FFE] (room ticks from the program's start)
    # and their creation numbers: the original's [FFE] at room 21 changes
    # by a few ticks from run to run (874, 876, 878-881 in three), so the
    # port's is set to the reference run's (874: 712 at room 1, which takes
    # 162); its frames every 10 ms, as consecutive targets step a tick
    # apart. Edison comes in from the left in that run. (At 3.5 s the
    # original may still be building room 21 on a black display, for longer
    # the busier the machine: that shot left out.)
    "hole-level1": {**hole((332, 148)), "shots": [x for x in every(2.5, 12, 0.5) if x[1] != "t035"],
                    "dense": [n for _, n in every(2.5, 12, 0.5) if n != "t035"],
                    "env": {"SCI_ROOMTICKS": "712", "SCI_RUNNERSIDE": "left"}},
    **{f"lesson-{n}": lesson(n) for n in range(6, 11)},
    "game-over": game_over(),
    "game-won": game_won(),
    "game-won-shift": game_won_shift(),
    "lesson-keys": lesson_keys(),
}
for s in SCENARIOS.values():
    s.setdefault("masks", COLUMNS)
