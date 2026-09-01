QT += core
CONFIG += console c++17 strict_c++
CONFIG -= app_bundle
TEMPLATE = app
TARGET = terrainrules_test
INCLUDEPATH += $$PWD/../../include
SOURCES += \
    $$PWD/terrainrules_test.cpp \
    $$PWD/../../src/studio/terrainrules.cpp
HEADERS += $$PWD/../../include/studio/terrainrules.h