QT += gui
CONFIG += console c++17 strict_c++
CONFIG -= app_bundle
TEMPLATE = app
TARGET = smartcollisionsuggester_test
INCLUDEPATH += $$PWD/../../include
SOURCES += \
    $$PWD/smartcollisionsuggester_test.cpp \
    $$PWD/../../src/studio/smartcollisionsuggester.cpp
HEADERS += $$PWD/../../include/studio/smartcollisionsuggester.h