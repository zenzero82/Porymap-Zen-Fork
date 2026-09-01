QT += charts network qml openglwidgets widgets gui core
CONFIG += console c++17 strict_c++
CONFIG -= app_bundle
TEMPLATE = app
TARGET = smartpath_integration_test
INCLUDEPATH += \
    $$PWD/../../include \
    $$PWD/../../include/core \
    $$PWD/../../include/ui \
    $$PWD/../../include/lib
SOURCES += $$PWD/smartpath_integration_test.cpp
APP_OBJECTS = $$files($$PWD/../../*.o)
APP_OBJECTS -= $$PWD/../../main.o
OBJECTS += $$APP_OBJECTS