/****************************************************************************
** Meta object code from reading C++ file 'MainWindow.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.15.13)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../src/ui/MainWindow.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'MainWindow.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.15.13. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_MainWindow_t {
    QByteArrayData data[21];
    char stringdata0[215];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_MainWindow_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_MainWindow_t qt_meta_stringdata_MainWindow = {
    {
QT_MOC_LITERAL(0, 0, 10), // "MainWindow"
QT_MOC_LITERAL(1, 11, 12), // "onBrowseFile"
QT_MOC_LITERAL(2, 24, 0), // ""
QT_MOC_LITERAL(3, 25, 11), // "onSendStart"
QT_MOC_LITERAL(4, 37, 12), // "onSendCancel"
QT_MOC_LITERAL(5, 50, 14), // "onServerToggle"
QT_MOC_LITERAL(6, 65, 14), // "onSendProgress"
QT_MOC_LITERAL(7, 80, 1), // "t"
QT_MOC_LITERAL(8, 82, 5), // "total"
QT_MOC_LITERAL(9, 88, 11), // "onSendSpeed"
QT_MOC_LITERAL(10, 100, 3), // "bps"
QT_MOC_LITERAL(11, 104, 14), // "onSendFinished"
QT_MOC_LITERAL(12, 119, 2), // "ok"
QT_MOC_LITERAL(13, 122, 3), // "msg"
QT_MOC_LITERAL(14, 126, 12), // "onSendStatus"
QT_MOC_LITERAL(15, 139, 14), // "onRecvProgress"
QT_MOC_LITERAL(16, 154, 11), // "onRecvSpeed"
QT_MOC_LITERAL(17, 166, 14), // "onRecvFinished"
QT_MOC_LITERAL(18, 181, 12), // "onRecvStatus"
QT_MOC_LITERAL(19, 194, 15), // "onNewConnection"
QT_MOC_LITERAL(20, 210, 4) // "peer"

    },
    "MainWindow\0onBrowseFile\0\0onSendStart\0"
    "onSendCancel\0onServerToggle\0onSendProgress\0"
    "t\0total\0onSendSpeed\0bps\0onSendFinished\0"
    "ok\0msg\0onSendStatus\0onRecvProgress\0"
    "onRecvSpeed\0onRecvFinished\0onRecvStatus\0"
    "onNewConnection\0peer"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_MainWindow[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
      13,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags
       1,    0,   79,    2, 0x08 /* Private */,
       3,    0,   80,    2, 0x08 /* Private */,
       4,    0,   81,    2, 0x08 /* Private */,
       5,    0,   82,    2, 0x08 /* Private */,
       6,    2,   83,    2, 0x08 /* Private */,
       9,    1,   88,    2, 0x08 /* Private */,
      11,    2,   91,    2, 0x08 /* Private */,
      14,    1,   96,    2, 0x08 /* Private */,
      15,    2,   99,    2, 0x08 /* Private */,
      16,    1,  104,    2, 0x08 /* Private */,
      17,    2,  107,    2, 0x08 /* Private */,
      18,    1,  112,    2, 0x08 /* Private */,
      19,    1,  115,    2, 0x08 /* Private */,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::LongLong, QMetaType::LongLong,    7,    8,
    QMetaType::Void, QMetaType::Double,   10,
    QMetaType::Void, QMetaType::Bool, QMetaType::QString,   12,   13,
    QMetaType::Void, QMetaType::QString,   13,
    QMetaType::Void, QMetaType::LongLong, QMetaType::LongLong,    7,    8,
    QMetaType::Void, QMetaType::Double,   10,
    QMetaType::Void, QMetaType::Bool, QMetaType::QString,   12,   13,
    QMetaType::Void, QMetaType::QString,   13,
    QMetaType::Void, QMetaType::QString,   20,

       0        // eod
};

void MainWindow::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<MainWindow *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->onBrowseFile(); break;
        case 1: _t->onSendStart(); break;
        case 2: _t->onSendCancel(); break;
        case 3: _t->onServerToggle(); break;
        case 4: _t->onSendProgress((*reinterpret_cast< qint64(*)>(_a[1])),(*reinterpret_cast< qint64(*)>(_a[2]))); break;
        case 5: _t->onSendSpeed((*reinterpret_cast< double(*)>(_a[1]))); break;
        case 6: _t->onSendFinished((*reinterpret_cast< bool(*)>(_a[1])),(*reinterpret_cast< QString(*)>(_a[2]))); break;
        case 7: _t->onSendStatus((*reinterpret_cast< QString(*)>(_a[1]))); break;
        case 8: _t->onRecvProgress((*reinterpret_cast< qint64(*)>(_a[1])),(*reinterpret_cast< qint64(*)>(_a[2]))); break;
        case 9: _t->onRecvSpeed((*reinterpret_cast< double(*)>(_a[1]))); break;
        case 10: _t->onRecvFinished((*reinterpret_cast< bool(*)>(_a[1])),(*reinterpret_cast< QString(*)>(_a[2]))); break;
        case 11: _t->onRecvStatus((*reinterpret_cast< QString(*)>(_a[1]))); break;
        case 12: _t->onNewConnection((*reinterpret_cast< QString(*)>(_a[1]))); break;
        default: ;
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject MainWindow::staticMetaObject = { {
    QMetaObject::SuperData::link<QMainWindow::staticMetaObject>(),
    qt_meta_stringdata_MainWindow.data,
    qt_meta_data_MainWindow,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *MainWindow::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *MainWindow::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_MainWindow.stringdata0))
        return static_cast<void*>(this);
    return QMainWindow::qt_metacast(_clname);
}

int MainWindow::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QMainWindow::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 13)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 13;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 13)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 13;
    }
    return _id;
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
