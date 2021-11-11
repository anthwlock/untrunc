#-------------------------------------------------
#
# Project created by QtCreator 2012-10-28T12:50:54
#
#-------------------------------------------------

QT -= core
QT -= gui

TARGET = untrunc
CONFIG += console
CONFIG -= -qt app_bundle
CONFIG += debug

CONFIG += object_parallel_to_source

GIT_VERSION = $$system(echo "v`git rev-list --count HEAD`-`git describe --always --dirty --abbrev=7`")
DEFINES += UNTR_VERSION=\\\"$$GIT_VERSION\\\"

TEMPLATE = app

SOURCES += $$files(src/*.cpp) $$files(src/avc1/*.cpp) $$files(src/hvc1/*.cpp)
HEADERS += $$files(src/*.h) $$files(src/avc1/*.h) $$files(src/hvc1/*.h)

LIBS += -lavformat -lavcodec -lavutil
