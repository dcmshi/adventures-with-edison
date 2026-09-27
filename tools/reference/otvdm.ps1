# Runs the ORIGINAL game under winevdm (otvdm) as a visual reference, safely.
#
#   otvdm.ps1 start [EXE]        start the game (default EDISON.EXE)
#   otvdm.ps1 shot OUT.png       screenshot the game window only (no focus change)
#   otvdm.ps1 dialogs            list open dialogs (error boxes) of the game
#   otvdm.ps1 stop               close the game safely
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
param([Parameter(Mandatory)][string]$Command, [string]$Arg)

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

switch ($Command) {
    "start" {
        $exe = if ($Arg) { $Arg } else { "EDISON.EXE" }
        Start-Process -FilePath $otvdm -ArgumentList $exe -WorkingDirectory $runDir
        Write-Output "started $exe"
    }
    "shot" {
        $h = GameWindows | Where-Object { [W]::Cls($_) -ne "#32770" } | Select-Object -First 1
        if (-not $h) { Write-Output "no game window"; exit 1 }
        $r = New-Object RECT; [W]::GetWindowRect($h, [ref]$r) | Out-Null
        $bmp = New-Object System.Drawing.Bitmap ($r.R - $r.L), ($r.B - $r.T)
        [System.Drawing.Graphics]::FromImage($bmp).CopyFromScreen($r.L, $r.T, 0, 0, $bmp.Size)
        $bmp.Save($Arg, [System.Drawing.Imaging.ImageFormat]::Png)
        $d = @(Dialogs).Count
        Write-Output "saved $Arg ($($r.R - $r.L)x$($r.B - $r.T))$(if ($d) { ", $d dialog(s) open" })"
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
