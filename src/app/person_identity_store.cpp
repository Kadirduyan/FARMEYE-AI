#include "person_identity_store.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <opencv2/imgproc.hpp>

namespace farmeye {

namespace {

QString histToBlob(const std::vector<float> &hist)
{
    QByteArray ba(reinterpret_cast<const char *>(hist.data()),
                  static_cast<int>(hist.size() * sizeof(float)));
    return QString::fromLatin1(ba.toBase64());
}

std::vector<float> histFromBlob(const QString &b64)
{
    const QByteArray ba = QByteArray::fromBase64(b64.toLatin1());
    std::vector<float> out(ba.size() / static_cast<int>(sizeof(float)));
    if (!out.empty()) {
        std::memcpy(out.data(), ba.constData(), out.size() * sizeof(float));
    }
    return out;
}

QJsonArray histToJson(const std::vector<float> &hist)
{
    QJsonArray a;
    for (float x : hist) {
        a.append(x);
    }
    return a;
}

std::vector<float> histFromJson(const QJsonArray &a)
{
    std::vector<float> out;
    out.reserve(a.size());
    for (const auto &x : a) {
        out.push_back(static_cast<float>(x.toDouble()));
    }
    return out;
}

} // namespace

PersonIdentityStore::PersonIdentityStore(const std::string &dbPath)
    : m_dbPath(dbPath.empty() ? "C:/FarmEyeData/people.db" : dbPath)
    , m_jsonPath("C:/FarmEyeData/person_identities.json")
    , m_connName(QStringLiteral("farmeye_people_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces)))
{
    QDir().mkpath(QStringLiteral("C:/FarmEyeData"));
#if defined(FARMEYE_SOURCE_DIR)
    if (!QDir().exists(QStringLiteral("C:/FarmEyeData"))) {
        m_dbPath = std::string(FARMEYE_SOURCE_DIR) + "/data/people.db";
        m_jsonPath = std::string(FARMEYE_SOURCE_DIR) + "/data/person_identities.json";
        QDir().mkpath(QFileInfo(QString::fromStdString(m_jsonPath)).absolutePath());
    }
#endif
    openDb();
    loadAll();
}

void PersonIdentityStore::openDb()
{
    if (!QSqlDatabase::isDriverAvailable(QStringLiteral("QSQLITE"))) {
        std::cerr << "[PersonIdentity] QSQLITE driver YOK — sadece JSON kullanılacak\n";
        m_sqlOk = false;
        return;
    }

    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connName);
    db.setDatabaseName(QString::fromStdString(m_dbPath));
    if (!db.open()) {
        std::cerr << "[PersonIdentity] SQLite açılamadı: " << db.lastError().text().toStdString() << "\n";
        m_sqlOk = false;
        return;
    }

    QSqlQuery q(db);
    if (!q.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS people ("
            "  id INTEGER PRIMARY KEY,"
            "  name TEXT NOT NULL DEFAULT '',"
            "  locked INTEGER NOT NULL DEFAULT 0,"
            "  hist_b64 TEXT NOT NULL,"
            "  updated_at TEXT NOT NULL DEFAULT (datetime('now'))"
            ")"))) {
        std::cerr << "[PersonIdentity] CREATE TABLE: " << q.lastError().text().toStdString() << "\n";
        m_sqlOk = false;
        return;
    }
    m_sqlOk = true;
    std::cerr << "[PersonIdentity] SQLite OK: " << m_dbPath << "\n";
}

void PersonIdentityStore::loadAll()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_ids.clear();
    m_nextId = 1;

    // 1) JSON (her zaman dene — isimler burada da olabilir)
    {
        QFile f(QString::fromStdString(m_jsonPath));
        if (f.open(QIODevice::ReadOnly)) {
            const auto doc = QJsonDocument::fromJson(f.readAll());
            const auto arr = doc.object().value(QStringLiteral("people")).toArray();
            for (const auto &v : arr) {
                const auto o = v.toObject();
                Entry e;
                e.id = o.value(QStringLiteral("id")).toInt();
                e.name = o.value(QStringLiteral("name")).toString();
                e.locked = o.value(QStringLiteral("locked")).toBool(false) || !e.name.isEmpty();
                e.hist = histFromJson(o.value(QStringLiteral("hist")).toArray());
                if (e.id > 0 && e.hist.size() == 48) {
                    m_ids.push_back(e);
                    m_nextId = (std::max)(m_nextId, e.id + 1);
                }
            }
        }
    }

    // 2) SQLite — isimli kayıtları JSON üzerine yaz (daha güncel olabilir)
    if (m_sqlOk) {
        QSqlDatabase db = QSqlDatabase::database(m_connName);
        QSqlQuery q(db);
        if (q.exec(QStringLiteral("SELECT id, name, locked, hist_b64 FROM people ORDER BY id"))) {
            while (q.next()) {
                Entry e;
                e.id = q.value(0).toInt();
                e.name = q.value(1).toString();
                e.locked = q.value(2).toInt() != 0 || !e.name.isEmpty();
                e.hist = histFromBlob(q.value(3).toString());
                if (e.id <= 0 || e.hist.size() != 48) {
                    continue;
                }
                bool found = false;
                for (Entry &ex : m_ids) {
                    if (ex.id == e.id) {
                        // İsimli SQLite kaydı öncelikli
                        if (!e.name.isEmpty() || e.locked) {
                            ex = e;
                        } else if (ex.name.isEmpty()) {
                            ex.hist = e.hist;
                        }
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    m_ids.push_back(e);
                }
                m_nextId = (std::max)(m_nextId, e.id + 1);
            }
        }
    }

    // Diskleri senkron tut
    saveJsonUnlocked();
    if (m_sqlOk) {
        for (const Entry &e : m_ids) {
            upsertSqlUnlocked(e);
        }
    }

    std::cerr << "[PersonIdentity] Yüklendi: " << m_ids.size() << " kişi"
              << " (json=" << m_jsonPath << ", sql=" << (m_sqlOk ? "on" : "off") << ")\n";
}

bool PersonIdentityStore::saveJsonUnlocked() const
{
    QJsonArray arr;
    for (const Entry &e : m_ids) {
        QJsonObject o;
        o.insert(QStringLiteral("id"), e.id);
        o.insert(QStringLiteral("name"), e.name);
        o.insert(QStringLiteral("locked"), e.locked);
        o.insert(QStringLiteral("hist"), histToJson(e.hist));
        arr.append(o);
    }
    QJsonObject root;
    root.insert(QStringLiteral("people"), arr);

    QFile f(QString::fromStdString(m_jsonPath));
    QDir().mkpath(QFileInfo(f).absolutePath());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        std::cerr << "[PersonIdentity] JSON yazılamadı: " << m_jsonPath << "\n";
        return false;
    }
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    f.flush();
    return true;
}

bool PersonIdentityStore::upsertSqlUnlocked(const Entry &e) const
{
    if (!m_sqlOk) {
        return false;
    }
    QSqlDatabase db = QSqlDatabase::database(m_connName);
    if (!db.isOpen()) {
        return false;
    }
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "INSERT INTO people (id, name, locked, hist_b64, updated_at) "
        "VALUES (?, ?, ?, ?, datetime('now')) "
        "ON CONFLICT(id) DO UPDATE SET "
        "  name=excluded.name, locked=excluded.locked, hist_b64=excluded.hist_b64, "
        "  updated_at=datetime('now')"));
    q.addBindValue(e.id);
    q.addBindValue(e.name);
    q.addBindValue(e.locked ? 1 : 0);
    q.addBindValue(histToBlob(e.hist));
    if (!q.exec()) {
        std::cerr << "[PersonIdentity] SQL upsert: " << q.lastError().text().toStdString() << "\n";
        return false;
    }
    return true;
}

bool PersonIdentityStore::persistEntry(const Entry &e) const
{
    const bool j = saveJsonUnlocked();
    const bool s = upsertSqlUnlocked(e);
    return j || s;
}

int PersonIdentityStore::allocNextIdUnlocked()
{
    return m_nextId++;
}

std::vector<float> PersonIdentityStore::makeHist(const cv::Mat &bgr)
{
    std::vector<float> out(48, 0.f);
    if (bgr.empty() || bgr.cols < 8 || bgr.rows < 8) {
        return out;
    }
    // Üst gövde (yüz/göğüs) daha ayırt edici — crop'un üst %60'ı
    const int hUse = (std::max)(8, static_cast<int>(bgr.rows * 0.6));
    cv::Mat roi = bgr(cv::Rect(0, 0, bgr.cols, hUse));
    cv::Mat hsv;
    cv::cvtColor(roi, hsv, cv::COLOR_BGR2HSV);
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

int PersonIdentityStore::resolve(const cv::Mat &bgrCrop, const std::set<int> &excludeIds)
{
    const auto hist = makeHist(bgrCrop);
    bool emptyHist = true;
    for (float v : hist) {
        if (v > 1e-6f) {
            emptyHist = false;
            break;
        }
    }
    if (emptyHist) {
        return -1;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    int bestNamed = -1;
    float bestNamedScore = 0.f;
    float secondNamedScore = 0.f;
    int bestOpen = -1;
    float bestOpenScore = 0.f;

    for (const Entry &e : m_ids) {
        if (excludeIds.count(e.id)) {
            continue; // bu kalıcı ID şu an başka track'te
        }
        const float s = similarity(hist, e.hist);
        if (e.locked || !e.name.isEmpty()) {
            if (s > bestNamedScore) {
                secondNamedScore = bestNamedScore;
                bestNamedScore = s;
                bestNamed = e.id;
            } else if (s > secondNamedScore) {
                secondNamedScore = s;
            }
        } else if (s > bestOpenScore) {
            bestOpenScore = s;
            bestOpen = e.id;
        }
    }

    // İsimli: yüksek eşik + net fark. Şüphede YENİ ID (yanlış "Kadir" olmasın).
    int matchId = -1;
    if (bestNamed > 0 && bestNamedScore >= m_namedMatchThresh
        && (bestNamedScore - secondNamedScore) >= m_namedMargin) {
        matchId = bestNamed;
    } else if (bestOpen > 0 && bestOpenScore >= m_openMatchThresh) {
        matchId = bestOpen;
    }

    if (matchId > 0) {
        for (Entry &e : m_ids) {
            if (e.id != matchId) {
                continue;
            }
            const float score = (e.locked || !e.name.isEmpty()) ? bestNamedScore : bestOpenScore;
            if (score >= m_histUpdateMin) {
                const float alpha = e.locked ? 0.04f : 0.12f;
                for (size_t i = 0; i < e.hist.size() && i < hist.size(); ++i) {
                    e.hist[i] = e.hist[i] * (1.f - alpha) + hist[i] * alpha;
                }
                persistEntry(e);
            }
            return e.id;
        }
    }

    Entry neu;
    neu.id = allocNextIdUnlocked();
    neu.hist = hist;
    neu.name.clear();
    neu.locked = false;
    m_ids.push_back(neu);
    persistEntry(neu);
    return neu.id;
}

bool PersonIdentityStore::setName(int id, const QString &name)
{
    const QString trimmed = name.trimmed();
    if (id <= 0 || trimmed.isEmpty()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    for (Entry &e : m_ids) {
        if (e.id == id) {
            e.name = trimmed;
            e.locked = true;
            const bool ok = persistEntry(e);
            if (!ok) {
                std::cerr << "[PersonIdentity] setName disk yazımı BAŞARISIZ id=" << id << "\n";
            }
            return ok;
        }
    }
    return false;
}

QString PersonIdentityStore::nameOf(int id) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const Entry &e : m_ids) {
        if (e.id == id) {
            return e.name;
        }
    }
    return {};
}

bool PersonIdentityStore::isNamed(int id) const
{
    return !nameOf(id).isEmpty();
}

bool PersonIdentityStore::isLocked(int id) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const Entry &e : m_ids) {
        if (e.id == id) {
            return e.locked || !e.name.isEmpty();
        }
    }
    return false;
}

int PersonIdentityStore::count() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return static_cast<int>(m_ids.size());
}

QString PersonIdentityStore::storageHint() const
{
    if (m_sqlOk) {
        return QStringLiteral("C:/FarmEyeData/people.db + person_identities.json");
    }
    return QStringLiteral("C:/FarmEyeData/person_identities.json");
}

} // namespace farmeye
