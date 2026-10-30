#pragma once

#include <QString>
#include <QDate>
#include <QDateTime>
#include <QVector>
#include <mutex>
#include <optional>

namespace farmeye {

// Merkezi saha verisi — yollar: farmeye_paths.h
//   kayitlar/veriler/farmeye.db
//   kayitlar/gunluk_videolar/YYYY-MM-DD/...  (günlük silinir)
//
// Saklama:
//   video_recordings  → sadece o gün
//   behavior_*        → 30 gün
//
// Davranış: yaptığı anda behavior_events; günlük özet behavior_daily.

enum class BehaviorEventType {
    DrinkStart,
    DrinkEnd,
    EatStart,
    EatEnd,
    LieStart,
    LieEnd,
    StandStart,
    WalkStart,
    Fall,
    LimpStart,
    LimpEnd,
    LowActivity,
    Other
};

struct AnimalRow {
    int id = 0;
    QString tag;
    QString name;
    bool active = true;
};

struct BehaviorEventRow {
    qint64 id = 0;
    int animalId = 0;
    QDate day;
    QDateTime ts;
    BehaviorEventType type = BehaviorEventType::Other;
    double confidence = 0.0;
    int cameraId = 0;
    std::optional<qint64> recordingId;
    QString note;
};

struct BehaviorDailyRow {
    qint64 id = 0;
    int animalId = 0;
    QDate day;
    int waterSec = 0;
    int eatSec = 0;
    int sleepSec = 0;
    int standSec = 0;
    int walkSec = 0;
    int fallCount = 0;
    double limpScoreMax = 0.0;
    QDateTime updatedAt;
};

struct VideoRecordingRow {
    qint64 id = 0;
    int cameraId = 0;
    QDate day;
    QDateTime startedAt;
    QDateTime endedAt;
    QString filePath;
    qint64 durationSec = 0;
    QString status; // recording | closed | deleted
};

class FarmEyeStore {
public:
    explicit FarmEyeStore(const QString &dbPath = QString());

    bool isOpen() const { return m_sqlOk; }
    QString dbPath() const { return m_dbPath; }
    QString videosRoot() const { return m_videosRoot; }
    QString storageHint() const;

    // Açılışta + gün değişiminde çağır
    void purgeExpired();

    // --- İnek kimliği ---
    int ensureAnimal(const QString &tag, const QString &name = QString());
    bool setAnimalName(int animalId, const QString &name);
    QVector<AnimalRow> listAnimals(bool onlyActive = true) const;
    std::optional<AnimalRow> animalById(int animalId) const;

    // --- Anlık davranış (yaptığı anda) ---
    qint64 recordEvent(int animalId,
                       BehaviorEventType type,
                       const QDateTime &ts = QDateTime(),
                       int cameraId = 0,
                       double confidence = 0.0,
                       const QString &note = QString(),
                       std::optional<qint64> recordingId = std::nullopt);

    QVector<BehaviorEventRow> eventsForAnimal(int animalId,
                                              const QDate &from,
                                              const QDate &to) const;

    // --- Günlük özet (1 ay) ---
    std::optional<BehaviorDailyRow> dailyFor(int animalId, const QDate &day) const;
    QVector<BehaviorDailyRow> dailyRange(int animalId, const QDate &from, const QDate &to) const;

    // DrinkEnd / EatEnd vb. gelince süreyi daily'ye ekler (eşStart ile eşleşirse)
    void applyDurationToDaily(int animalId, BehaviorEventType endType, int durationSec, const QDate &day);

    // --- Video (günlük klasör) ---
    QString videoDayDir(const QDate &day) const;
    qint64 beginVideoRecording(int cameraId, const QString &filePath, const QDateTime &startedAt = QDateTime());
    bool endVideoRecording(qint64 recordingId, const QDateTime &endedAt = QDateTime());
    QVector<VideoRecordingRow> videosForDay(const QDate &day) const;

    static QString eventTypeToString(BehaviorEventType t);
    static BehaviorEventType eventTypeFromString(const QString &s);

private:
    void openDb();
    bool exec(const QString &sql) const;
    void ensureSchema();
    void bumpDailyCounters(int animalId, const QDate &day, BehaviorEventType type, double confidence);
    void upsertDailyTouch(int animalId, const QDate &day) const;

    mutable std::mutex m_mutex;
    QString m_dbPath;
    QString m_videosRoot;
    QString m_connName;
    bool m_sqlOk = false;
};

} // namespace farmeye
