<# :
@echo off
cd /d "%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "Invoke-Expression ([System.IO.File]::ReadAllText('%~f0'))"
exit /b %errorlevel%
#>
# Detects the tools used by the VS Code / CMake build and stores them as user
# environment variables. Not needed for e2studio.
$ErrorActionPreference = 'Stop'
function Write-Ok   ($m) { Write-Host "  [OK]   $m" -ForegroundColor Green }
function Write-Warn ($m) { Write-Host "  [WARN] $m" -ForegroundColor Yellow }
function Set-UserEnv { param([string]$Name, [string]$Value)
    [System.Environment]::SetEnvironmentVariable($Name, $Value, 'User')
    Set-Item -Path "Env:$Name" -Value $Value
    Write-Ok "$Name = $Value" }
function Find-Newest { param([string[]]$Patterns)
    Get-ChildItem -Path $Patterns -ErrorAction SilentlyContinue | Sort-Object FullName | Select-Object -Last 1 }

Write-Host ""; Write-Host "ra6e2_da14531ebz_v2 - VS Code environment setup" -ForegroundColor Cyan; Write-Host ""
$allFound = $true
$e2  = "$env:LOCALAPPDATA\Programs\Renesas\e2_studio"
$rpf = "$env:USERPROFILE\.renesas\platform"

Write-Host "1) LLVM (ATfE) toolchain..."
$atfe = Find-Newest @("$e2\toolchains\llvm_arm\ATfE-*\bin\clang.exe")
if ($atfe) { Set-UserEnv 'ARM_TOOLCHAIN_PATH' $atfe.DirectoryName }
else { $allFound = $false; Write-Warn "ATfE not found. Install LLVM for Arm 21.1.1 via e2studio, then re-run." }

Write-Host "2) GNU Arm toolchain (fallback kit)..."
$gcc = Find-Newest @("$rpf\arm-gnu\*\*\bin\arm-none-eabi-gcc.exe", "$e2\toolchains\gcc_arm\*\bin\arm-none-eabi-gcc.exe", "C:\arm-gnu-toolchain-*\bin\arm-none-eabi-gcc.exe")
if ($gcc) { Set-UserEnv 'ARM_GCC_TOOLCHAIN_PATH' $gcc.DirectoryName }
else { Write-Warn "arm-none-eabi-gcc not found (only needed for the 'ARM GCC' kit)." }

Write-Host "3) Arm GNU debugger (gdb)..."
$gdb = $null
if ($gcc) { $gdb = Get-Item (Join-Path $gcc.DirectoryName 'arm-none-eabi-gdb.exe') -ErrorAction SilentlyContinue }
if (-not $gdb) { $gdb = Find-Newest @("$rpf\DebugComp\RA\arm-none-eabi-gdb.exe", "C:\arm-gnu-toolchain-*\bin\arm-none-eabi-gdb.exe") }
if ($gdb) { Set-UserEnv 'ARM_GDB_PATH' $gdb.FullName }
else { $allFound = $false; Write-Warn "arm-none-eabi-gdb.exe not found (needed for debugging)." }

Write-Host "3b) Python 3.10 for the Renesas debugger gdb..."
$py = Find-Newest @("$e2\eclipse\plugins\com.renesas.python3.win32.x86_64_*\bin\python310.dll")
if ($py) { Set-UserEnv 'RENESAS_GDB_PYTHONHOME' $py.DirectoryName }
else { $allFound = $false; Write-Warn "e2studio Python 3.10 plugin not found (needed by 'Renesas GDB Debugging Default')." }

Write-Host "4) Ninja..."
$ninja = (Get-Command ninja -ErrorAction SilentlyContinue).Source
if (-not $ninja) { $n = Find-Newest @("$rpf\ninja-build\ninja.exe", "C:\cmake-*\bin\ninja.exe"); if ($n) { $ninja = $n.FullName } }
if ($ninja) { Set-UserEnv 'NINJA_PATH' (Split-Path $ninja) }
else { $allFound = $false; Write-Warn "ninja.exe not found." }

Write-Host "5) FSP Smart Configurator (RASC)..."
$fsp = ''
$cfg = Join-Path $PWD 'configuration.xml'
if (Test-Path $cfg) { $m = Select-String -Path $cfg -Pattern '#FSPVersion#" value="([0-9.]+)"' | Select-Object -First 1; if ($m) { $fsp = $m.Matches[0].Groups[1].Value } }
$rascAll = Get-ChildItem -Path @("$rpf\sc\ra\fsp_*\eclipse\rasc.exe", "C:\Renesas\RA\sc_v*\eclipse\rasc.exe", "$env:LOCALAPPDATA\Programs\Renesas\RA\sc_v*\eclipse\rasc.exe") -ErrorAction SilentlyContinue | Sort-Object FullName
$rasc = $null
if ($fsp) { $rasc = $rascAll | Where-Object { $_.FullName -match "fsp_v?$([regex]::Escape($fsp))[\\_]" } | Select-Object -Last 1 }
if (-not $rasc) { $rasc = $rascAll | Select-Object -Last 1 }
if ($rasc) { Set-UserEnv 'RASC_EXE_PATH' $rasc.FullName; if ($fsp -and $rasc.FullName -notmatch [regex]::Escape($fsp)) { Write-Warn "RASC does not match FSP $fsp from configuration.xml." } }
else { $allFound = $false; Write-Warn "rasc.exe not found. Install the FSP $fsp Smart Configurator." }

Write-Host ""
if ($allFound) { Write-Ok "All required tools configured." } else { Write-Warn "Some tools are missing (see above)." }
Write-Host "IMPORTANT: close and re-open VS Code so it picks up the new variables." -ForegroundColor Magenta
if ($Host.Name -eq 'ConsoleHost' -and -not $env:WT_SESSION) { Read-Host "Press Enter to close" }
