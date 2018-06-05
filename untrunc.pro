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


TEMPLATE = app

SOURCES += main.cpp \
    atom.cpp \
    mp4.cpp \
    file.cpp \
    track.cpp \
    codec.cpp \
    common.cpp \
    nal.cpp \
    nal-sps.cpp \
    nal-slice.cpp

HEADERS += \
    atom.h \
    mp4.h \
    file.h \
    track.h \
    codec.h \
    common.h \
    nal.h \
    nal-sps.h \
    nal-slice.h

LIBS += -lavformat -lavcodec -lavutil
