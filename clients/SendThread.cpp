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

#include <string.h>
#include "serialize.h"
#include "SendThread.h"

extern "C" {
    #include "audiodata.h"
    #include "phash_audio.h"
}

SendThread::SendThread(QObject *parent):QThread(parent){
    fields << "composer" << "title"    << "performer"\
           << "date"     << "album"    << "genre"\
           << "year"     << "duration" << "part";

    context = NULL;
    skt = NULL;
    nbsecs = 0.0f;
}
SendThread::~SendThread(){

}

void SendThread::startWithArgs(SelectionMode m, zmq::context_t *ctx, QString address, \
                                   QStringList &files, float seconds){
    mode = m;
    if (m != QUERY_MODE && m != SUBMIT_MODE) {
	qDebug() << "wrong mode arg value, " << m;
	return;
    }
    context = ctx;
    serverAddress.clear();
    serverAddress.append(address);
    nbsecs = seconds;
    fileList.clear();
    fileList.append(files);
    hashArray = NULL;
    hashlen = 0;
    start();
}

void SendThread::startWithArgs(SelectionMode m, zmq::context_t *ctx, QString address,\
			       quint32 *hash, quint32 nbframes){
    mode = m;
    if (m != SAMPLE_MODE) {
	qDebug() << "wrong mode arg value, " << m;
	return;
    }
    context = ctx;
    serverAddress.clear();
    serverAddress.append(address);
    hashArray = hash;
    hashlen = nbframes;
    start();
}


static const char delim[2] = {0x1E, 0x00};
static const char space = 32;

static void free_func(void *data, void *hint){
    Q_UNUSED(hint);
    free(data);
}

void SendThread::run(){

    /* connect to server */
    try {
	skt = new zmq::socket_t(*context, ZMQ_REQ);
	skt->connect(serverAddress.toUtf8().data());
    } catch (zmq::error_t err){
	QString errorString = QString("unable to connect: %1").arg(err.what());
	qDebug() << "failed to connect skt: " << errorString;
	emit postedError(errorString);
	return;
    }

    if (hashArray && hashlen){
	doSendSample();
    } else {
	doSend();
    }

    delete skt;

    emit appendedText(tr("Done."));
    emit doneWithTask();
}

void SendThread::doSend(){        	

    quint8 cmd;
    if (mode == QUERY_MODE) {
	cmd = 1;
    } else if (mode == SUBMIT_MODE){
	cmd = 2;
    } else{
	return;
    }

    QString delimStr(delim);

    emit activatedProgress(true);
    emit changedRange(0, fileList.size());
    emit changedLevel(0);

    int error;
    float *sigbuf = new float[1<<24];
    const unsigned int buflen = 1<<24;

    quint32 nbframes = 0, snbframes = 0, id = 0;
    char *data = NULL;
    char mdata_inlinestr[512];
    AudioHashStInfo hash_st;
    hash_st.sr = 0;
    int index = 0;

    AudioMetaData mdata;
    init_mdata(&mdata);
    foreach(QString currentFile, fileList){
	emit appendedText(tr("looking up ") + currentFile);

	unsigned int tmpbuflen = buflen;
	char *file = currentFile.toLocal8Bit().data();

	float *buf = readaudio(file, 6000, sigbuf, &tmpbuflen, nbsecs, &mdata, &error);
	if (!buf){
	    QString errorString = QString("unable to read audio: err code %1").arg(error);
	    qDebug() << "could not read file: " << errorString;
	    emit postedError(errorString);
	    continue;
	}

	quint32 *hash = NULL;

	int res = audiohash(buf,&hash,NULL,NULL,NULL,&nbframes,NULL,NULL,\
			    tmpbuflen,0,6000,&hash_st);
	if (res < 0){
	    QString errorString = QString("unable to extract a hash");
	    emit postedError(errorString);
	    qDebug() << "could not get hash: " << errorString.toUtf8().data();
	    if (buf != sigbuf) ph_free(buf);
	    continue;
	}
	
	try {
	    zmq::message_t cmdmsg(&cmd, sizeof(quint8), NULL);
	    snbframes = hosttonet32(nbframes);
	    zmq::message_t framesmsg(&snbframes, sizeof(quint32), NULL);
	    zmq::message_t hashmsg(hash, nbframes*sizeof(quint32), free_func);
	    skt->send(cmdmsg, ZMQ_SNDMORE);
	    skt->send(framesmsg, ZMQ_SNDMORE);
	    
	    if (cmd == 1){ //query
		skt->send(hashmsg, 0);
	    } else {       //query
		skt->send(hashmsg, ZMQ_SNDMORE);
		metadata_to_inlinestr(&mdata, mdata_inlinestr, 512);
		zmq::message_t metadata_msg(mdata_inlinestr, strlen(mdata_inlinestr)+1, NULL);
		skt->send(metadata_msg, 0);
	    }



	    zmq::message_t respmsg;
	    skt->recv(&respmsg, 0);
	    data = (char*)respmsg.data();

	    QString retdString(data);
	    
	    if (cmd == 1){
		QStringList resultList = retdString.split(delimStr);

		int i = 0;
		foreach (QString resultStr, resultList){
		    if (i < fields.size()){
			emit appendedText(fields[i] + ": " + resultStr);
		    }
		    i++;
		}
		resultList.clear();
	    } else if (cmd == 2){
		if (respmsg.size()  != sizeof(quint32)) {
		    QString errorString = QString("recieved msg of incorrectsize, %1").arg(respmsg.size());
		    emit postedError(errorString);
		    if (buf != sigbuf) ph_free(buf);
		    continue;
		}
		memcpy(&id, data, sizeof(quint32));
		id = nettohost32(id);
		QString line = QString("assigned id = %1").arg(id);
		emit appendedText(line);
	    } 
	} catch (zmq::error_t err){
	    QString errorString = QString("unable to send: %1").arg(err.what());
	    qDebug() << "unable to send " << errorString;
	    emit postedError(errorString);
	    continue;
	}

	emit changedLevel(++index);

	if (buf != sigbuf) ph_free(buf);
	free_mdata(&mdata);
    }

    ph_hashst_free(&hash_st);
    delete sigbuf;

    emit activatedProgress(false);

}

void SendThread::doSendSample(){
    QString delimStr(delim);
    quint8 cmd = 1;
    char *data = NULL;
    quint32 shashlen = hosttonet32(hashlen);

    for (quint32 i=0;i<hashlen;i++){
	hashArray[i] = hosttonet32(hashArray[i]);
    }

    try {
	zmq::message_t cmdmsg(&cmd, sizeof(quint8), NULL);
	zmq::message_t framesmsg(&shashlen, sizeof(quint32), NULL);
	zmq::message_t hashmsg(hashArray, hashlen*sizeof(quint32), free_func);
	skt->send(cmdmsg, ZMQ_SNDMORE);
	skt->send(framesmsg, ZMQ_SNDMORE);
	skt->send(hashmsg, 0);
	    
	zmq::message_t respmsg;
	skt->recv(&respmsg, 0);
	data = (char*)respmsg.data();
	    
	QString retdStr(data);
	QStringList resultList;
	resultList.append(retdStr.split(delimStr));

	int i = 0;
	foreach (QString resultStr, resultList){
	    if (i < fields.size()){
		emit appendedText(fields[i] + ": " + resultStr);
	    }
	    i++;
	}

	resultList.clear();

    } catch (zmq::error_t e){
	QString errStr = QString("unable to send message: %1").arg(e.what());
	emit postedError(errStr);
    }

}
