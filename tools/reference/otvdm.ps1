# Runs the ORIGINAL game under winevdm (otvdm) as a visual reference, safely.
#
#   otvdm.ps1 start [EXE]        start the game (default EDISON.EXE)
#   otvdm.ps1 shot OUT.png       screenshot the game window only (no focus change)
#   otvdm.ps1 dialogs            list open dialogs (error boxes) of the game
#   otvdm.ps1 stop               close the game safely
#   otvdm.ps1 click X Y          left click at game coordinates (rclick: right)
#   otvdm.ps1 type TEXT          type TEXT ('|' is Enter; no Shift, so lower case)
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
# Needs winevdm in D:\tools\otvdm (set $env:OTVDM otherwise) and a folder
# with the game files and the WinG DLLs ($env:EDISON_RUN, default
# D:\tools\edison-run).
param([Parameter(Mandatory)][string]$Command, [string]$Arg, [string]$Arg2)

$otvdm = if ($env:OTVDM) { $env:OTVDM } else { "D:\tools\otvdm\otvdm-v0.9.0\otvdmw.exe" }
$runDir = if ($env:EDISON_RUN) { $env:EDISON_RUN } else { "D:\tools\edison-run" }

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
  public static List<IntPtr> All() { var l = new List<IntPtr>(); EnumWindows((h, p) => { l.Add(h); return true; }, IntPtr.Zero); return l; }
  public static string Cls(IntPtr h) { var s = new StringBuilder(256); GetClassName(h, s, 256); return s.ToString(); }
  public static string Text(IntPtr h) { var s = new StringBuilder(256); GetWindowText(h, s, 256); return s.ToString(); }
  public static uint Pid(IntPtr h) { uint p; GetWindowThreadProcessId(h, out p); return p; }
}
"@

function Game { Get-Process otvdmw -ErrorAction SilentlyContinue }
function GameWindows { $ids = @(Game | ForEach-Object Id); [W]::All() | Where-Object { $ids -contains [W]::Pid($_) -and [W]::IsWindowVisible($_) } }
function Dialogs { GameWindows | Where-Object { [W]::Cls($_) -eq "#32770" } }

function Enable-DisabledWindows {
    $gameIds = @(Game | ForEach-Object Id)
    foreach ($h in [W]::All()) {
        if ([W]::IsWindowVisible($h) -and -not [W]::IsWindowEnabled($h) -and [W]::Cls($h) -ne "#32770" -and $gameIds -notcontains [W]::Pid($h)) {
            [W]::EnableWindow($h, $true) | Out-Null
            Write-Output "re-enabled '$([W]::Text($h))' ($([W]::Cls($h)))"
        }
    }
}

function MainWindow { GameWindows | Where-Object { [W]::Cls($_) -ne "#32770" } | Select-Object -First 1 }
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

function TypeText($text) {
    $h = MainWindow
    if (-not $h) { Write-Output "no game window"; return }
    foreach ($c in $text.ToCharArray()) {
        $code = if ($c -eq '|') { 13 } else { [int]$c }
        $vk = [int][char]::ToUpper([char]$code)
        $sc = [int][W]::MapVirtualKey([uint32]$vk, 0)
        Post $h 0x100 $vk (1 -bor ($sc -shl 16))           # WM_KEYDOWN
        Post $h 0x102 $code (1 -bor ($sc -shl 16))         # WM_CHAR
        Start-Sleep -Milliseconds 40
        Post $h 0x101 $vk ([int64]0xC0000001 -bor ($sc -shl 16))  # WM_KEYUP
        Start-Sleep -Milliseconds 60
    }
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

switch ($Command) {
    "start" {
        $exe = if ($Arg) { $Arg } else { "EDISON.EXE" }
        Start-Process -FilePath $otvdm -ArgumentList $exe -WorkingDirectory $runDir
        Write-Output "started $exe"
    }
    "shot" { Shot $Arg }
    "click" { Click $Arg $Arg2 $false }
    "rclick" { Click $Arg $Arg2 $true }
    "type" { TypeText $Arg }
    "run" {
        $dir = if ($Arg2) { $Arg2 } else { "." }
        foreach ($line in Get-Content $Arg) {
            $w = ($line -replace '#.*$', '').Trim() -split '\s+', 2
            if (-not $w[0]) { continue }
            $a = if ($w.Count -gt 1) { $w[1] } else { "" }
            switch ($w[0]) {
                "wait" { Start-Sleep -Milliseconds ([double]$a * 1000) }
                "click" { $p = $a -split '\s+'; Click $p[0] $p[1] $false }
                "rclick" { $p = $a -split '\s+'; Click $p[0] $p[1] $true }
                "type" { TypeText $a }
                "shot" { Shot (Join-Path $dir $a) | Out-Null }
                default { Write-Output "unknown step: $line" }
            }
        }
        Write-Output "ran $Arg"
    }
    "dialogs" { Dialogs | ForEach-Object { Write-Output "'$([W]::Text($_))'" } }
    "stop" {
        foreach ($d in @(Dialogs)) { [W]::SendMessage($d, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null }  # WM_CLOSE
        Start-Sleep -Milliseconds 500
        Game | ForEach-Object { $_.CloseMainWindow() | Out-Null }
        for ($i = 0; $i -lt 10 -and (Game); $i++) {
            foreach ($d in @(Dialogs)) { [W]::SendMessage($d, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null }
            Start-Sleep -Milliseconds 500
        }
        if (Game) { Game | Stop-Process -Force; Write-Output "had to force-kill" } else { Write-Output "closed" }
        Start-Sleep -Milliseconds 300
        Enable-DisabledWindows
    }
    default { Write-Output "unknown command $Command"; exit 2 }
}
