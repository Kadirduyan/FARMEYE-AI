@echo off
cd /d C:\FarmEye\FARMEYE-AI\build-ui
set "PATH=C:\vcpkg\installed\x64-windows\bin;%LOCALAPPDATA%\Programs\gstreamer\1.0\msvc_x86_64\bin;%PATH%"
set "GST_PLUGIN_PATH=%LOCALAPPDATA%\Programs\gstreamer\1.0\msvc_x86_64\lib\gstreamer-1.0"
echo platforms: > C:\FarmEye\run_test.txt
dir platforms\qwindows.dll >> C:\FarmEye\run_test.txt 2>&1
echo running 3s... >> C:\FarmEye\run_test.txt
start /B FarmEyeAI.exe
timeout /t 3 /nobreak >nul
tasklist | findstr /i FarmEyeAI >> C:\FarmEye\run_test.txt
if errorlevel 1 (
  echo NOT_IN_TASKLIST >> C:\FarmEye\run_test.txt
) else (
  echo RUNNING_OK >> C:\FarmEye\run_test.txt
)
