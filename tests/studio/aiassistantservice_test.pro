QT += core network
CONFIG += console c++17
CONFIG -= app_bundle
TEMPLATE = app

INCLUDEPATH += ../../../include

SOURCES += \
    aiassistantservice_test.cpp \
    ../../../src/studio/aiassistantservice.cpp

HEADERS += \
    ../../../include/studio/aiassistantservice.h