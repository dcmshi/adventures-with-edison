"""Rock and Bach against the original: a timeline of clicks, holds and typing
in an activity, played in the port (--level N) and in the original under
winevdm (WINMSKIP.EXE, tools/reference/winmain_skip.py: straight into the
activity, with Edison's look from ed.yyy), each of the original's shots
against the port's closest frame near its time.

  python tools/testing/rbcompare.py [NAME ...] [--port-only | --compare-only] [--list]

Shots and diffs (the pixels that differ in magenta) go to
build/scratch/rbcompare/NAME/. Needs EDISON_RUN (with FRED.VID), OTVDM,
and the CD mounted (its \\RB
folder: the instruments' and Sound FX's WAVs; on Windows,
Mount-DiskImage on the CD image). Scenarios with music run the port in
real time (the driver's events follow the sound, not the virtual clock),
so their frames near a scene change can differ by a frame or two. What
always differs: colour 255 (white in the port, the backdrop's colour in
the original under winevdm) and a pressed button's bevel corners (the
original's window refresh leaves them out); see docs/ROCKBACH.md.
"""
import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

from PIL import Image, ImageChops

from testlib import REFERENCE, ROOT, SCRATCH, edison

OUT = SCRATCH / "rbcompare"
START = 5  # seconds from WINMSKIP.EXE's start to the activity's time 0

# The activities by hallway result (docs/ROCKBACH.md).
JUKEBOX, DRUMS, LIBRARY, HARMONY, INSTRUMENTS, SOUNDFX, STUDIO = 2, 3, 4, 6, 7, 8, 9


def click(t, x, y):
    return (t, "click", x, y)


def hold(t, x, y, seconds=0.08):
    return (t, "hold", x, y, seconds)


def shots(*pairs):
    return list(pairs)


# Each: the activity, the events and the shots (seconds from the
# activity's start), "realtime" for the music's events.
SCENARIOS = {
    "instruments": dict(level=INSTRUMENTS, events=[
        click(1, 120, 250), click(4, 215, 150), click(6, 50, 40), click(9, 425, 152), click(12, 400, 287),
        click(15, 515, 160), click(18, 215, 150), click(20, 580, 40)],
        shots=shots((0.5, "start"), (3, "flute"), (5, "page2"), (8, "violin"), (10, "note"), (13, "keys"),
                    (14.5, "conductor"), (17, "piano"), (19, "piano2"), (23, "drums"))),
    "jukebox": dict(level=JUKEBOX, realtime=True, events=[
        click(1, 54, 114), click(2, 126, 168), click(3, 514, 60), click(4, 586, 222), click(5, 75, 322),
        click(6, 495, 277), click(7, 533, 303), click(8, 215, 240), click(11, 420, 20), click(12, 420, 20),
        click(14, 215, 20), click(16, 415, 240)],
        shots=shots((0.5, "start"), (4.5, "members"), (6.5, "song"), (7.5, "lights"), (9, "go"), (13, "faster"),
                    (15, "slower"), (17, "stop"))),
    "drums": dict(level=DRUMS, realtime=True, events=[
        click(1, 126, 114), click(2.5, 586, 60), click(4, 57, 300), click(6, 157, 364), click(8, 57, 364),
        click(10, 157, 300), click(12, 260, 290), click(13, 420, 328), click(14, 210, 240), click(17, 373, 240)],
        shots=shots((0.5, "start"), (2, "kit"), (3.5, "pattern"), (5, "mode0"), (7, "mode3"), (9, "mode2"),
                    (11, "mode1"), (12.5, "mouth"), (13.5, "drum"), (15, "go"), (18, "stop"))),
    "library": dict(level=LIBRARY, realtime=True, events=[
        click(1, 126, 168), click(3, 310, 240), click(5.5, 320, 200), click(8, 550, 90), click(10, 478, 346),
        click(11.5, 527, 378), click(13, 420, 17), click(14.5, 213, 17), click(16, 360, 355)],
        shots=shots((0.5, "start"), (2.5, "composer"), (4.5, "info"), (7, "info2"), (9.5, "piece2"),
                    (11, "inst1"), (12.5, "inst4"), (14, "faster"), (15.5, "slower"), (18, "ink"))),
    "harmony": dict(level=HARMONY, realtime=True, events=[
        click(1, 90, 115), click(2.5, 507, 173), click(4, 110, 345), click(5.5, 215, 240), click(8, 90, 296),
        click(10, 527, 295), click(12, 540, 360), click(14, 415, 240), click(15.5, 36, 57)],
        shots=shots((0.5, "start"), (2, "key"), (3.5, "chord"), (5, "style"), (7, "go"), (9, "riff"),
                    (11, "light"), (13, "reset"), (15, "stop"), (16.5, "keyC"))),
    # LOAD from the CD; the list's arrows held (the original steps a row
    # each poll, the port every 0.1 s: these two differ).
    "soundfx-list": dict(level=SOUNDFX, events=[
        click(1, 90, 206), click(3, 195, 140), hold(6, 512, 320, 0.3), hold(8, 512, 320, 1.0)],
        shots=shots((2, "chooser"), (5, "list"), (7, "held"), (10, "held-more"))),
    # FRED.VID loaded (it plays), then EDIT held: the press that ends the
    # video presses EDIT too (f34_0fb6 takes a held button as a press).
    "studio-edit": dict(level=STUDIO, realtime=True, events=[
        click(1, 60, 37), click(3, 300, 135), click(5, 190, 175), hold(16, 60, 137), click(19, 160, 155)],
        shots=shots((0.5, "front"), (2.5, "load"), (4.5, "list"), (18, "what"), (22, "camera"))),
}


def run_folder():
    if not os.environ.get("EDISON_RUN"):
        sys.exit("set EDISON_RUN to the folder with the game files")
    return Path(os.environ["EDISON_RUN"])


def play_orig(name, s):
    """WINMSKIP.EXE written for the activity, then otvdm.ps1 runs it (the
    activity is up in about 4 s) and the timeline."""
    d = OUT / name / "orig"
    shutil.rmtree(d, ignore_errors=True)
    d.mkdir(parents=True)
    subprocess.run([sys.executable, str(REFERENCE / "winmain_skip.py"), str(s["level"])], check=True,
                   stdout=subprocess.DEVNULL)
    lines = [f"wait {START}"]
    timeline = [(e[0], e) for e in s["events"]] + [(t, ("shot", n)) for t, n in s["shots"]]
    now = 0.0
    for at, e in sorted(timeline, key=lambda p: p[0]):
        if at > now:
            lines.append(f"wait {at - now:.2f}")
            now = at
        if e[0] == "shot":
            lines.append(f"shot {e[1]}.png")
        elif e[1] == "click":
            lines.append(f"click {e[2]} {e[3]}")
        elif e[1] == "hold":
            lines += [f"down {e[2]} {e[3]}", f"wait {e[4]}", f"up {e[2]} {e[3]}"]
            now += e[4]
    (d / "script.txt").write_text("\n".join(lines) + "\n")
    subprocess.run(["pwsh", "-NoProfile", "-File", str(REFERENCE / "otvdm.ps1"), "play", "WINMSKIP.EXE",
                    str(d / "script.txt"), str(d)], check=True)


def play_port(name, s, lead):
    d = OUT / name / "port"
    shutil.rmtree(d, ignore_errors=True)
    exe = edison(d)
    save = d / "save"
    save.mkdir()
    # The original's player and videos, so both start the same.
    for f in run_folder().iterdir():
        if f.suffix.lower() in (".vid", ".yyy"):
            shutil.copy2(f, save / f.name.lower())
    args = []
    for e in s["events"]:
        ms = str(int((e[0] + lead) * 1000))
        if e[1] == "click":
            args += ["--click", ms, str(e[2]), str(e[3])]
        elif e[1] == "hold":
            args += ["--drag", ms, str(e[2]), str(e[3]), str(e[2]), str(e[3]), str(int(e[4] * 1000))]
    end = max([t for t, _ in s["shots"]] + [e[0] for e in s["events"]]) + lead + 2
    clock = [] if s.get("realtime") else ["--virtual-clock"]
    with open(d / "run.log", "w") as log:
        subprocess.run([str(exe), str(ROOT / "original" / "cd" / "DSK3"), "--game", "rockbach", "--level",
                        str(s["level"]), "--hidden", *clock, "--save", str(save), "--capture", str(d), "100",
                        *args, "--quit-after", str(int(end * 1000))],
                       cwd=d, stdout=log, stderr=subprocess.STDOUT, timeout=end + 60)


def compare(name, s, lead, window):
    """Each original shot against the port's frames within the window of
    its time: the fewest pixels that differ."""
    d = OUT / name
    frames = sorted((int(p.stem), p) for p in (d / "port").glob("*.bmp"))
    for t, n in s["shots"]:
        o = Image.open(d / "orig" / f"{n}.png").convert("RGB")
        best = None
        for ms, p in frames:
            if abs(ms / 1000 - (t + lead)) > window:
                continue
            f = Image.open(p).convert("RGB")
            mask = ImageChops.difference(f, o).convert("L").point(lambda v: 255 if v else 0)
            c = mask.histogram()[255]
            if best is None or c < best[0]:
                best = (c, ms, mask, f)
        if best is None:
            print(f"{name} {n}: no port frame near {t + lead:.1f} s")
            continue
        c, ms, mask, f = best
        where = ""
        if c:
            where = f" in {mask.getbbox()}"
            f.paste((255, 0, 255), mask=mask)
            f.save(d / f"diff-{n}.png")
        print(f"{name} {n}: {c} pixels (port {ms} ms){where}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("names", nargs="*")
    ap.add_argument("--port-only", action="store_true", help="play the port again, keep the original's shots")
    ap.add_argument("--compare-only", action="store_true", help="compare the last runs' frames")
    ap.add_argument("--list", action="store_true")
    ap.add_argument("--lead", type=float, default=1.0, help="seconds the port's activity starts before its first click")
    ap.add_argument("--window", type=float, default=1.5, help="seconds around each shot to search the port's frames")
    a = ap.parse_args()
    if a.list:
        for n, s in SCENARIOS.items():
            print(n, f"(activity {s['level']}, {len(s['shots'])} shots)")
        return
    for name in a.names or list(SCENARIOS):
        s = SCENARIOS[name]
        if not a.compare_only:
            play_port(name, s, a.lead)
            if not a.port_only:
                play_orig(name, s)
        compare(name, s, a.lead, a.window)


if __name__ == "__main__":
    main()
