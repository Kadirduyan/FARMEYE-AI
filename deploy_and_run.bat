@echo off
setlocal EnableExtensions
cd /d C:\FarmEye\FARMEYE-AI\build-ui

set "VCPKG_QT=C:\vcpkg\installed\x64-windows"
set "GST=%LOCALAPPDATA%\Programs\gstreamer\1.0\msvc_x86_64"

echo === Deploy Qt plugins (vcpkg) ===
if not exist platforms mkdir platforms
if not exist styles mkdir styles
if not exist imageformats mkdir imageformats
if not exist sqldrivers mkdir sqldrivers

copy /Y "%VCPKG_QT%\bin\Qt6*.dll" . >nul 2>nul
copy /Y "%VCPKG_QT%\bin\opencv_*.dll" . >nul 2>nul

if exist "%VCPKG_QT%\Qt6\plugins\platforms\qwindows.dll" (
  copy /Y "%VCPKG_QT%\Qt6\plugins\platforms\qwindows.dll" platforms\ >nul
  echo Copied vcpkg Qt6\plugins\platforms\qwindows.dll
) else if exist "%VCPKG_QT%\plugins\platforms\qwindows.dll" (
  copy /Y "%VCPKG_QT%\plugins\platforms\qwindows.dll" platforms\ >nul
  echo Copied vcpkg plugins\platforms\qwindows.dll
)

if exist "%VCPKG_QT%\Qt6\plugins\styles\qmodernwindowsstyle.dll" copy /Y "%VCPKG_QT%\Qt6\plugins\styles\qmodernwindowsstyle.dll" styles\ >nul 2>nul
if exist "%VCPKG_QT%\Qt6\plugins\styles\qwindowsvistastyle.dll" copy /Y "%VCPKG_QT%\Qt6\plugins\styles\qwindowsvistastyle.dll" styles\ >nul 2>nul
if exist "%VCPKG_QT%\Qt6\plugins\sqldrivers\qsqlite.dll" copy /Y "%VCPKG_QT%\Qt6\plugins\sqldrivers\qsqlite.dll" sqldrivers\ >nul 2>nul

echo === Check platforms ===
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
