#pragma once

#include <QByteArray>
#include <QString>

namespace farmeye {
namespace paths {

// Tek veri kökü (FARMEYE_DATA_ROOT ortam değişkeni varsa onu kullanır, yoksa C:/FarmEyeData)
inline QString dataRoot()
{
    const QByteArray envRoot = qgetenv("FARMEYE_DATA_ROOT");
    if (!envRoot.isEmpty()) {
        return QString::fromUtf8(envRoot);
    }
#if defined(_WIN32)
    return QStringLiteral("C:/FarmEyeData");
#else
    return QStringLiteral("/data");
#endif
}

// Eğitim / dataset — kullanıcının düzenlediği yapı
inline QString cowPhotos()
{
    return dataRoot() + QStringLiteral("/inekfotograflari");
}

inline QString cowVideos()
{
    return dataRoot() + QStringLiteral("/inekvideolar");
}

inline QString cowVideoFiles()
{
    return cowVideos() + QStringLiteral("/videolar");
}

// Uygulama kayıtları
inline QString records()
{
    return dataRoot() + QStringLiteral("/kayitlar");
}

inline QString dailyVideos()
{
    return records() + QStringLiteral("/gunluk_videolar");
}

inline QString databases()
{
    return records() + QStringLiteral("/veriler");
}

inline QString farmeyeDb()
{
    return databases() + QStringLiteral("/farmeye.db");
}

inline QString peopleDb()
{
    return databases() + QStringLiteral("/people.db");
}

inline QString peopleJson()
{
    return databases() + QStringLiteral("/person_identities.json");
}

// YOLO etiketleri (eğitim için)
inline QString yoloLabels()
{
    return dataRoot() + QStringLiteral("/inekfotograflari/etiketler");
}

} // namespace paths
} // namespace farmeye
