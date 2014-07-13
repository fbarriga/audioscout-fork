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

#ifndef _SENDTHREAD_H
#define _SENDTHREAD_H

#include <QtCore>
#include <QThread>
#include <zmq.hpp>

    
enum SelectionMode { QUERY_MODE = 1, SUBMIT_MODE, SAMPLE_MODE };

class SendThread : public QThread {

    Q_OBJECT

 public:
    SendThread(QObject *parent = 0);
    ~SendThread();
    void startWithArgs(SelectionMode m,zmq::context_t *ctx,QString address,\
                             QStringList &files, float seconds);

    void startWithArgs(SelectionMode m, zmq::context_t *ctx,QString address,\
		       quint32 *hash, quint32 nbframes);
 signals:
    void appendedText(QString string);
    void changedLevel(int level);
    void changedRange(int min, int max);
    void activatedProgress(bool visible);
    void postedError(QString errorString);
    void doneWithTask();

 protected:
    void run();
    void doSendSample();
    void doSend();

 private:
    zmq::context_t *context;
    zmq::socket_t *skt;

    QString serverAddress;
    QStringList fileList;
    QStringList fields;

    float nbsecs;

    SelectionMode mode;

    quint32 *hashArray;
    quint32 hashlen;
};

#endif // _SENDTHREAD_H 
