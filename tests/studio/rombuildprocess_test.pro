QT += core
CONFIG += console c++17 strict_c++
CONFIG -= app_bundle
TEMPLATE = app
TARGET = rombuildprocess_test
INCLUDEPATH += $$PWD/../../include
SOURCES += \
    $$PWD/rombuildprocess_test.cpp \
    $$PWD/../../src/studio/rombuildprocess.cpp
HEADERS += $$PWD/../../include/studio/rombuildprocess.h