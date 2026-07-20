@echo off
setlocal EnableExtensions
cd /d C:\FarmEye\FARMEYE-AI\build-ui

set "VCPKG_QT=C:\vcpkg\installed\x64-windows"
set "QT683=C:\Qt\6.8.3\msvc2022_64"
set "GST=%LOCALAPPDATA%\Programs\gstreamer\1.0\msvc_x86_64"

echo === Deploy Qt plugins ===
if not exist platforms mkdir platforms
if not exist styles mkdir styles
if not exist imageformats mkdir imageformats

REM Prefer vcpkg plugins (DLL'ler vcpkg'ten geldi)
if exist "%VCPKG_QT%\Qt6\plugins\platforms\qwindows.dll" (
  copy /Y "%VCPKG_QT%\Qt6\plugins\platforms\qwindows.dll" platforms\ >nul
  echo Copied vcpkg Qt6\plugins\platforms
) else if exist "%VCPKG_QT%\plugins\platforms\qwindows.dll" (
  copy /Y "%VCPKG_QT%\plugins\platforms\qwindows.dll" platforms\ >nul
  echo Copied vcpkg plugins\platforms
) else if exist "%QT683%\plugins\platforms\qwindows.dll" (
  copy /Y "%QT683%\plugins\platforms\qwindows.dll" platforms\ >nul
  echo Copied Qt 6.8.3 platforms
) else (
  echo ERROR: qwindows.dll not found
  exit /b 1
)

if exist "%VCPKG_QT%\Qt6\plugins\styles\qmodernwindowsstyle.dll" copy /Y "%VCPKG_QT%\Qt6\plugins\styles\qmodernwindowsstyle.dll" styles\ >nul 2>nul
if exist "%VCPKG_QT%\plugins\styles\qmodernwindowsstyle.dll" copy /Y "%VCPKG_QT%\plugins\styles\qmodernwindowsstyle.dll" styles\ >nul 2>nul
if exist "%VCPKG_QT%\Qt6\plugins\styles\qwindowsvistastyle.dll" copy /Y "%VCPKG_QT%\Qt6\plugins\styles\qwindowsvistastyle.dll" styles\ >nul 2>nul
if exist "%QT683%\plugins\styles\qmodernwindowsstyle.dll" copy /Y "%QT683%\plugins\styles\qmodernwindowsstyle.dll" styles\ >nul 2>nul

REM Try windeployqt if available
where windeployqt >nul 2>&1
if not errorlevel 1 (
  windeployqt --release --no-translations FarmEyeAI.exe
) else if exist "%QT683%\bin\windeployqt.exe" (
  "%QT683%\bin\windeployqt.exe" --release --no-translations FarmEyeAI.exe
) else if exist "%VCPKG_QT%\tools\Qt6\bin\windeployqt.exe" (
  "%VCPKG_QT%\tools\Qt6\bin\windeployqt.exe" --release --no-translations FarmEyeAI.exe
)

echo === Check platforms ===
dir platforms\qwindows.dll
if not exist platforms\qwindows.dll (
  echo STILL MISSING qwindows.dll
  exit /b 2
)

echo === PATH for GStreamer + Qt + vcpkg ===
set "PATH=%VCPKG_QT%\bin;%GST%\bin;%GST%\lib;%PATH%"
set "GST_PLUGIN_PATH=%GST%\lib\gstreamer-1.0"
set "GI_TYPELIB_PATH=%GST%\lib\girepository-1.0"

echo === Launch ===
start "" /D "C:\FarmEye\FARMEYE-AI\build-ui" FarmEyeAI.exe
echo LAUNCHED
exit /b 0
