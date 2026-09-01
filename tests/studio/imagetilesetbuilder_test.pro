QT += gui widgets

CONFIG += console c++17 strict_c++
CONFIG -= app_bundle

TEMPLATE = app
TARGET = imagetilesetbuilder_test

INCLUDEPATH += \
    $$PWD/../../include \
    $$PWD/../../include/core \
    $$PWD/../../include/ui \
    $$PWD/../../include/lib

SOURCES += \
    $$PWD/imagetilesetbuilder_test.cpp \
    $$PWD/../../src/studio/assettilesetbuilder.cpp \
    $$PWD/../../src/studio/imagetilesetbuilder.cpp

HEADERS += \
    $$PWD/../../include/studio/assettilesetbuilder.h \
    $$PWD/../../include/studio/imagetilesetbuilder.h