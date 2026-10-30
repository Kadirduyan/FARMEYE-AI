#include "farmeye_store.h"
#include "farmeye_paths.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <QVariant>
#include <iostream>

namespace farmeye {

namespace {

constexpr int kBehaviorRetainDays = 30;

QString defaultDbPath()
{
    return paths::farmeyeDb();
}

QString defaultVideosRoot()
{
    return paths::dailyVideos();
}

} // namespace

FarmEyeStore::FarmEyeStore(const QString &dbPath)
    : m_dbPath(dbPath.isEmpty() ? defaultDbPath() : dbPath)
    , m_videosRoot(defaultVideosRoot())
{
    QDir().mkpath(paths::databases());
    QDir().mkpath(paths::dailyVideos());
    QDir().mkpath(paths::cowPhotos());
    QDir().mkpath(paths::cowVideoFiles());
    QDir().mkpath(paths::yoloLabels());
    QDir().mkpath(QFileInfo(m_dbPath).absolutePath());
    QDir().mkpath(m_videosRoot);
    openDb();
    if (m_sqlOk) {
        purgeExpired();
    }
}

QString FarmEyeStore::storageHint() const
{
    if (!m_sqlOk) {
        return QStringLiteral("farmeye.db açılamadı");
    }
    return QStringLiteral("SQLite: %1 | videolar: %2 (günlük) | davranış: %3 gün")
        .arg(m_dbPath, m_videosRoot)
        .arg(kBehaviorRetainDays);
}

void FarmEyeStore::openDb()
{
    m_connName = QStringLiteral("farmeye_store_%1").arg(QUuid::createUuid().toString(QUuid::Id128));
    auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connName);
    db.setDatabaseName(m_dbPath);
    if (!db.open()) {
        std::cerr << "[FarmEyeStore] SQLite open failed: "
                  << db.lastError().text().toStdString() << "\n";
        m_sqlOk = false;
        return;
    }
    m_sqlOk = true;
    ensureSchema();
}

bool FarmEyeStore::exec(const QString &sql) const
{
    QSqlQuery q(QSqlDatabase::database(m_connName));
    if (!q.exec(sql)) {
        std::cerr << "[FarmEyeStore] SQL error: " << q.lastError().text().toStdString()
                  << " | " << sql.toStdString() << "\n";
        return false;
    }
    return true;
}

void FarmEyeStore::ensureSchema()
{
    exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS schema_meta ("
        "  key TEXT PRIMARY KEY,"
        "  value TEXT NOT NULL"
        ")"));
    exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS animals ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  tag TEXT NOT NULL UNIQUE,"
        "  name TEXT NOT NULL DEFAULT '',"
        "  active INTEGER NOT NULL DEFAULT 1,"
        "  created_at TEXT NOT NULL"
        ")"));
    exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS video_recordings ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  camera_id INTEGER NOT NULL,"
        "  day TEXT NOT NULL,"
        "  started_at TEXT NOT NULL,"
        "  ended_at TEXT,"
        "  file_path TEXT NOT NULL,"
        "  duration_sec INTEGER NOT NULL DEFAULT 0,"
        "  status TEXT NOT NULL DEFAULT 'recording'"
        ")"));
    exec(QStringLiteral(
        "CREATE INDEX IF NOT EXISTS idx_video_day ON video_recordings(day)"));
    exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS behavior_events ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  animal_id INTEGER NOT NULL,"
        "  day TEXT NOT NULL,"
        "  ts TEXT NOT NULL,"
        "  event_type TEXT NOT NULL,"
        "  confidence REAL NOT NULL DEFAULT 0,"
        "  camera_id INTEGER NOT NULL DEFAULT 0,"
        "  recording_id INTEGER,"
        "  note TEXT NOT NULL DEFAULT '',"
        "  FOREIGN KEY(animal_id) REFERENCES animals(id)"
        ")"));
    exec(QStringLiteral(
        "CREATE INDEX IF NOT EXISTS idx_be_animal_day ON behavior_events(animal_id, day)"));
    exec(QStringLiteral(
        "CREATE INDEX IF NOT EXISTS idx_be_day ON behavior_events(day)"));
    exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS behavior_daily ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  animal_id INTEGER NOT NULL,"
        "  day TEXT NOT NULL,"
        "  water_sec INTEGER NOT NULL DEFAULT 0,"
        "  eat_sec INTEGER NOT NULL DEFAULT 0,"
        "  sleep_sec INTEGER NOT NULL DEFAULT 0,"
        "  stand_sec INTEGER NOT NULL DEFAULT 0,"
        "  walk_sec INTEGER NOT NULL DEFAULT 0,"
        "  fall_count INTEGER NOT NULL DEFAULT 0,"
        "  limp_score_max REAL NOT NULL DEFAULT 0,"
        "  updated_at TEXT NOT NULL,"
        "  UNIQUE(animal_id, day),"
        "  FOREIGN KEY(animal_id) REFERENCES animals(id)"
        ")"));
    exec(QStringLiteral(
        "CREATE INDEX IF NOT EXISTS idx_bd_day ON behavior_daily(day)"));

    QSqlQuery q(QSqlDatabase::database(m_connName));
    q.prepare(QStringLiteral("INSERT OR REPLACE INTO schema_meta(key, value) VALUES('version', '1')"));
    q.exec();
}

void FarmEyeStore::purgeExpired()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_sqlOk) {
        return;
    }

    const QDate today = QDate::currentDate();
    const QString todayStr = today.toString(Qt::ISODate);
    const QString cutoff = today.addDays(-kBehaviorRetainDays).toString(Qt::ISODate);

    // 1) Eski video klasörleri + satırlar (günlük)
    {
        QSqlQuery q(QSqlDatabase::database(m_connName));
        q.prepare(QStringLiteral("SELECT id, file_path, day FROM video_recordings WHERE day < ?"));
        q.addBindValue(todayStr);
        if (q.exec()) {
            while (q.next()) {
                const QString path = q.value(1).toString();
                if (!path.isEmpty()) {
                    QFile::remove(path);
                }
            }
        }
        QSqlQuery del(QSqlDatabase::database(m_connName));
        del.prepare(QStringLiteral("DELETE FROM video_recordings WHERE day < ?"));
        del.addBindValue(todayStr);
        del.exec();

        // Gün klasörlerini sil
        QDir root(m_videosRoot);
        const auto dirs = root.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString &name : dirs) {
            const QDate d = QDate::fromString(name, Qt::ISODate);
            if (d.isValid() && d < today) {
                QDir(root.filePath(name)).removeRecursively();
            }
        }
    }

    // 2) Davranış 30 gün
    {
        QSqlQuery q(QSqlDatabase::database(m_connName));
        q.prepare(QStringLiteral("DELETE FROM behavior_events WHERE day < ?"));
        q.addBindValue(cutoff);
        q.exec();
        q.prepare(QStringLiteral("DELETE FROM behavior_daily WHERE day < ?"));
        q.addBindValue(cutoff);
        q.exec();
    }
}

int FarmEyeStore::ensureAnimal(const QString &tag, const QString &name)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_sqlOk || tag.trimmed().isEmpty()) {
        return 0;
    }

    QSqlQuery q(QSqlDatabase::database(m_connName));
    q.prepare(QStringLiteral("SELECT id FROM animals WHERE tag = ?"));
    q.addBindValue(tag.trimmed());
    if (q.exec() && q.next()) {
        return q.value(0).toInt();
    }

    q.prepare(QStringLiteral(
        "INSERT INTO animals(tag, name, active, created_at) VALUES(?,?,1,?)"));
    q.addBindValue(tag.trimmed());
    q.addBindValue(name.trimmed());
    q.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
    if (!q.exec()) {
        return 0;
    }
    return q.lastInsertId().toInt();
}

bool FarmEyeStore::setAnimalName(int animalId, const QString &name)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_sqlOk || animalId <= 0) {
        return false;
    }
    QSqlQuery q(QSqlDatabase::database(m_connName));
    q.prepare(QStringLiteral("UPDATE animals SET name = ? WHERE id = ?"));
    q.addBindValue(name.trimmed());
    q.addBindValue(animalId);
    return q.exec();
}

QVector<AnimalRow> FarmEyeStore::listAnimals(bool onlyActive) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    QVector<AnimalRow> out;
    if (!m_sqlOk) {
        return out;
    }
    QSqlQuery q(QSqlDatabase::database(m_connName));
    if (onlyActive) {
        q.exec(QStringLiteral("SELECT id, tag, name, active FROM animals WHERE active = 1 ORDER BY id"));
    } else {
        q.exec(QStringLiteral("SELECT id, tag, name, active FROM animals ORDER BY id"));
    }
    while (q.next()) {
        AnimalRow r;
        r.id = q.value(0).toInt();
        r.tag = q.value(1).toString();
        r.name = q.value(2).toString();
        r.active = q.value(3).toInt() != 0;
        out.push_back(r);
    }
    return out;
}

std::optional<AnimalRow> FarmEyeStore::animalById(int animalId) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_sqlOk) {
        return std::nullopt;
    }
    QSqlQuery q(QSqlDatabase::database(m_connName));
    q.prepare(QStringLiteral("SELECT id, tag, name, active FROM animals WHERE id = ?"));
    q.addBindValue(animalId);
    if (!q.exec() || !q.next()) {
        return std::nullopt;
    }
    AnimalRow r;
    r.id = q.value(0).toInt();
    r.tag = q.value(1).toString();
    r.name = q.value(2).toString();
    r.active = q.value(3).toInt() != 0;
    return r;
}

QString FarmEyeStore::eventTypeToString(BehaviorEventType t)
{
    switch (t) {
    case BehaviorEventType::DrinkStart: return QStringLiteral("drink_start");
    case BehaviorEventType::DrinkEnd: return QStringLiteral("drink_end");
    case BehaviorEventType::EatStart: return QStringLiteral("eat_start");
    case BehaviorEventType::EatEnd: return QStringLiteral("eat_end");
    case BehaviorEventType::LieStart: return QStringLiteral("lie_start");
    case BehaviorEventType::LieEnd: return QStringLiteral("lie_end");
    case BehaviorEventType::StandStart: return QStringLiteral("stand_start");
    case BehaviorEventType::WalkStart: return QStringLiteral("walk_start");
    case BehaviorEventType::Fall: return QStringLiteral("fall");
    case BehaviorEventType::LimpStart: return QStringLiteral("limp_start");
    case BehaviorEventType::LimpEnd: return QStringLiteral("limp_end");
    case BehaviorEventType::LowActivity: return QStringLiteral("low_activity");
    default: return QStringLiteral("other");
    }
}

BehaviorEventType FarmEyeStore::eventTypeFromString(const QString &s)
{
    if (s == QLatin1String("drink_start")) return BehaviorEventType::DrinkStart;
    if (s == QLatin1String("drink_end")) return BehaviorEventType::DrinkEnd;
    if (s == QLatin1String("eat_start")) return BehaviorEventType::EatStart;
    if (s == QLatin1String("eat_end")) return BehaviorEventType::EatEnd;
    if (s == QLatin1String("lie_start")) return BehaviorEventType::LieStart;
    if (s == QLatin1String("lie_end")) return BehaviorEventType::LieEnd;
    if (s == QLatin1String("stand_start")) return BehaviorEventType::StandStart;
    if (s == QLatin1String("walk_start")) return BehaviorEventType::WalkStart;
    if (s == QLatin1String("fall")) return BehaviorEventType::Fall;
    if (s == QLatin1String("limp_start")) return BehaviorEventType::LimpStart;
    if (s == QLatin1String("limp_end")) return BehaviorEventType::LimpEnd;
    if (s == QLatin1String("low_activity")) return BehaviorEventType::LowActivity;
    return BehaviorEventType::Other;
}

void FarmEyeStore::upsertDailyTouch(int animalId, const QDate &day) const
{
    QSqlQuery q(QSqlDatabase::database(m_connName));
    q.prepare(QStringLiteral(
        "INSERT INTO behavior_daily(animal_id, day, updated_at) VALUES(?,?,?) "
        "ON CONFLICT(animal_id, day) DO UPDATE SET updated_at = excluded.updated_at"));
    q.addBindValue(animalId);
    q.addBindValue(day.toString(Qt::ISODate));
    q.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
    q.exec();
}

void FarmEyeStore::bumpDailyCounters(int animalId, const QDate &day, BehaviorEventType type, double confidence)
{
    upsertDailyTouch(animalId, day);
    QSqlQuery q(QSqlDatabase::database(m_connName));
    if (type == BehaviorEventType::Fall) {
        q.prepare(QStringLiteral(
            "UPDATE behavior_daily SET fall_count = fall_count + 1, updated_at = ? "
            "WHERE animal_id = ? AND day = ?"));
        q.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
        q.addBindValue(animalId);
        q.addBindValue(day.toString(Qt::ISODate));
        q.exec();
    } else if (type == BehaviorEventType::LimpStart) {
        q.prepare(QStringLiteral(
            "UPDATE behavior_daily SET "
            "limp_score_max = CASE WHEN limp_score_max > ? THEN limp_score_max ELSE ? END, "
            "updated_at = ? WHERE animal_id = ? AND day = ?"));
        q.addBindValue(confidence);
        q.addBindValue(confidence);
        q.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
        q.addBindValue(animalId);
        q.addBindValue(day.toString(Qt::ISODate));
        q.exec();
    }
}

void FarmEyeStore::applyDurationToDaily(int animalId, BehaviorEventType endType, int durationSec, const QDate &day)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_sqlOk || animalId <= 0 || durationSec <= 0) {
        return;
    }
    upsertDailyTouch(animalId, day);
    QString col;
    switch (endType) {
    case BehaviorEventType::DrinkEnd: col = QStringLiteral("water_sec"); break;
    case BehaviorEventType::EatEnd: col = QStringLiteral("eat_sec"); break;
    case BehaviorEventType::LieEnd: col = QStringLiteral("sleep_sec"); break;
    default: return;
    }
    QSqlQuery q(QSqlDatabase::database(m_connName));
    q.prepare(QStringLiteral("UPDATE behavior_daily SET %1 = %1 + ?, updated_at = ? "
                             "WHERE animal_id = ? AND day = ?").arg(col));
    q.addBindValue(durationSec);
    q.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
    q.addBindValue(animalId);
    q.addBindValue(day.toString(Qt::ISODate));
    q.exec();
}

qint64 FarmEyeStore::recordEvent(int animalId,
                                 BehaviorEventType type,
                                 const QDateTime &tsIn,
                                 int cameraId,
                                 double confidence,
                                 const QString &note,
                                 std::optional<qint64> recordingId)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_sqlOk || animalId <= 0) {
        return 0;
    }
    const QDateTime ts = tsIn.isValid() ? tsIn : QDateTime::currentDateTime();
    const QDate day = ts.date();

    QSqlQuery q(QSqlDatabase::database(m_connName));
    q.prepare(QStringLiteral(
        "INSERT INTO behavior_events(animal_id, day, ts, event_type, confidence, camera_id, recording_id, note) "
        "VALUES(?,?,?,?,?,?,?,?)"));
    q.addBindValue(animalId);
    q.addBindValue(day.toString(Qt::ISODate));
    q.addBindValue(ts.toString(Qt::ISODate));
    q.addBindValue(eventTypeToString(type));
    q.addBindValue(confidence);
    q.addBindValue(cameraId);
    if (recordingId.has_value()) {
        q.addBindValue(recordingId.value());
    } else {
        q.addBindValue(QVariant());
    }
    q.addBindValue(note);
    if (!q.exec()) {
        return 0;
    }
    const qint64 id = q.lastInsertId().toLongLong();
    bumpDailyCounters(animalId, day, type, confidence);
    return id;
}

QVector<BehaviorEventRow> FarmEyeStore::eventsForAnimal(int animalId, const QDate &from, const QDate &to) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    QVector<BehaviorEventRow> out;
    if (!m_sqlOk) {
        return out;
    }
    QSqlQuery q(QSqlDatabase::database(m_connName));
    q.prepare(QStringLiteral(
        "SELECT id, animal_id, day, ts, event_type, confidence, camera_id, recording_id, note "
        "FROM behavior_events WHERE animal_id = ? AND day >= ? AND day <= ? ORDER BY ts"));
    q.addBindValue(animalId);
    q.addBindValue(from.toString(Qt::ISODate));
    q.addBindValue(to.toString(Qt::ISODate));
    if (!q.exec()) {
        return out;
    }
    while (q.next()) {
        BehaviorEventRow r;
        r.id = q.value(0).toLongLong();
        r.animalId = q.value(1).toInt();
        r.day = QDate::fromString(q.value(2).toString(), Qt::ISODate);
        r.ts = QDateTime::fromString(q.value(3).toString(), Qt::ISODate);
        r.type = eventTypeFromString(q.value(4).toString());
        r.confidence = q.value(5).toDouble();
        r.cameraId = q.value(6).toInt();
        if (!q.value(7).isNull()) {
            r.recordingId = q.value(7).toLongLong();
        }
        r.note = q.value(8).toString();
        out.push_back(r);
    }
    return out;
}

std::optional<BehaviorDailyRow> FarmEyeStore::dailyFor(int animalId, const QDate &day) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_sqlOk) {
        return std::nullopt;
    }
    QSqlQuery q(QSqlDatabase::database(m_connName));
    q.prepare(QStringLiteral(
        "SELECT id, animal_id, day, water_sec, eat_sec, sleep_sec, stand_sec, walk_sec, "
        "fall_count, limp_score_max, updated_at FROM behavior_daily "
        "WHERE animal_id = ? AND day = ?"));
    q.addBindValue(animalId);
    q.addBindValue(day.toString(Qt::ISODate));
    if (!q.exec() || !q.next()) {
        return std::nullopt;
    }
    BehaviorDailyRow r;
    r.id = q.value(0).toLongLong();
    r.animalId = q.value(1).toInt();
    r.day = QDate::fromString(q.value(2).toString(), Qt::ISODate);
    r.waterSec = q.value(3).toInt();
    r.eatSec = q.value(4).toInt();
    r.sleepSec = q.value(5).toInt();
    r.standSec = q.value(6).toInt();
    r.walkSec = q.value(7).toInt();
    r.fallCount = q.value(8).toInt();
    r.limpScoreMax = q.value(9).toDouble();
    r.updatedAt = QDateTime::fromString(q.value(10).toString(), Qt::ISODate);
    return r;
}

QVector<BehaviorDailyRow> FarmEyeStore::dailyRange(int animalId, const QDate &from, const QDate &to) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    QVector<BehaviorDailyRow> out;
    if (!m_sqlOk) {
        return out;
    }
    QSqlQuery q(QSqlDatabase::database(m_connName));
    q.prepare(QStringLiteral(
        "SELECT id, animal_id, day, water_sec, eat_sec, sleep_sec, stand_sec, walk_sec, "
        "fall_count, limp_score_max, updated_at FROM behavior_daily "
        "WHERE animal_id = ? AND day >= ? AND day <= ? ORDER BY day"));
    q.addBindValue(animalId);
    q.addBindValue(from.toString(Qt::ISODate));
    q.addBindValue(to.toString(Qt::ISODate));
    if (!q.exec()) {
        return out;
    }
    while (q.next()) {
        BehaviorDailyRow r;
        r.id = q.value(0).toLongLong();
        r.animalId = q.value(1).toInt();
        r.day = QDate::fromString(q.value(2).toString(), Qt::ISODate);
        r.waterSec = q.value(3).toInt();
        r.eatSec = q.value(4).toInt();
        r.sleepSec = q.value(5).toInt();
        r.standSec = q.value(6).toInt();
        r.walkSec = q.value(7).toInt();
        r.fallCount = q.value(8).toInt();
        r.limpScoreMax = q.value(9).toDouble();
        r.updatedAt = QDateTime::fromString(q.value(10).toString(), Qt::ISODate);
        out.push_back(r);
    }
    return out;
}

QString FarmEyeStore::videoDayDir(const QDate &day) const
{
    const QString dir = m_videosRoot + QLatin1Char('/') + day.toString(Qt::ISODate);
    QDir().mkpath(dir);
    return dir;
}

qint64 FarmEyeStore::beginVideoRecording(int cameraId, const QString &filePath, const QDateTime &startedAtIn)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_sqlOk) {
        return 0;
    }
    const QDateTime startedAt = startedAtIn.isValid() ? startedAtIn : QDateTime::currentDateTime();
    QSqlQuery q(QSqlDatabase::database(m_connName));
    q.prepare(QStringLiteral(
        "INSERT INTO video_recordings(camera_id, day, started_at, file_path, status) "
        "VALUES(?,?,?,?, 'recording')"));
    q.addBindValue(cameraId);
    q.addBindValue(startedAt.date().toString(Qt::ISODate));
    q.addBindValue(startedAt.toString(Qt::ISODate));
    q.addBindValue(filePath);
    if (!q.exec()) {
        return 0;
    }
    return q.lastInsertId().toLongLong();
}

bool FarmEyeStore::endVideoRecording(qint64 recordingId, const QDateTime &endedAtIn)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_sqlOk || recordingId <= 0) {
        return false;
    }
    const QDateTime endedAt = endedAtIn.isValid() ? endedAtIn : QDateTime::currentDateTime();

    QSqlQuery sel(QSqlDatabase::database(m_connName));
    sel.prepare(QStringLiteral("SELECT started_at FROM video_recordings WHERE id = ?"));
    sel.addBindValue(recordingId);
    if (!sel.exec() || !sel.next()) {
        return false;
    }
    const QDateTime started = QDateTime::fromString(sel.value(0).toString(), Qt::ISODate);
    const qint64 dur = started.secsTo(endedAt);

    QSqlQuery q(QSqlDatabase::database(m_connName));
    q.prepare(QStringLiteral(
        "UPDATE video_recordings SET ended_at = ?, duration_sec = ?, status = 'closed' WHERE id = ?"));
    q.addBindValue(endedAt.toString(Qt::ISODate));
    q.addBindValue(dur < 0 ? 0 : dur);
    q.addBindValue(recordingId);
    return q.exec();
}

QVector<VideoRecordingRow> FarmEyeStore::videosForDay(const QDate &day) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    QVector<VideoRecordingRow> out;
    if (!m_sqlOk) {
        return out;
    }
    QSqlQuery q(QSqlDatabase::database(m_connName));
    q.prepare(QStringLiteral(
        "SELECT id, camera_id, day, started_at, ended_at, file_path, duration_sec, status "
        "FROM video_recordings WHERE day = ? ORDER BY started_at"));
    q.addBindValue(day.toString(Qt::ISODate));
    if (!q.exec()) {
        return out;
    }
    while (q.next()) {
        VideoRecordingRow r;
        r.id = q.value(0).toLongLong();
        r.cameraId = q.value(1).toInt();
        r.day = QDate::fromString(q.value(2).toString(), Qt::ISODate);
        r.startedAt = QDateTime::fromString(q.value(3).toString(), Qt::ISODate);
        if (!q.value(4).isNull()) {
            r.endedAt = QDateTime::fromString(q.value(4).toString(), Qt::ISODate);
        }
        r.filePath = q.value(5).toString();
        r.durationSec = q.value(6).toLongLong();
        r.status = q.value(7).toString();
        out.push_back(r);
    }
    return out;
}

} // namespace farmeye
