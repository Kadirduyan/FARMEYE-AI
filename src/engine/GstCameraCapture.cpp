#include "GstCameraCapture.h"

#include <cstring>
#include <iostream>

#if defined(HAVE_GSTREAMER)
#  include <gst/gst.h>
#  include <gst/app/gstappsink.h>
#endif

namespace farmeye {

namespace {

#if defined(HAVE_GSTREAMER)

bool startsWith(const std::string &s, const char *prefix)
{
    const size_t n = std::char_traits<char>::length(prefix);
    return s.size() >= n && s.compare(0, n, prefix) == 0;
}

bool looksLikeFile(const std::string &uri)
{
    if (startsWith(uri, "file://")) {
        return true;
    }
    if (uri.find(".mp4") != std::string::npos || uri.find(".avi") != std::string::npos ||
        uri.find(".mkv") != std::string::npos || uri.find(".mov") != std::string::npos) {
        return true;
    }
    if (uri.size() >= 2 && uri[1] == ':') {
        return true; // C:\...
    }
    return false;
}

std::string normalizeFilePath(std::string path)
{
    if (startsWith(path, "file://")) {
        path = path.substr(7);
    }
    for (char &c : path) {
        if (c == '\\') {
            c = '/';
        }
    }
    return path;
}

int parseDeviceIndex(const std::string &uri, const char *prefix)
{
    const size_t n = std::char_traits<char>::length(prefix);
    if (uri.size() > n + 1 && uri[n] == ':') {
        try {
            return std::stoi(uri.substr(n + 1));
        } catch (...) {
            return 0;
        }
    }
    return 0;
}

std::string buildPipeline(const std::string &uri)
{
    // Webcam / USB (Windows Media Foundation)
    if (uri == "webcam" || startsWith(uri, "webcam:") || uri == "usb" || startsWith(uri, "usb:")) {
        const int idx = parseDeviceIndex(uri, startsWith(uri, "usb") ? "usb" : "webcam");
        return "mfvideosrc device-index=" + std::to_string(idx) +
               " ! videoconvert ! video/x-raw,format=BGR ! "
               "appsink name=sink emit-signals=false max-buffers=1 drop=true sync=false";
    }

    // RTSP (Windows HW decode)
    if (startsWith(uri, "rtsp://")) {
        return "rtspsrc location=" + uri + " latency=100 ! "
               "rtph264depay ! h264parse ! d3d11h264dec ! d3d11download ! "
               "videoconvert ! video/x-raw,format=BGR ! "
               "appsink name=sink emit-signals=false max-buffers=1 drop=true sync=false";
    }

    // Yerel video dosyası
    if (looksLikeFile(uri)) {
        const std::string path = normalizeFilePath(uri);
        return "filesrc location=\"" + path + "\" ! decodebin ! videoconvert ! "
               "video/x-raw,format=BGR ! appsink name=sink emit-signals=false "
               "max-buffers=1 drop=true sync=false";
    }

    // Varsayılan: RTSP gibi dene
    return "rtspsrc location=" + uri + " latency=100 ! "
           "rtph264depay ! h264parse ! decodebin ! videoconvert ! "
           "video/x-raw,format=BGR ! "
           "appsink name=sink emit-signals=false max-buffers=1 drop=true sync=false";
}

#endif

} // namespace

GstCameraCapture::~GstCameraCapture()
{
    close();
}

bool GstCameraCapture::open(const std::string &uri)
{
    close();
    m_uri = uri;
    if (uri.empty()) {
        return false;
    }

#if !defined(HAVE_GSTREAMER)
    std::cerr << "[GstCameraCapture] GStreamer bagli degil (HAVE_GSTREAMER yok).\n";
    return false;
#else
    static bool gstInited = false;
    if (!gstInited) {
        gst_init(nullptr, nullptr);
        gstInited = true;
    }

    const std::string pipelineStr = buildPipeline(uri);
    std::cout << "[GstCameraCapture] pipeline: " << pipelineStr << "\n";

    GError *error = nullptr;
    GstElement *pipeline = gst_parse_launch(pipelineStr.c_str(), &error);
    if (!pipeline) {
        std::cerr << "[GstCameraCapture] pipeline hatasi: "
                  << (error ? error->message : "bilinmiyor") << "\n";
        if (error) {
            g_error_free(error);
        }
        return false;
    }
    if (error) {
        g_error_free(error);
    }

    GstElement *sink = gst_bin_get_by_name(GST_BIN(pipeline), "sink");
    if (!sink) {
        std::cerr << "[GstCameraCapture] appsink bulunamadi\n";
        gst_object_unref(pipeline);
        return false;
    }

    const GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        std::cerr << "[GstCameraCapture] PLAYING basarisiz\n";
        gst_object_unref(sink);
        gst_object_unref(pipeline);
        return false;
    }

    m_pipeline = pipeline;
    m_appsink = sink;
    m_open = true;
    return true;
#endif
}

void GstCameraCapture::close()
{
#if defined(HAVE_GSTREAMER)
    if (m_pipeline) {
        gst_element_set_state(static_cast<GstElement *>(m_pipeline), GST_STATE_NULL);
        gst_object_unref(static_cast<GstElement *>(m_pipeline));
    }
    if (m_appsink) {
        gst_object_unref(static_cast<GstElement *>(m_appsink));
    }
#endif
    m_pipeline = nullptr;
    m_appsink = nullptr;
    m_open = false;
}

bool GstCameraCapture::read(FrameBuffer &out)
{
    out = {};
#if !defined(HAVE_GSTREAMER)
    return false;
#else
    if (!m_open || !m_appsink) {
        return false;
    }

    GstSample *sample =
        gst_app_sink_try_pull_sample(GST_APP_SINK(m_appsink), 1000 * GST_MSECOND);
    if (!sample) {
        return false;
    }

    GstBuffer *buffer = gst_sample_get_buffer(sample);
    GstCaps *caps = gst_sample_get_caps(sample);
    if (!buffer || !caps) {
        gst_sample_unref(sample);
        return false;
    }

    const GstStructure *s = gst_caps_get_structure(caps, 0);
    int width = 0;
    int height = 0;
    gst_structure_get_int(s, "width", &width);
    gst_structure_get_int(s, "height", &height);
    if (width <= 0 || height <= 0) {
        gst_sample_unref(sample);
        return false;
    }

    GstMapInfo map;
    if (!gst_buffer_map(buffer, &map, GST_MAP_READ)) {
        gst_sample_unref(sample);
        return false;
    }

    const size_t expected = static_cast<size_t>(width) * static_cast<size_t>(height) * 3u;
    out.width = width;
    out.height = height;
    out.channels = 3;
    out.data.resize(expected);
    std::memcpy(out.data.data(), map.data, (std::min)(map.size, expected));

    gst_buffer_unmap(buffer, &map);
    gst_sample_unref(sample);
    return true;
#endif
}

} // namespace farmeye
