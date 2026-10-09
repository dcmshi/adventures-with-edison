# Runs the ORIGINAL game under winevdm (otvdm) as a visual reference, safely.
#
#   otvdm.ps1 start [EXE]        start the game (default EDISON.EXE); a run
#                                left over from before is stopped first
#   otvdm.ps1 play EXE SCRIPT [DIR]  start, run SCRIPT, then stop, whatever
#                                happens (the way test scripts should use it)
#   otvdm.ps1 shot OUT.png       screenshot the game window only (no focus change)
#   otvdm.ps1 dialogs            list open dialogs (error boxes) of the game
#   otvdm.ps1 guard              (started by start) while the game runs, keep
#                                re-enabling the windows its dialogs disable
#   otvdm.ps1 stop               close the game safely
#   otvdm.ps1 unlock             re-enable windows left disabled (the game
#                                must be closed): after a crash or a reboot
#                                mid-run, terminals can stay uninteractable
#                                till this (or stop) runs
#   otvdm.ps1 unlock             re-enable windows the game left disabled
#                                (after a crash or a kill; stop does this too)
#   otvdm.ps1 click X Y          left click at game coordinates (rclick: right)
#   otvdm.ps1 down X Y / up X Y  press / release the left button there (to hold it)
#   otvdm.ps1 move X Y           move the mouse there (button up)
#   otvdm.ps1 type TEXT          type TEXT ('|' is Enter; capitals with Shift held,
#                                its scan code too: the arcade's S key, a room)
#   otvdm.ps1 run's keydown VK / keyup VK  press / release a key by its
#                                virtual-key code (with its scan code: 27 Esc)
#   otvdm.ps1 volume N          the game's volume in the Windows mixer (0-100;
#                                scripts set EDISON_VOLUME's, default 0: muted)
#   otvdm.ps1 run SCRIPT [DIR]   run a script of the commands above, one a line,
#                                plus `wait SECONDS`; shots go to DIR; # comments
#
# Input is posted to the game's window, so the real cursor isn't moved (but
# a game that confines the cursor, like Rock and Bach's video makers, pulls
# it into the window). Keys carry their scan codes: the games read those
# from lParam, not wParam.
#
# Why the care: 16-bit programs often open message boxes without an owner,
# and otvdm then makes the window in front (for example a terminal) the
# owner, which Windows disables until the dialog closes. Force-killing the
# game while such a dialog is open leaves that window disabled for good: it
# chimes on click and ignores input. EnableWindow(hwnd, TRUE) fixes it.
# So `stop` closes dialogs first, asks the game to close, only then kills
# it, and finally re-enables any disabled top-level window it finds.
#
# `start` needs winevdm's otvdmw.exe ($env:OTVDM, or on the PATH) and a
# folder with the game files and the WinG DLLs ($env:EDISON_RUN).
param([Parameter(Mandatory)][string]$Command, [string]$Arg, [string]$Arg2, [string]$Arg3)

$otvdm = if ($env:OTVDM) { $env:OTVDM } else { (Get-Command otvdmw.exe -ErrorAction SilentlyContinue).Source }
$runDir = $env:EDISON_RUN

Add-Type -AssemblyName System.Drawing
Add-Type @"
using System; using System.Text; using System.Collections.Generic; using System.Runtime.InteropServices;
public struct RECT { public int L, T, R, B; }
public static class W {
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc f, IntPtr p);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern int GetClassName(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern int GetWindowText(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern bool IsWindowEnabled(IntPtr h);
  [DllImport("user32.dll")] public static extern bool EnableWindow(IntPtr h, bool on);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern uint MapVirtualKey(uint c, uint t);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int c);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int cx, int cy, uint f);
  [DllImport("kernel32.dll")] public static extern uint SetThreadExecutionState(uint f);
  public static List<IntPtr> All() { var l = new List<IntPtr>(); EnumWindows((h, p) => { l.Add(h); return true; }, IntPtr.Zero); return l; }
  public static string Cls(IntPtr h) { var s = new StringBuilder(256); GetClassName(h, s, 256); return s.ToString(); }
  public static string Text(IntPtr h) { var s = new StringBuilder(256); GetWindowText(h, s, 256); return s.ToString(); }
  public static uint Pid(IntPtr h) { uint p; GetWindowThreadProcessId(h, out p); return p; }
}
"@

function Game { Get-Process otvdmw -ErrorAction SilentlyContinue }

# The game's volume in the Windows mixer (Core Audio: its process's audio
# sessions on the default output). A session only exists once the game has
# played a sound; Windows then keeps the level for otvdmw.exe across runs.
Add-Type @"
using System; using System.Runtime.InteropServices;
[ComImport, Guid("BCDE0395-E52F-467C-8E3D-C4579291692E")] class MMDeviceEnumeratorCom {}
[Guid("A95664D2-9614-4F35-A746-DE8DB63617E6"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
interface IMMDeviceEnumerator {
  int NotImpl1();
  [PreserveSig] int GetDefaultAudioEndpoint(int flow, int role, out IMMDevice dev);
}
[Guid("D666063F-1587-4E43-81F1-B948E807363F"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
interface IMMDevice {
  [PreserveSig] int Activate(ref Guid iid, int ctx, IntPtr p, [MarshalAs(UnmanagedType.IUnknown)] out object o);
}
[Guid("77AA99A0-1BD6-484F-8BC7-2C654C9A9B6F"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
interface IAudioSessionManager2 {
  int NotImpl1(); int NotImpl2();
  [PreserveSig] int GetSessionEnumerator(out IAudioSessionEnumerator e);
}
[Guid("E2F5BB11-0570-40CA-ACDD-3AA01277DEE8"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
interface IAudioSessionEnumerator {
  [PreserveSig] int GetCount(out int n);
  [PreserveSig] int GetSession(int i, out IAudioSessionControl2 s);
}
[Guid("BFB7FF88-7239-4FC9-8FA2-07C950BE9C6D"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
interface IAudioSessionControl2 {
  int N1(); int N2(); int N3(); int N4(); int N5(); int N6(); int N7(); int N8(); int N9();
  int N10(); int N11();
  [PreserveSig] int GetProcessId(out uint pid);
}
[Guid("87CE5498-68D6-44E5-9215-6DA47EF883D8"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
interface ISimpleAudioVolume {
  [PreserveSig] int SetMasterVolume(float level, ref Guid ctx);
  [PreserveSig] int GetMasterVolume(out float level);
}
public static class Mixer {
  // Sets the volume (0-1) of every session of the processes given; the count set.
  public static int Set(uint[] pids, float level) {
    var en = (IMMDeviceEnumerator)new MMDeviceEnumeratorCom();
    IMMDevice dev; if (en.GetDefaultAudioEndpoint(0, 1, out dev) != 0) return 0;
    var iid = typeof(IAudioSessionManager2).GUID; object o;
    if (dev.Activate(ref iid, 23, IntPtr.Zero, out o) != 0) return 0;
    IAudioSessionEnumerator se; if (((IAudioSessionManager2)o).GetSessionEnumerator(out se) != 0) return 0;
    int n, set = 0; se.GetCount(out n);
    for (int i = 0; i < n; i++) {
      IAudioSessionControl2 s; if (se.GetSession(i, out s) != 0) continue;
      uint pid; s.GetProcessId(out pid);
      if (Array.IndexOf(pids, pid) < 0) continue;
      var g = Guid.Empty; ((ISimpleAudioVolume)s).SetMasterVolume(level, ref g); set++;
    }
    return set;
  }
}
"@

# EDISON_VOLUME (0-100, default 0: muted, as tests needn't hear it): the
# game's level in the mixer (scripts apply it once its session appears).
$volume = if ($env:EDISON_VOLUME) { [int]$env:EDISON_VOLUME } else { 0 }
$volumeSet = $false
function SetVolume($percent) {
    $ids = [uint32[]]@(Game | ForEach-Object { [uint32]$_.Id })
    if ($ids.Count -eq 0) { return 0 }
    try { [Mixer]::Set($ids, [Math]::Max(0, [Math]::Min(100, $percent)) / 100.0) } catch { 0 }
}
function GameWindows { $ids = @(Game | ForEach-Object Id); [W]::All() | Where-Object { $ids -contains [W]::Pid($_) -and [W]::IsWindowVisible($_) } }
function Dialogs { GameWindows | Where-Object { [W]::Cls($_) -eq "#32770" } }

function Enable-DisabledWindows {
    $gameIds = @(Game | ForEach-Object Id)
    foreach ($h in [W]::All()) {
        # conpty's PseudoConsoleWindow, UWP frames and DWM's listener are disabled normally
        if ([W]::IsWindowVisible($h) -and -not [W]::IsWindowEnabled($h) -and $gameIds -notcontains [W]::Pid($h) -and
            @("#32770", "PseudoConsoleWindow", "ApplicationFrameWindow", "DummyDWMListenerWindow") -notcontains [W]::Cls($h)) {
            [W]::EnableWindow($h, $true) | Out-Null
            Write-Output "re-enabled '$([W]::Text($h))' ($([W]::Cls($h)))"
        }
    }
}

# The Artech library opens a black popup the size of the desktop behind the
# game (class WinArtechBackDropWindow); it isn't needed for references. So
# `start` hides it and keeps the game's own window on top (so shots, which
# copy its rectangle from the screen, aren't covered). OTVDM_FULLSCREEN=1
# keeps the backdrop.
function Windowed {
    for ($i = 0; $i -lt 40; $i++) {
        $back = @(GameWindows | Where-Object { [W]::Cls($_) -like "*BackDrop*" })
        $main = MainWindow
        if ($main -and $back.Count) {
            foreach ($b in $back) { [W]::ShowWindow($b, 0) | Out-Null }               # SW_HIDE
            [W]::SetWindowPos($main, [IntPtr](-1), 0, 0, 0, 0, 0x13) | Out-Null       # HWND_TOPMOST, no move/size/activate
            Write-Output "windowed (backdrop hidden, game on top)"
            return
        }
        Start-Sleep -Milliseconds 250
    }
    Write-Output "no backdrop window found (left as it is)"
}

function MainWindow { GameWindows | Where-Object { [W]::Cls($_) -ne "#32770" -and [W]::Cls($_) -notlike "*BackDrop*" } | Select-Object -First 1 }
function Post($h, $m, $w, $l) { [W]::PostMessage($h, $m, [IntPtr]$w, [IntPtr]$l) | Out-Null }

function Click($x, $y, $right) {
    $h = MainWindow
    if (-not $h) { Write-Output "no game window"; return }
    $xy = ([int]$y -shl 16) -bor ([int]$x -band 0xFFFF)
    $down, $up, $key = if ($right) { 0x204, 0x205, 2 } else { 0x201, 0x202, 1 }
    Post $h 0x200 0 $xy; Start-Sleep -Milliseconds 50
    Post $h $down $key $xy; Start-Sleep -Milliseconds 80
    Post $h $up 0 $xy
}

function Button($x, $y, $down) {
    $h = MainWindow
    if (-not $h) { Write-Output "no game window"; return }
    $xy = ([int]$y -shl 16) -bor ([int]$x -band 0xFFFF)
    Post $h 0x200 ($(if ($down) { 1 } else { 0 })) $xy; Start-Sleep -Milliseconds 30
    if ($down) { Post $h 0x201 1 $xy } else { Post $h 0x202 0 $xy }
}

function TypeText($text) {
    $h = MainWindow
    if (-not $h) { Write-Output "no game window"; return }
    foreach ($c in $text.ToCharArray()) {
        $code = if ($c -eq '|') { 13 } else { [int]$c }
        $vk = [int][char]::ToUpper([char]$code)
        $sc = [int][W]::MapVirtualKey([uint32]$vk, 0)
        $shift = $c -cmatch '[A-Z]'
        if ($shift) { Post $h 0x100 0x10 (1 -bor (0x2A -shl 16)) }  # Shift down (left, scan 2Ah)
        Post $h 0x100 $vk (1 -bor ($sc -shl 16))           # WM_KEYDOWN
        Post $h 0x102 $code (1 -bor ($sc -shl 16))         # WM_CHAR
        Start-Sleep -Milliseconds 40
        Post $h 0x101 $vk ([int64]0xC0000001 -bor ($sc -shl 16))  # WM_KEYUP
        if ($shift) { Post $h 0x101 0x10 ([int64]0xC0000001 -bor (0x2A -shl 16)) }
        Start-Sleep -Milliseconds 60
    }
}

function Key($vk, $down) {
    $h = MainWindow
    if (-not $h) { Write-Output "no game window"; return }
    $sc = [int][W]::MapVirtualKey([uint32]$vk, 0)
    if ($down) { Post $h 0x100 $vk (1 -bor ($sc -shl 16)) }                  # WM_KEYDOWN
    else { Post $h 0x101 $vk ([int64]0xC0000001 -bor ($sc -shl 16)) }        # WM_KEYUP
}

function Shot($out) {
    $h = MainWindow
    if (-not $h) { Write-Output "no game window"; return }
    $r = New-Object RECT; [W]::GetWindowRect($h, [ref]$r) | Out-Null
    $bmp = New-Object System.Drawing.Bitmap ($r.R - $r.L), ($r.B - $r.T)
    [System.Drawing.Graphics]::FromImage($bmp).CopyFromScreen($r.L, $r.T, 0, 0, $bmp.Size)
    $bmp.Save($out, [System.Drawing.Imaging.ImageFormat]::Png)
    $d = @(Dialogs).Count
    Write-Output "saved $out ($($r.R - $r.L)x$($r.B - $r.T))$(if ($d) { ", $d dialog(s) open" })"
}

function StopGame {
    if (-not (Game)) { return "nothing running" }
    foreach ($d in @(Dialogs)) { [W]::SendMessage($d, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null }  # WM_CLOSE
    Start-Sleep -Milliseconds 500
    Game | ForEach-Object { $_.CloseMainWindow() | Out-Null }
    for ($i = 0; $i -lt 10 -and (Game); $i++) {
        foreach ($d in @(Dialogs)) { [W]::SendMessage($d, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null }
        Start-Sleep -Milliseconds 500
    }
    $how = if (Game) { Game | Stop-Process -Force; "had to force-kill" } else { "closed" }
    Start-Sleep -Milliseconds 300
    Enable-DisabledWindows
    $how
}

function StartGame($exe) {
    if (-not $otvdm) { throw "set OTVDM to winevdm's otvdmw.exe (or put it on the PATH)" }
    if (-not $runDir) { throw "set EDISON_RUN to the folder with the game files" }
    # A run left over from an interrupted test (or a crash) first: two at
    # once confuse the shots, the input and memwatch.
    if (Game) { Write-Output "a run was left over: $(StopGame)" }
    Start-Process -FilePath $otvdm -ArgumentList $exe -WorkingDirectory $runDir
    Write-Output "started $exe"
    # The guard: while the game runs, any window its dialogs disabled (the
    # terminal in front when a greeting box opened) is enabled again, so a
    # run waiting on a dialog never locks the user out.
    Start-Process -FilePath "pwsh" -ArgumentList @("-NoProfile", "-File", $PSCommandPath, "guard") -WindowStyle Hidden
    if ($env:OTVDM_FULLSCREEN -ne "1") { Windowed }
}

function RunScript($script, $dir) {
    if (-not $dir) { $dir = "." }
    # The display kept on while the script runs: asleep, nothing is drawn
    # and every shot is the same stale picture (the script's clicks are
    # posted messages, not input, and don't keep it awake).
    # ES_CONTINUOUS | ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED, for this
    # process's life.
    [W]::SetThreadExecutionState(0x80000003u) | Out-Null
    # Waits add up from the script's start, so the time the steps take
    # (a shot, a click) doesn't push everything after them later.
    $clock = [Diagnostics.Stopwatch]::StartNew(); $due = 0.0
    foreach ($line in Get-Content $script) {
        $w = ($line -replace '#.*$', '').Trim() -split '\s+', 2
        if (-not $w[0]) { continue }
        $a = if ($w.Count -gt 1) { $w[1] } else { "" }
        # The volume, once the game's audio session is there.
        if (-not $script:volumeSet -and (SetVolume $volume) -gt 0) { $script:volumeSet = $true; Write-Output "volume $volume%" }
        switch ($w[0]) {
            "wait" { $due += [double]$a * 1000; $ms = $due - $clock.Elapsed.TotalMilliseconds; if ($ms -gt 0) { Start-Sleep -Milliseconds $ms } }
            "click" { $p = $a -split '\s+'; Click $p[0] $p[1] $false }
            "rclick" { $p = $a -split '\s+'; Click $p[0] $p[1] $true }
            "move" { $p = $a -split '\s+'; $h = MainWindow; if ($h) { Post $h 0x200 0 (([int]$p[1] -shl 16) -bor ([int]$p[0] -band 0xFFFF)) } }
            "down" { $p = $a -split '\s+'; Button $p[0] $p[1] $true }
            "up" { $p = $a -split '\s+'; Button $p[0] $p[1] $false }
            "type" { TypeText $a }
            "keydown" { Key ([int]$a) $true }
            "keyup" { Key ([int]$a) $false }
            "shot" { Shot (Join-Path $dir $a) | Out-Null }
            default { Write-Output "unknown step: $line" }
        }
    }
    Write-Output "ran $script"
}

switch ($Command) {
    "start" { StartGame $(if ($Arg) { $Arg } else { "EDISON.EXE" }) }
    "play" {
        # Start, the script, stop: the stop runs even when a step fails or
        # the script is interrupted, so the next start is clean.
        try { StartGame $Arg; RunScript $Arg2 $Arg3 } finally { StopGame }
    }
    "shot" { Shot $Arg }
    "click" { Click $Arg $Arg2 $false }
    "rclick" { Click $Arg $Arg2 $true }
    "move" { $h = MainWindow; if ($h) { Post $h 0x200 0 (([int]$Arg2 -shl 16) -bor ([int]$Arg -band 0xFFFF)) } }
    "down" { Button $Arg $Arg2 $true }
    "up" { Button $Arg $Arg2 $false }
    "type" { TypeText $Arg }
    "run" { RunScript $Arg $Arg2 }
    "volume" { Write-Output "sessions set: $(SetVolume ([int]$Arg))" }
    "unlock" {
        if (Game) { Write-Output "the game is still running: use stop"; exit 1 }
        $out = @(Enable-DisabledWindows)
        if ($out) { $out } else { Write-Output "no disabled windows" }
    }
    "guard" {
        # (Started by start: polls till the game has gone.)
        for ($i = 0; $i -lt 50 -and -not (Game); $i++) { Start-Sleep -Milliseconds 200 }
        while (Game) { Enable-DisabledWindows | Out-Null; Start-Sleep -Milliseconds 300 }
        Enable-DisabledWindows | Out-Null
    }
    "dialogs" { Dialogs | ForEach-Object { Write-Output "'$([W]::Text($_))'" } }
    "stop" { StopGame }
    default { Write-Output "unknown command $Command"; exit 2 }
}
