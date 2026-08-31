QT += gui

CONFIG += console c++17 strict_c++
CONFIG -= app_bundle

TEMPLATE = app
TARGET = porytilesprocess_test

INCLUDEPATH += $$PWD/../../include

SOURCES += \
    $$PWD/porytilesprocess_test.cpp \
    $$PWD/../../src/studio/porytilesprocess.cpp

HEADERS += $$PWD/../../include/studio/porytilesprocess.h