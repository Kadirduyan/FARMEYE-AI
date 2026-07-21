#pragma once

// GstCameraCapture — webcam / file / rtsp / usb için GStreamer yakalama.
// Düşük CPU: videoscale + videorate (varsayılan 640x360 @ 12 fps).

#include <cstdint>
#include <string>
#include <vector>

namespace farmeye {

struct FrameBuffer {
    int width = 0;
    int height = 0;
    int channels = 3; // BGR
    std::vector<uint8_t> data;
};

class GstCameraCapture {
public:
    GstCameraCapture() = default;
    ~GstCameraCapture();

    GstCameraCapture(const GstCameraCapture &) = delete;
    GstCameraCapture &operator=(const GstCameraCapture &) = delete;

    // URI: webcam:N | usb:N | rtsp://... | dosya yolu
    bool open(const std::string &uri, int maxW = 640, int maxH = 360);
    bool open(const std::string &uri, int maxW, int maxH, int fps);
    void close();
    bool isOpen() const { return m_open; }

    bool read(FrameBuffer &out);

private:
    bool m_open = false;
    std::string m_uri;
    void *m_pipeline = nullptr;
    void *m_appsink = nullptr;
};

} // namespace farmeye
