#pragma once

#include <opencv2/core.hpp>
#include <string>
#include <vector>

namespace farmeye {

// İnsan görünümüne göre kalıcı ID (HSV histogram eşleme).
// Dosya: C:/FarmEyeData/person_identities.json (yoksa proje data/)
class PersonIdentityStore {
public:
    explicit PersonIdentityStore(const std::string &jsonPath);

    // Crop BGR → kalıcı ID (1-based). Eşleşme yoksa yeni ID üretir ve kaydeder.
    int resolve(const cv::Mat &bgrCrop);

    void load();
    void save() const;

    int count() const { return static_cast<int>(m_ids.size()); }

private:
    struct Entry {
        int id = 0;
        std::vector<float> hist; // 48 dims: 16H + 16S + 16V
    };

    static std::vector<float> makeHist(const cv::Mat &bgr);
    static float similarity(const std::vector<float> &a, const std::vector<float> &b);

    std::string m_path;
    std::vector<Entry> m_ids;
    int m_nextId = 1;
    float m_matchThresh = 0.82f;
};

} // namespace farmeye
