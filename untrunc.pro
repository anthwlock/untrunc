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

GIT_VERSION = $$system(git describe --always --dirty --abbrev=7)
DEFINES += UNTR_VERSION=\\\"$$GIT_VERSION\\\"

TEMPLATE = app

SOURCES += $$files(src/*.cpp)
HEADERS += $$files(src/*.h)

LIBS += -lavformat -lavcodec -lavutil
