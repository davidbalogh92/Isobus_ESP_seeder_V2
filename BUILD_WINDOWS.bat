@echo off
setlocal
cd /d "%~dp0"
echo ==============================================
echo  Vetogep ISOBUS - forditas es BIN
echo  AgIsoAuxInputs (8 MB) es Waveshare 8DI-8RO (16 MB)
echo ==============================================
where pio >nul 2>nul || (echo PlatformIO nem talalhato. Telepites: py -3 -m pip install platformio & goto fail)
py -3 build_release.py
if errorlevel 1 goto fail
echo KESZ. A BIN fajlok a firmware mappaban vannak.
pause
exit /b 0
:fail
echo HIBA. Masold ki az elso hibauzenetet.
pause
exit /b 1
