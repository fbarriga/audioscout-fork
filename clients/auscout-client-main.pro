CONFIG += qt
QT += multimedia
LIBS += -L/usr/local/lib -L. -lzmq -lAudioData -lpHashAudio
INCLUDEPATH += /usr/include/QtMultimediaKit
INCLUDEPATH += /usr/local/include/QtMultimediaKit
HEADERS = mainwindow.h SendThread.h audiodata.h phash_audio.h MeterWidget.h
SOURCES = auscout-client-main.cpp mainwindow.cpp SendThread.cpp MeterWidget.cpp
TARGET = auscout

