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
    return {"room": 1, "events": shoot(1.0, *aim), "shots": every(2.5, 12, 0.5), "length": 13}


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
    return {"room": 61, "events": events, "shots": shots, "length": t + 3}


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
    return {"room": 65, "events": events, "shots": [(11.1, "bonus"), (15.2, "lesson"), (25.5, "hs")],
            "length": 27}


SCENARIOS = {
    "hole-lab": hole((280, 156)),
    "hole-play": hole((260, 148)),
    "hole-level1": hole((332, 148)),
    **{f"lesson-{n}": lesson(n) for n in range(6, 11)},
    "game-over": game_over(),
    "game-won": game_won(),
}
for s in SCENARIOS.values():
    s.setdefault("masks", COLUMNS)
