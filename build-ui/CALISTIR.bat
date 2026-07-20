@echo off
REM Cift tikla - FarmEye AI calistir
cd /d C:\FarmEye\FARMEYE-AI\build-ui
set "PATH=C:\vcpkg\installed\x64-windows\bin;%LOCALAPPDATA%\Programs\gstreamer\1.0\msvc_x86_64\bin;%LOCALAPPDATA%\Programs\gstreamer\1.0\msvc_x86_64\lib;%PATH%"
set "GST_PLUGIN_PATH=%LOCALAPPDATA%\Programs\gstreamer\1.0\msvc_x86_64\lib\gstreamer-1.0"
if not exist platforms\qwindows.dll (
  echo Eksik plugin - once deploy_and_run.bat calistir
  pause
  exit /b 1
)
start "" FarmEyeAI.exe
