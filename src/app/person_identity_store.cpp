#include "person_identity_store.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>
#include <cmath>
#include <opencv2/imgproc.hpp>

namespace farmeye {

namespace {

std::string defaultPath()
{
    const QString farm = QStringLiteral("C:/FarmEyeData/person_identities.json");
    if (QDir().mkpath(QStringLiteral("C:/FarmEyeData"))) {
        return farm.toStdString();
    }
#if defined(FARMEYE_SOURCE_DIR)
    return std::string(FARMEYE_SOURCE_DIR) + "/data/person_identities.json";
#else
    return "data/person_identities.json";
#endif
}

} // namespace

PersonIdentityStore::PersonIdentityStore(const std::string &jsonPath)
    : m_path(jsonPath.empty() ? defaultPath() : jsonPath)
{
    load();
}

std::vector<float> PersonIdentityStore::makeHist(const cv::Mat &bgr)
{
    std::vector<float> out(48, 0.f);
    if (bgr.empty() || bgr.cols < 8 || bgr.rows < 8) {
        return out;
    }
    cv::Mat hsv;
    cv::cvtColor(bgr, hsv, cv::COLOR_BGR2HSV);
    const int bins = 16;
    int counts[48] = {};
    const int n = hsv.rows * hsv.cols;
    for (int y = 0; y < hsv.rows; ++y) {
        const cv::Vec3b *row = hsv.ptr<cv::Vec3b>(y);
        for (int x = 0; x < hsv.cols; ++x) {
            counts[row[x][0] * bins / 180]++;
            counts[bins + row[x][1] * bins / 256]++;
            counts[2 * bins + row[x][2] * bins / 256]++;
        }
    }
    for (int i = 0; i < 48; ++i) {
        out[i] = static_cast<float>(counts[i]) / static_cast<float>(n);
    }
    return out;
}

float PersonIdentityStore::similarity(const std::vector<float> &a, const std::vector<float> &b)
{
    if (a.size() != b.size() || a.empty()) {
        return 0.f;
    }
    double dot = 0, na = 0, nb = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        dot += a[i] * b[i];
        na += a[i] * a[i];
        nb += b[i] * b[i];
    }
    if (na < 1e-12 || nb < 1e-12) {
        return 0.f;
    }
    return static_cast<float>(dot / (std::sqrt(na) * std::sqrt(nb)));
}

int PersonIdentityStore::resolve(const cv::Mat &bgrCrop)
{
    const auto hist = makeHist(bgrCrop);
    int bestId = -1;
    float best = 0.f;
    for (const Entry &e : m_ids) {
        const float s = similarity(hist, e.hist);
        if (s > best) {
            best = s;
            bestId = e.id;
        }
    }
    if (bestId > 0 && best >= m_matchThresh) {
        // soft update histogram
        for (Entry &e : m_ids) {
            if (e.id == bestId) {
                for (size_t i = 0; i < e.hist.size() && i < hist.size(); ++i) {
                    e.hist[i] = e.hist[i] * 0.85f + hist[i] * 0.15f;
                }
                break;
            }
        }
        save();
        return bestId;
    }

    Entry neu;
    neu.id = m_nextId++;
    neu.hist = hist;
    m_ids.push_back(neu);
    save();
    return neu.id;
}

void PersonIdentityStore::load()
{
    m_ids.clear();
    m_nextId = 1;
    QFile f(QString::fromStdString(m_path));
    if (!f.open(QIODevice::ReadOnly)) {
        return;
    }
    const auto doc = QJsonDocument::fromJson(f.readAll());
    const auto arr = doc.object().value(QStringLiteral("people")).toArray();
    for (const auto &v : arr) {
        const auto o = v.toObject();
        Entry e;
        e.id = o.value(QStringLiteral("id")).toInt();
        const auto h = o.value(QStringLiteral("hist")).toArray();
        e.hist.reserve(h.size());
        for (const auto &x : h) {
            e.hist.push_back(static_cast<float>(x.toDouble()));
        }
        if (e.id > 0 && e.hist.size() == 48) {
            m_ids.push_back(e);
            m_nextId = (std::max)(m_nextId, e.id + 1);
        }
    }
}

void PersonIdentityStore::save() const
{
    QJsonArray arr;
    for (const Entry &e : m_ids) {
        QJsonObject o;
        o.insert(QStringLiteral("id"), e.id);
        QJsonArray h;
        for (float x : e.hist) {
            h.append(x);
        }
        o.insert(QStringLiteral("hist"), h);
        arr.append(o);
    }
    QJsonObject root;
    root.insert(QStringLiteral("people"), arr);
    QFile f(QString::fromStdString(m_path));
    QDir().mkpath(QFileInfo(f).absolutePath());
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    }
}

} // namespace farmeye
