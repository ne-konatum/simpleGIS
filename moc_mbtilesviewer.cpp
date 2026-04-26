/****************************************************************************
** Meta object code from reading C++ file 'mbtilesviewer.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.15.8)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "mbtilesviewer.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'mbtilesviewer.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.15.8. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_MBTilesViewer_t {
    QByteArrayData data[11];
    char stringdata0[93];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_MBTilesViewer_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_MBTilesViewer_t qt_meta_stringdata_MBTilesViewer = {
    {
QT_MOC_LITERAL(0, 0, 13), // "MBTilesViewer"
QT_MOC_LITERAL(1, 14, 10), // "tileLoaded"
QT_MOC_LITERAL(2, 25, 0), // ""
QT_MOC_LITERAL(3, 26, 1), // "z"
QT_MOC_LITERAL(4, 28, 1), // "x"
QT_MOC_LITERAL(5, 30, 1), // "y"
QT_MOC_LITERAL(6, 32, 3), // "img"
QT_MOC_LITERAL(7, 36, 24), // "cursorCoordinatesChanged"
QT_MOC_LITERAL(8, 61, 9), // "longitude"
QT_MOC_LITERAL(9, 71, 8), // "latitude"
QT_MOC_LITERAL(10, 80, 12) // "onTileLoaded"

    },
    "MBTilesViewer\0tileLoaded\0\0z\0x\0y\0img\0"
    "cursorCoordinatesChanged\0longitude\0"
    "latitude\0onTileLoaded"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_MBTilesViewer[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
       3,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       2,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    4,   29,    2, 0x06 /* Public */,
       7,    2,   38,    2, 0x06 /* Public */,

 // slots: name, argc, parameters, tag, flags
      10,    4,   43,    2, 0x08 /* Private */,

 // signals: parameters
    QMetaType::Void, QMetaType::Int, QMetaType::Int, QMetaType::Int, QMetaType::QImage,    3,    4,    5,    6,
    QMetaType::Void, QMetaType::Double, QMetaType::Double,    8,    9,

 // slots: parameters
    QMetaType::Void, QMetaType::Int, QMetaType::Int, QMetaType::Int, QMetaType::QImage,    3,    4,    5,    6,

       0        // eod
};

void MBTilesViewer::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<MBTilesViewer *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->tileLoaded((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2])),(*reinterpret_cast< int(*)>(_a[3])),(*reinterpret_cast< const QImage(*)>(_a[4]))); break;
        case 1: _t->cursorCoordinatesChanged((*reinterpret_cast< double(*)>(_a[1])),(*reinterpret_cast< double(*)>(_a[2]))); break;
        case 2: _t->onTileLoaded((*reinterpret_cast< int(*)>(_a[1])),(*reinterpret_cast< int(*)>(_a[2])),(*reinterpret_cast< int(*)>(_a[3])),(*reinterpret_cast< const QImage(*)>(_a[4]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (MBTilesViewer::*)(int , int , int , const QImage & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&MBTilesViewer::tileLoaded)) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (MBTilesViewer::*)(double , double );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&MBTilesViewer::cursorCoordinatesChanged)) {
                *result = 1;
                return;
            }
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject MBTilesViewer::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_meta_stringdata_MBTilesViewer.data,
    qt_meta_data_MBTilesViewer,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *MBTilesViewer::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *MBTilesViewer::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_MBTilesViewer.stringdata0))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int MBTilesViewer::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 3)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 3;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 3)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 3;
    }
    return _id;
}

// SIGNAL 0
void MBTilesViewer::tileLoaded(int _t1, int _t2, int _t3, const QImage & _t4)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t3))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t4))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void MBTilesViewer::cursorCoordinatesChanged(double _t1, double _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
