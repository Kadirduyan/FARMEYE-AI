# ==============================================================================
# Stage 1: Build Stage (C++17 + CMake + Qt6 + OpenCV DNN + GStreamer 1.0)
# ==============================================================================
FROM ubuntu:24.04 AS build

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    ninja-build \
    pkg-config \
    qt6-base-dev \
    libqt6opengl6-dev \
    libqt6sql6-sqlite \
    libopencv-dev \
    libgstreamer1.0-dev \
    libgstreamer-plugins-base1.0-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src

COPY CMakeLists.txt ./
COPY src/ ./src/
COPY resources/ ./resources/

RUN cmake -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DFARMEYE_BUILD_ENGINE=ON \
    -DFARMEYE_BUILD_UI=ON \
    -DFARMEYE_ENABLE_GSTREAMER=ON \
    && cmake --build build --parallel

# ==============================================================================
# Stage 2: Runtime Stage (Qt6 UI via Xvfb/X11 + Headless farmeye_engine + Python YOLO)
# ==============================================================================
FROM ubuntu:24.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive \
    QT_QPA_PLATFORM=offscreen \
    FARMEYE_DATA_ROOT=/data

RUN apt-get update && apt-get install -y --no-install-recommends \
    libqt6widgets6 \
    libqt6opengl6 \
    libqt6openglwidgets6 \
    libqt6gui6 \
    libqt6core6 \
    libqt6sql6 \
    libqt6sql6-sqlite \
    libopencv-core406t64 \
    libopencv-imgproc406t64 \
    libopencv-imgcodecs406t64 \
    libopencv-highgui406t64 \
    libopencv-dnn406t64 \
    libgstreamer1.0-0 \
    libgstreamer-plugins-base1.0-0 \
    gstreamer1.0-plugins-base \
    gstreamer1.0-plugins-good \
    gstreamer1.0-plugins-bad \
    gstreamer1.0-libav \
    xvfb \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY --from=build /src/build/farmeye_engine /app/farmeye_engine
COPY --from=build /src/build/FarmEyeAI /app/FarmEyeAI
COPY config/ /app/config/
COPY models/yolov8n.onnx /app/models/yolov8n.onnx
COPY models/yolov8n_cow.onnx /app/models/yolov8n_cow.onnx
COPY src/ui/theme.qss /app/src/ui/theme.qss
COPY scripts/ /app/scripts/

RUN mkdir -p /data/kayitlar/veriler /data/kayitlar/gunluk_videolar

VOLUME ["/data"]

# Varsayılan olarak konsol tabanlı gerçek zamanlı AI motorunu veya UI'ı çalıştırır
ENTRYPOINT ["/app/farmeye_engine"]
CMD ["/data/inekvideolar/videolar/1.mp4", "/app/models/yolov8n_cow.onnx"]
