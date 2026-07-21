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
        return true;
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

// Güvenilir kuyruk: ölçek + BGR + drop (videorate yok — webcam'lerde kareyi kesebiliyor)
std::string appsinkTail(int maxW, int maxH)
{
    if (maxW < 160) {
        maxW = 160;
    }
    if (maxH < 120) {
        maxH = 120;
    }
    return "videoconvert ! videoscale method=0 ! "
           "video/x-raw,format=BGR,width=" +
           std::to_string(maxW) + ",height=" + std::to_string(maxH) +
           " ! appsink name=sink emit-signals=false max-buffers=1 drop=true sync=false";
}

// En basit: sadece BGR appsink (ölçek yok) — son çare
std::string appsinkTailSimple()
{
    return "videoconvert ! video/x-raw,format=BGR ! "
           "appsink name=sink emit-signals=false max-buffers=1 drop=true sync=false";
}

std::string buildWebcamPipeline(int idx, int maxW, int maxH, int variant)
{
    // variant 0: ölçekli (tercih)
    // variant 1: ölçeksiz basit
    if (variant == 0) {
        return "mfvideosrc device-index=" + std::to_string(idx) + " ! " + appsinkTail(maxW, maxH);
    }
    return "mfvideosrc device-index=" + std::to_string(idx) + " ! " + appsinkTailSimple();
}

std::string buildPipeline(const std::string &uri, int maxW, int maxH, int variant)
{
    if (uri == "webcam" || startsWith(uri, "webcam:") || uri == "usb" || startsWith(uri, "usb:")) {
        const int idx = parseDeviceIndex(uri, startsWith(uri, "usb") ? "usb" : "webcam");
        return buildWebcamPipeline(idx, maxW, maxH, variant);
    }

    const std::string sink = (variant == 0) ? appsinkTail(maxW, maxH) : appsinkTailSimple();

    if (startsWith(uri, "rtsp://")) {
        if (variant == 0) {
            return "rtspsrc location=" + uri + " latency=120 drop-on-latency=true ! "
                   "rtph264depay ! h264parse ! d3d11h264dec ! d3d11download ! " +
                   sink;
        }
        return "rtspsrc location=" + uri + " latency=120 drop-on-latency=true ! "
               "rtph264depay ! h264parse ! decodebin ! " + sink;
    }

    if (looksLikeFile(uri)) {
        const std::string path = normalizeFilePath(uri);
        return "filesrc location=\"" + path + "\" ! decodebin ! " + sink;
    }

    return "rtspsrc location=" + uri + " latency=120 drop-on-latency=true ! "
           "decodebin ! " + sink;
}

bool tryStartAndVerify(const std::string &pipelineStr, GstElement **outPipe, GstElement **outSink)
{
    std::cout << "[GstCameraCapture] try: " << pipelineStr << "\n";

    GError *error = nullptr;
    GstElement *pipeline = gst_parse_launch(pipelineStr.c_str(), &error);
    if (!pipeline) {
        std::cerr << "[GstCameraCapture] parse hatasi: "
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
        std::cerr << "[GstCameraCapture] appsink yok\n";
        gst_object_unref(pipeline);
        return false;
    }

    const GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        std::cerr << "[GstCameraCapture] PLAYING failure\n";
        gst_object_unref(sink);
        gst_object_unref(pipeline);
        return false;
    }

    // ASYNC tamamlanmasını bekle
    gst_element_get_state(pipeline, nullptr, nullptr, 2 * GST_SECOND);

    // İlk kare gelmezse pipeline işe yaramıyor say
    GstSample *sample =
        gst_app_sink_try_pull_sample(GST_APP_SINK(sink), 2500 * GST_MSECOND);
    if (!sample) {
        std::cerr << "[GstCameraCapture] ilk kare gelmedi — pipeline elendi\n";
        gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(sink);
        gst_object_unref(pipeline);
        return false;
    }
    gst_sample_unref(sample);

    *outPipe = pipeline;
    *outSink = sink;
    return true;
}

#endif

} // namespace

GstCameraCapture::~GstCameraCapture()
{
    close();
}

bool GstCameraCapture::open(const std::string &uri, int maxW, int maxH)
{
    return open(uri, maxW, maxH, 12);
}

bool GstCameraCapture::open(const std::string &uri, int maxW, int maxH, int /*fps*/)
{
    close();
    m_uri = uri;
    if (uri.empty()) {
        return false;
    }

#if !defined(HAVE_GSTREAMER)
    (void)maxW;
    (void)maxH;
    std::cerr << "[GstCameraCapture] GStreamer bagli degil (HAVE_GSTREAMER yok).\n";
    return false;
#else
    static bool gstInited = false;
    if (!gstInited) {
        gst_init(nullptr, nullptr);
        gstInited = true;
    }

    GstElement *pipeline = nullptr;
    GstElement *sink = nullptr;

    // 0: ölçekli, 1: basit — ilk kare doğrulamalı
    for (int variant = 0; variant < 2; ++variant) {
        const std::string pipe = buildPipeline(uri, maxW, maxH, variant);
        if (tryStartAndVerify(pipe, &pipeline, &sink)) {
            m_pipeline = pipeline;
            m_appsink = sink;
            m_open = true;
            std::cout << "[GstCameraCapture] OK variant=" << variant << "\n";
            return true;
        }
    }

    return false;
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
        gst_app_sink_try_pull_sample(GST_APP_SINK(m_appsink), 100 * GST_MSECOND);
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
