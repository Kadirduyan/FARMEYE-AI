#include "class_policy.h"

#include <unordered_set>

namespace farmeye {

namespace {

// COCO animal-like classes (excluding person=0, cow=19 handled separately)
const std::unordered_set<int> kOtherAnimals = {
    14, // bird
    15, // cat
    16, // dog
    17, // horse
    18, // sheep
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
    if (classId == 19) {
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

} // namespace farmeye
