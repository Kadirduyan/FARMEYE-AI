#include "class_policy.h"

#include <algorithm>
#include <unordered_set>

namespace farmeye {

namespace {

// COCO animal-like classes (excluding person=0, cow=19 handled separately)
const std::unordered_set<int> kOtherAnimals = {
    14, // bird
    15, // cat
    16, // dog
    17, // horse
    // 18 sheep → Cow (çiftlik / fotoğraf)
    20, // elephant
    21, // bear
    22, // zebra
    23, // giraffe
};

} // namespace

ObjectKind classifyCoco(int classId)
{
    if (classId == 0) {
        return ObjectKind::Human;
    }
    // cow=19; telefon fotoğraflarında sıkça sheep=18 / horse=17 karışır → inek say
    if (classId == 19 || classId == 18) {
        return ObjectKind::Cow;
    }
    if (kOtherAnimals.count(classId) != 0) {
        return ObjectKind::OtherAnimal;
    }
    return ObjectKind::Ignore;
}

std::string kindToTurkish(ObjectKind kind)
{
    switch (kind) {
    case ObjectKind::Cow:
        return "inek";
    case ObjectKind::Human:
        return "insan";
    case ObjectKind::OtherAnimal:
        return "başka canlı";
    default:
        return "yok";
    }
}

std::vector<ClassifiedDetection> applyClassPolicy(const std::vector<Detection> &dets)
{
    std::vector<ClassifiedDetection> out;
    out.reserve(dets.size());
    for (const Detection &d : dets) {
        const ObjectKind kind = classifyCoco(d.classId);
        if (kind == ObjectKind::Ignore) {
            continue;
        }
        ClassifiedDetection c;
        static_cast<Detection &>(c) = d;
        c.kind = kind;
        c.labelTr = kindToTurkish(kind);
        out.push_back(std::move(c));
    }
    return out;
}

namespace {

float boxArea(const ClassifiedDetection &d)
{
    return (std::max)(0.f, d.x2 - d.x1) * (std::max)(0.f, d.y2 - d.y1);
}

float intersectionArea(const ClassifiedDetection &a, const ClassifiedDetection &b)
{
    const float xx1 = (std::max)(a.x1, b.x1);
    const float yy1 = (std::max)(a.y1, b.y1);
    const float xx2 = (std::min)(a.x2, b.x2);
    const float yy2 = (std::min)(a.y2, b.y2);
    return (std::max)(0.f, xx2 - xx1) * (std::max)(0.f, yy2 - yy1);
}

} // namespace

std::vector<ClassifiedDetection> suppressNestedHumans(const std::vector<ClassifiedDetection> &dets)
{
    std::vector<bool> drop(dets.size(), false);
    for (size_t i = 0; i < dets.size(); ++i) {
        if (dets[i].kind != ObjectKind::Human) {
            continue;
        }
        const float ai = boxArea(dets[i]);
        if (ai < 1.f) {
            drop[i] = true;
            continue;
        }
        for (size_t j = 0; j < dets.size(); ++j) {
            if (i == j || dets[j].kind != ObjectKind::Human) {
                continue;
            }
            const float aj = boxArea(dets[j]);
            // i, j'nin içinde ve belirgin şekilde daha küçük → telefon yüzü vb.
            if (aj > ai * 1.8f) {
                const float inter = intersectionArea(dets[i], dets[j]);
                if (inter / ai >= 0.55f) {
                    drop[i] = true;
                    break;
                }
            }
        }
    }

    std::vector<ClassifiedDetection> out;
    out.reserve(dets.size());
    for (size_t i = 0; i < dets.size(); ++i) {
        if (!drop[i]) {
            out.push_back(dets[i]);
        }
    }
    return out;
}

} // namespace farmeye
