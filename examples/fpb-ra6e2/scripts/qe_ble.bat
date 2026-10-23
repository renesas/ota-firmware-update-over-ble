@echo off
REM QE-for-BLE sync helper.
REM
REM The COMPILED BLE sources are  src\rs_ota_fw_update_over_ble\ble  (part of the project).
REM qe_gen\ble is QE's working/merge area and is EXCLUDED from the build
REM (.cproject: source entry "qe_gen" excludes "ble") so the two copies can never
REM produce duplicate symbols.
REM
REM QE only preserves code inside its "Start user code" / "End user code" blocks,
REM and it merges that code into ITS OWN copy under qe_gen. So the order matters:
REM
REM   1) qe_ble.bat push   BEFORE  "Generate Code"  (copy -> qe_gen, so QE merges
REM                                                  your CURRENT user code)
REM   2) regenerate the profile in QE
REM   3) qe_ble.bat pull   AFTER   "Generate Code"  (qe_gen -> copy)
REM
REM   qe_ble.bat diff      list differences, change nothing
setlocal
set "R=%~dp0.."
set "GEN=%R%\qe_gen\ble"
set "SRC=%R%\src\rs_ota_fw_update_over_ble\ble"

if /i "%~1"=="pull" ( robocopy "%GEN%" "%SRC%" /MIR /NFL /NDL /NJH /NJS & goto :ok )
if /i "%~1"=="push" ( robocopy "%SRC%" "%GEN%" /MIR /NFL /NDL /NJH /NJS & goto :ok )
if /i "%~1"=="diff" ( robocopy "%GEN%" "%SRC%" /L /NJH /NJS /NS /NC & goto :ok )

echo Usage: qe_ble.bat [pull^|push^|diff]
exit /b 1

:ok
if errorlevel 8 ( echo [ERROR] copy failed & exit /b 1 )
echo [OK] %~1 done
exit /b 0
