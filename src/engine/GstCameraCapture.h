#pragma once

// GstCameraCapture — webcam / file / rtsp / usb için GStreamer yakalama.

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

    // URI örnekleri:
    //   "webcam" / "webcam:0"
    //   "C:/videos/test.mp4" veya "file:///C:/videos/test.mp4"
    //   "rtsp://192.168.1.10/stream"
    //   "usb:0" (ileride; şimdilik webcam ile aynı)
    bool open(const std::string &uri);
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
