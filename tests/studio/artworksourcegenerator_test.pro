QT += gui widgets

CONFIG += console c++17 strict_c++
CONFIG -= app_bundle

TEMPLATE = app
TARGET = artworksourcegenerator_test

INCLUDEPATH += \
    $$PWD/../../include \
    $$PWD/../../include/core

SOURCES += \
    $$PWD/artworksourcegenerator_test.cpp \
    $$PWD/../../src/studio/artworksourcegenerator.cpp

HEADERS += \
    $$PWD/../../include/studio/artworksourcegenerator.h