#pragma once

#include <opencv2/core.hpp>
#include <QString>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace farmeye {

// Kalıcı insan kimliği.
// Birincil: C:/FarmEyeData/person_identities.json (her zaman)
// İkincil:  C:/FarmEyeData/people.db (SQLite eklentisi varsa)
//
// Yanlış pozitiften kaçın: isimli eşleşme YÜKSEK eşik + margin ister.
// excludeIds: aynı karede başka track'e verilmiş kalıcı ID'ler (örn. Kadir zaten ekranda).
class PersonIdentityStore {
public:
    explicit PersonIdentityStore(const std::string &dbPath = std::string());

    int resolve(const cv::Mat &bgrCrop, const std::set<int> &excludeIds = {});

    bool setName(int id, const QString &name);
    QString nameOf(int id) const;
    bool isNamed(int id) const;
    bool isLocked(int id) const;

    int count() const;
    QString storageHint() const;

private:
    struct Entry {
        int id = 0;
        QString name;
        bool locked = false;
        std::vector<float> hist;
    };

    static std::vector<float> makeHist(const cv::Mat &bgr);
    static float similarity(const std::vector<float> &a, const std::vector<float> &b);

    void openDb();
    void loadAll();
    bool persistEntry(const Entry &e) const;
    bool saveJsonUnlocked() const;
    bool upsertSqlUnlocked(const Entry &e) const;
    int allocNextIdUnlocked();

    mutable std::mutex m_mutex;
    std::string m_dbPath;
    std::string m_jsonPath;
    QString m_connName;
    bool m_sqlOk = false;
    std::vector<Entry> m_ids;
    int m_nextId = 1;

    // İsimli: sıkı. İsimsiz: biraz yumuşak (aynı kişi 007→008 olmasın).
    float m_namedMatchThresh = 0.90f;
    float m_namedMargin = 0.06f;
    float m_openMatchThresh = 0.76f;
    float m_histUpdateMin = 0.90f;
};

} // namespace farmeye
