@echo off
setlocal
cd /d C:\FarmEye\FARMEYE-AI\build-ui
set OUT=C:\FarmEye\diag.txt
echo ==== DIAG %DATE% %TIME% ==== > "%OUT%"

echo === EXE === >> "%OUT%"
dir FarmEyeAI.exe >> "%OUT%" 2>&1

echo === platforms === >> "%OUT%"
dir platforms >> "%OUT%" 2>&1
if not exist platforms\qwindows.dll echo MISSING platforms\qwindows.dll >> "%OUT%"

echo === Qt DLLs === >> "%OUT%"
dir Qt6*.dll >> "%OUT%" 2>&1

echo === GST PATH === >> "%OUT%"
echo GST=%GSTREAMER_1_0_ROOT_MSVC_X86_64% >> "%OUT%"
dir "%LOCALAPPDATA%\Programs\gstreamer\1.0\msvc_x86_64\bin\gstreamer-1.0-0.dll" >> "%OUT%" 2>&1

echo === TRY RUN === >> "%OUT%"
FarmEyeAI.exe > C:\FarmEye\app_stdout.txt 2> C:\FarmEye\app_stderr.txt
echo EXIT=%ERRORLEVEL% >> "%OUT%"
type C:\FarmEye\app_stderr.txt >> "%OUT%"
type C:\FarmEye\app_stdout.txt >> "%OUT%"
echo ==== END ==== >> "%OUT%"
