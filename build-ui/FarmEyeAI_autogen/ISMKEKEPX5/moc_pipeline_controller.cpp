/****************************************************************************
** Meta object code from reading C++ file 'pipeline_controller.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/app/pipeline_controller.h"
#include <QtCore/qmetatype.h>
#include <QtCore/QList>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'pipeline_controller.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.11.1. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {
struct qt_meta_tag_ZN7farmeye18PipelineControllerE_t {};
} // unnamed namespace

template <> constexpr inline auto farmeye::PipelineController::qt_create_metaobjectdata<qt_meta_tag_ZN7farmeye18PipelineControllerE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "farmeye::PipelineController",
        "frameReady",
        "",
        "QImage",
        "frame",
        "QList<farmeye::TrackOverlay>",
        "tracks",
        "thumbFrameReady",
        "camId",
        "alertRaised",
        "farmeye::AlertEvent",
        "alert",
        "runningChanged",
        "running",
        "statusMessage",
        "msg",
        "start",
        "pause",
        "setModelPath",
        "path",
        "setPersonName",
        "persistentId",
        "name",
        "personName",
        "peopleStorageHint",
        "pollUiFrames"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'frameReady'
        QtMocHelpers::SignalData<void(const QImage &, const QVector<farmeye::TrackOverlay> &)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 }, { 0x80000000 | 5, 6 },
        }}),
        // Signal 'thumbFrameReady'
        QtMocHelpers::SignalData<void(int, const QImage &)>(7, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 8 }, { 0x80000000 | 3, 4 },
        }}),
        // Signal 'alertRaised'
        QtMocHelpers::SignalData<void(farmeye::AlertEvent)>(9, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 10, 11 },
        }}),
        // Signal 'runningChanged'
        QtMocHelpers::SignalData<void(bool)>(12, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 13 },
        }}),
        // Signal 'statusMessage'
        QtMocHelpers::SignalData<void(const QString &)>(14, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 15 },
        }}),
        // Slot 'start'
        QtMocHelpers::SlotData<void()>(16, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'pause'
        QtMocHelpers::SlotData<void()>(17, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'setModelPath'
        QtMocHelpers::SlotData<void(const QString &)>(18, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 19 },
        }}),
        // Slot 'setPersonName'
        QtMocHelpers::SlotData<bool(int, const QString &)>(20, 2, QMC::AccessPublic, QMetaType::Bool, {{
            { QMetaType::Int, 21 }, { QMetaType::QString, 22 },
        }}),
        // Slot 'personName'
        QtMocHelpers::SlotData<QString(int) const>(23, 2, QMC::AccessPublic, QMetaType::QString, {{
            { QMetaType::Int, 21 },
        }}),
        // Slot 'peopleStorageHint'
        QtMocHelpers::SlotData<QString() const>(24, 2, QMC::AccessPublic, QMetaType::QString),
        // Slot 'pollUiFrames'
        QtMocHelpers::SlotData<void()>(25, 2, QMC::AccessPrivate, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<PipelineController, qt_meta_tag_ZN7farmeye18PipelineControllerE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject farmeye::PipelineController::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN7farmeye18PipelineControllerE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN7farmeye18PipelineControllerE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN7farmeye18PipelineControllerE_t>.metaTypes,
    nullptr
} };

void farmeye::PipelineController::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<PipelineController *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->frameReady((*reinterpret_cast<std::add_pointer_t<QImage>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QList<farmeye::TrackOverlay>>>(_a[2]))); break;
        case 1: _t->thumbFrameReady((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QImage>>(_a[2]))); break;
        case 2: _t->alertRaised((*reinterpret_cast<std::add_pointer_t<farmeye::AlertEvent>>(_a[1]))); break;
        case 3: _t->runningChanged((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 4: _t->statusMessage((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 5: _t->start(); break;
        case 6: _t->pause(); break;
        case 7: _t->setModelPath((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 8: { bool _r = _t->setPersonName((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])));
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 9: { QString _r = _t->personName((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])));
            if (_a[0]) *reinterpret_cast<QString*>(_a[0]) = std::move(_r); }  break;
        case 10: { QString _r = _t->peopleStorageHint();
            if (_a[0]) *reinterpret_cast<QString*>(_a[0]) = std::move(_r); }  break;
        case 11: _t->pollUiFrames(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
        case 0:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 1:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< QList<farmeye::TrackOverlay> >(); break;
            }
            break;
        case 2:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 0:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< farmeye::AlertEvent >(); break;
            }
            break;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (PipelineController::*)(const QImage & , const QVector<farmeye::TrackOverlay> & )>(_a, &PipelineController::frameReady, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (PipelineController::*)(int , const QImage & )>(_a, &PipelineController::thumbFrameReady, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (PipelineController::*)(farmeye::AlertEvent )>(_a, &PipelineController::alertRaised, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (PipelineController::*)(bool )>(_a, &PipelineController::runningChanged, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (PipelineController::*)(const QString & )>(_a, &PipelineController::statusMessage, 4))
            return;
    }
}

const QMetaObject *farmeye::PipelineController::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *farmeye::PipelineController::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN7farmeye18PipelineControllerE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int farmeye::PipelineController::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 12)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 12;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 12)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 12;
    }
    return _id;
}

// SIGNAL 0
void farmeye::PipelineController::frameReady(const QImage & _t1, const QVector<farmeye::TrackOverlay> & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1, _t2);
}

// SIGNAL 1
void farmeye::PipelineController::thumbFrameReady(int _t1, const QImage & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1, _t2);
}

// SIGNAL 2
void farmeye::PipelineController::alertRaised(farmeye::AlertEvent _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1);
}

// SIGNAL 3
void farmeye::PipelineController::runningChanged(bool _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1);
}

// SIGNAL 4
void farmeye::PipelineController::statusMessage(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 4, nullptr, _t1);
}
QT_WARNING_POP
