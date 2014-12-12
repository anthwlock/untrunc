#-------------------------------------------------
#
# Project created by QtCreator 2012-10-28T12:50:54
#
#-------------------------------------------------

QT       -= core
QT       -= gui

TARGET = untrunc
CONFIG   += console
CONFIG   -= -qt app_bundle


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

INCLUDEPATH += -I/usr/local/lib
LIBS += -L/usr/local/lib -lavformat -lavcodec -lavutil
DEFINES += _FILE_OFFSET_BITS=64

#INCLUDEPATH += -I../libav-0.8.7/libavformat -I../libav-0.8.7/libavcodec -I../libav-0.8.7/libavutil
#LIBS += ../libav-0.8.7/libavformat/libavformat.a ../libav-0.8.7/libavcodec/libavcodec.a ../libav-0.8.7/libavutil/libavutil.a
LIBS += -lz

#QMAKE_LFLAGS += -static
#LIBS += /usr/lib/x86_64-linux-gnu/libavcodec.a \
#        /usr/lib/x86_64-linux-gnu/libavformat.a \
#        /usr/lib/x86_64-linux-gnu/libavutil.a
