#pragma once

#include "InferenceEngine.h"

#include <string>

namespace farmeye {

enum class ObjectKind {
    Ignore = 0,
    Cow,         // hedef hayvan (inek)
    Human,       // insan
    OtherAnimal  // başka hareket eden canlı
};

struct ClassifiedDetection : public Detection {
    ObjectKind kind = ObjectKind::Ignore;
    std::string labelTr; // "inek" / "insan" / "başka canlı"
};

ObjectKind classifyCoco(int classId);
std::string kindToTurkish(ObjectKind kind);

// Ignore olanları at; diğerlerini ClassifiedDetection yap.
std::vector<ClassifiedDetection> applyClassPolicy(const std::vector<Detection> &dets);

} // namespace farmeye
