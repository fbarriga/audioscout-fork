/*
    Audio Scout - audio content indexing software
    Copyright (C) 2010  D. Grant Starkweather & Evan Klinger
    
    Audio Scout is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    D. Grant Starkweather - dstarkweather@phash.org
    Evan Klinger          - eklinger@phash.org
*/

#ifndef _MAINWINDOW_H_
#define _MAINWINDOW_H_
#include <QtGui>
#include <QtCore>
#include <QtMultimediaKit/QtMultimediaKit>
#include <zmq.hpp>
#include "SendThread.h"
#include "MeterWidget.h"

class MainWindow : public QMainWindow {

    Q_OBJECT

public:

    MainWindow(QWidget *parent=0);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *event);

private slots:
    void connectToServer();
    void browseforfile();
    void about();
    void submit();
    void changeToQueryMode();
    void changeToSubmitMode();
    void sampleAudio();
    void readSamples();
    void appendPlainText(QString text);
    void changeProgressBarLevel(int level);
    void changeProgressBarRange(int min, int max);
    void activateProgressBar(bool visible);
    void finishWithTask();
    void postError(QString errorString);
    void changeSampleInterval();
    void changeGain();
    void clearDisplay();
    void audioStateChanged(QAudio::State newState);

private:
    /* functions */
    void createActions();
    void createMenus();
    void readSettings();
    void writeSettings();

    zmq::context_t *ctx; //zeromq context 

    QStringList files;
    QString serverAddress;
    QList<QAudioDeviceInfo> deviceList;
    int sample_duration;
    float signal_gain;

    SelectionMode mode;

    /* widgets */
    QPushButton *queryButton;
    QWidget *mainWidget;
    QProgressBar *progressBar;
    QComboBox *fileComboBox, *devComboBox;
    QPlainTextEdit *displayText;
    MeterWidget *meter;
    
    QMenu *connectMenu, *aboutMenu;
    QAction *connectAct, *aboutAct, *setSampleIntervalAct, *setGainAct;

    SendThread *sendThread;

    QAudioInput *audioInput;
    QIODevice *ioDev;
    QVector<float> *sampledData;
};

#endif /* _MAINWINDOW_H_ */ 
