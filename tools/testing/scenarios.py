"""The scenarios scenario.py plays in the port and the original.

Each starts in a table room (the original gets there with S and the room's
two digits, origrun.py) and is a timeline in seconds after the room is up:
presses, drags, moves and typing, and the moments to take shots. Masks are
boxes (x0, y0, x1, y1) left out of the comparison: what is random in the
original too. Other differences the known counts allow for: things caught
at another moment of their animation (the lab's professor, room 21's
objects), Edison walking to put up an OUT OF ORDER sign, and a box's
backdrop (one of ten by rand, f24's dialogs: lesson 9's end).

"alone": recorded one at a time even with scenario.py --orig-jobs: beside
other copies of the original these failed their known counts (2026-10-10:
hole-level1's targets, lessons 6 and 7's ends, field-drag's puddle, the
originals behind the script's clock), and the story and the lab lost their
narration and stayed black.
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
VKEYS = {"shift": 16, "esc": 27, "left": 37, "up": 38, "right": 39, "down": 40}


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
    if n in (6, 7):
        s["alone"] = True
    if n <= 8:
        # The professor's mouth in the classroom: after each closed mouth
        # the next is one of two by rand() (dialog.cpp, f24's), and the
        # original's rand calls change from run to run (lesson 7's s3 a
        # laugh in two runs, the grin in the reference before).
        s["masks"] = COLUMNS + [(522, 158, 562, 188)]
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


def arrive(room, came_from):
    """Room ROOM entered from CAME_FROM by the S key (event 9: the player's
    +90 the room left), its arrival's doors (f45_0722, f47_044a,
    f48_0f14, f45_0955); shots till the ball is at rest."""
    return {"room": came_from, "events": [typed(1.0, f"S{room:02d}")], "shots": every(4.0, 10, 0.5), "length": 11}


def greeting_flash(room):
    """Rooms 23 and 50 from room 1 (S): f24_1ee3 with no box (screen 2's
    play area filled with colour 2, f31_0385): seen, or all redrawn first?
    Shots every 0.2 s round the arrival, the port's frames every 10 ms.
    (Till 1.5 s the original may still be building the room on a black
    display, as in hole-level1: those left out.)"""
    shots = every(1.6, 4.0, 0.2, "a")
    return {"room": 1, "events": [typed(1.0, f"S{room:02d}")], "shots": shots, "length": 6,
            "dense": [n for _, n in shots]}


def bonus_door(room, aim, before, power=None):
    """A room's bonus door (C46h, the hole's method 8: f43_0bce in 14,
    f59_07df in 92): the bonus, a ball, and the completion's shares from
    3 shots on zeroed (f27_0859): the "Bonus Points" box lists them all
    (1-4 shots), then the count. BEFORE: the clicks first (a greeting's
    OK, room 14's two switches)."""
    events = list(before)
    t = (events[-1][0] if events else 0.0) + 1.0
    if power:
        events.append(drag(t, 445, 325, 445, power, 2.5))
        t += 3.0
    events += shoot(t, *aim)
    shots = every(t + 4.0, t + 11.0, 0.5, "b")
    events.append(click(t + 11.5, 320, 200))
    shots += [(t + 13.0, "count"), (t + 16.0, "count2"), (t + 19.0, "next")]
    # (The ball to give, over the list: a rolling frame by rand, f24_175a.)
    return {"room": room, "events": events, "shots": shots, "length": t + 20,
            "masks": COLUMNS + [(206, 74, 236, 100)]}


def from_title(name):
    """From the title (room 0: the original's WMAINLAB.EXE, nothing
    skipped): the title clicked away, the story's pages (colour 10 set to
    3F3F3F before each picture's palette, f14_00a3, on a true-colour
    desktop), NAME typed in the lab against a high-score file holding
    "Ed.s" (its dots read as spaces: f39_1c22; the returning player gets
    its look, green, f19_0a59), DONE, lesson 5 ended by Escape, room 1's
    hole to the high scores (502: the player's row, spaces as "_",
    f19_0f1a; the records' dots as spaces). Each wait the games share
    ends with the same input, so they meet again after it."""
    events = [click(8.0, 320, 200), typed(85.0, name + "|"), click(97.0, 400, 325), key(112.0, "esc"),
              *shoot(117.0, 120, 236)]
    shots = [(5.0, "title"), (22.0, "story1"), (40.0, "story2"), (60.0, "story3"), (84.0, "name"),
             (95.0, "look"), (110.0, "lesson"), (116.0, "room1"), (126.0, "hs"), (128.0, "hs2")]
    # (Alone: beside another copy of the original, the story's narration
    # didn't play and the lab stayed black.)
    return {"room": 0, "events": events, "shots": shots, "length": 130, "alone": True,
            "files": {"WSCIENCE.HS": "HSFILE  Ed.s 9000 1 1 3 6 2 5 \r\n"}}


def more_outside():
    """Lesson 6 (room 61's hole) with presses beside its MORE (528, 370,
    88, 24): left of it and above it, which go nowhere (the lesson's +08
    takes only its rectangle); then MORE itself."""
    s = lesson(6)
    events = [drag(3.0, 445, 325, 445, 250, 2.5), *shoot(6.0, 306, 176), click(13.0, 520, 380),
              click(14.0, 572, 365), click(15.0, 550, 380)]
    shots = [(12.0, "s1"), (13.5, "left"), (14.5, "above"), (17.0, "more")]
    # (Recorded side by side: its shots are of still bubbles.)
    return {**s, "events": events, "shots": shots, "length": 19, "dense": [], "alone": False}


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
    # (At 3.5 s the original may still be building the next room on a black
    # display, as in hole-level1: left out.)
    "hole-play": {**hole((260, 148)), "shots": [x for x in every(2.5, 12, 0.5) if x[1] != "t035"],
                  "dense": ["t025", "t030"]},
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
                    "env": {"SCI_ROOMTICKS": "712", "SCI_RUNNERSIDE": "left"}, "alone": True},
    **{f"lesson-{n}": lesson(n) for n in range(6, 11)},
    "game-over": game_over(),
    "game-won": game_won(),
    "game-won-shift": game_won_shift(),
    "lesson-keys": lesson_keys(),
    # Room 5's hot field dragged by its core (f08_07c6: the puddle's middle):
    # the press takes the mouse, no aim (no target ring at 2.5 s in either),
    # then a shot through where it was. The puddle's size and the fountains'
    # spray come from rand (the original's differ from run to run): the
    # known counts are theirs (about 800-2400 pixels).
    "field-drag": {"room": 5, "events": [drag(2.0, 422, 197, 500, 205, 1.0), *shoot(5.0, 422, 197)],
                   "shots": every(1.5, 9, 0.25), "masks": COLUMNS, "length": 10, "alone": True},
    # The POWER lever pressed, then the arrow keys (f30_32a9: they'd move it
    # if the panel got them; f31_1d70 sends keys to the room while there is one).
    "slider-keys": {"room": 1, "events": [click(2.0, 445, 325), key(2.5, "up"), key(3.0, "up"),
                                          key(3.5, "right"), key(4.0, "down")],
                    "shots": every(1.5, 5, 0.5), "masks": COLUMNS, "length": 6},
    # The scan's changes (TODO.md): the doors shut on arrival from the room
    # behind them, room 23's from 44 (the ball out of the hole to 44,
    # mode 1: 363,290 in the port), the greetings, the bonus doors, a name
    # with a space and one with a dot, MORE's area.
    **{f"arrive-{r}-from-{f}": arrive(r, f) for r, f in ((22, 40), (40, 22), (33, 52), (23, 44))},
    "flash-23": greeting_flash(23),
    "flash-50": greeting_flash(50),
    # Room 67's two greetings (f57's builder), each its OK. (Their pictures
    # are rand's, f24_193e, new in each recording of the original: the
    # reference run's here, found by trying SCI_DIALOGPIC 0-4.)
    "greet-67": {"room": 67, "events": [click(1.5, 290, 225), click(3.5, 290, 225)],
                 "shots": [(0.5, "box1"), (2.5, "box2"), (5.0, "room"), (6.0, "room2")], "length": 7,
                 "env": {"SCI_DIALOGPIC": "2"}},
    # Room 14: its switches 3 and 6 (the levers at 287,42 and 503,68)
    # open the way to the bonus door. Room 92: its greeting's OK, power 9.
    # (Room 10's greeting after: the reference run's picture, as greet-67's.)
    "bonus-14": {**bonus_door(14, (176, 230), [click(1.0, 287, 42), click(1.7, 503, 68)]),
                 "env": {"SCI_DIALOGPIC": "2"}},
    "bonus-92": bonus_door(92, (328, 162), [click(1.0, 290, 225)], power=322),
    "lab-space": from_title("Ed s"),
    "lab-dot": from_title("Ed.s"),
    "more-outside": more_outside(),
}
for s in SCENARIOS.values():
    s.setdefault("masks", COLUMNS)
