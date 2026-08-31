QT += gui

CONFIG += console c++17 strict_c++
CONFIG -= app_bundle

TEMPLATE = app
TARGET = imagemetatilematcher_test

INCLUDEPATH += \
    $$PWD/../../include \
    $$PWD/../../include/core \
    $$PWD/../../include/ui

SOURCES += \
    $$PWD/imagemetatilematcher_test.cpp \
    $$PWD/../../src/studio/imagemetatileapproval.cpp \
    $$PWD/../../src/studio/imagemetatilematcher.cpp

HEADERS += \
    $$PWD/../../include/studio/imagemetatilematcher.h \
    $$PWD/../../include/studio/imagemetatileapproval.h \
    $$PWD/../../include/studio/metatilerenderservice.h