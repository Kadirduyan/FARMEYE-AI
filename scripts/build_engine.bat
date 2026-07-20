@echo off
REM yolov8n.onnx -> yolov8n_fp16.engine (TensorRT trtexec)
REM Onkosul: TensorRT kurulu ve trtexec PATH'te

set ROOT=%~dp0..
set ONNX=%ROOT%\models\yolov8n.onnx
set ENGINE=%ROOT%\models\yolov8n_fp16.engine

where trtexec >nul 2>&1
if errorlevel 1 (
  echo [HATA] trtexec bulunamadi. TensorRT kurup PATH'e ekleyin.
  exit /b 1
)

if not exist "%ONNX%" (
  echo [HATA] ONNX yok: %ONNX%
  exit /b 1
)

echo Building FP16 engine for GTX 1650...
trtexec --onnx="%ONNX%" --saveEngine="%ENGINE%" --fp16
if errorlevel 1 exit /b 1
echo OK: %ENGINE%
