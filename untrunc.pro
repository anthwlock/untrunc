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


TEMPLATE = app

SOURCES += main.cpp \
    atom.cpp \
    mp4.cpp \
    file.cpp \
    track.cpp

HEADERS += \
    atom.h \
    mp4.h \
    file.h \
    track.h

INCLUDEPATH += ../libav-12.2
LIBS += ../libav-12.2/libavformat/libavformat.a \
../libav-12.2/libavcodec/libavcodec.a \
../libav-12.2/libavutil/libavutil.a \
../libav-12.2/libavresample/libavresample.a


#INCLUDEPATH += -I/usr/local/lib
#LIBS += -L/usr/local/lib -lavformat -lavcodec -lavutil
DEFINES += _FILE_OFFSET_BITS=64 VERBOSE VERBOSE1

LIBS += -lz

#QMAKE_LFLAGS += -static
#LIBS += /usr/lib/x86_64-linux-gnu/libavcodec.a \
#        /usr/lib/x86_64-linux-gnu/libavformat.a \
#        /usr/lib/x86_64-linux-gnu/libavutil.a
