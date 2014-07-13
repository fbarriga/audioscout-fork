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

#include "math.h"
#include "mainwindow.h"

extern "C" {
    #include "serialize.h"
    #include "phash_audio.h"
}

MainWindow::MainWindow(QWidget *parent):QMainWindow(parent){
    /* create widgets */
    mainWidget = new QWidget();
    setWindowTitle("AudioScout");
    resize(650,400);
    
    createActions();
    createMenus();
    readSettings();

    QVBoxLayout *mainLayout = new QVBoxLayout;
    QHBoxLayout *controlLayout = new QHBoxLayout;

    QGroupBox *connectGroup = new QGroupBox(tr("Set Connection to Server"));
    QHBoxLayout *connectLayout = new QHBoxLayout;
    QPushButton *connectButton = new QPushButton("&Connect");
    connectLayout->addSpacing(25);
    connectLayout->addWidget(connectButton);
    connectLayout->addSpacing(25);
    connectGroup->setLayout(connectLayout);
    connectGroup->setAlignment(Qt::AlignCenter);
    connect(connectButton, SIGNAL(clicked()), connectAct, SLOT(trigger()));

    QGroupBox *sampleGroup = new QGroupBox(tr("Sample from Microphone"));
    QGridLayout *sampleLayout = new QGridLayout;
    QLabel *deviceLabel = new QLabel(tr("device:"));
    devComboBox = new QComboBox(this);
    QPushButton *sampleButton = new QPushButton(tr("&Sample"));
    QLabel *micLabel = new QLabel;
    meter = new MeterWidget(this);
    meter->setVisible(true);
    micLabel->setPixmap(QIcon("Microphone-icon.png").pixmap(64));
    sampleLayout->addWidget(deviceLabel,  0, 0, Qt::AlignCenter);
    sampleLayout->addWidget(devComboBox,  0, 1, Qt::AlignHCenter);
    sampleLayout->addWidget(sampleButton, 0, 2, Qt::AlignHCenter);
    sampleLayout->addWidget(micLabel,     1, 0,  1, -1, Qt::AlignHCenter);
    sampleLayout->addWidget(meter,        2, 0,  1, -1, Qt::AlignCenter); 
    sampleGroup->setLayout(sampleLayout);
    sampleGroup->setAlignment(Qt::AlignCenter);
    controlLayout->addWidget(connectGroup);
    controlLayout->addWidget(sampleGroup);

    mainLayout->addItem(controlLayout);

    connect(sampleButton, SIGNAL(clicked()), this, SLOT(sampleAudio()));

    deviceList.append(QAudioDeviceInfo::availableDevices(QAudio::AudioInput));
    foreach (QAudioDeviceInfo info, deviceList){
	devComboBox->addItem(info.deviceName(), QVariant::fromValue(info));
    }

    QGridLayout *gridSelectionLayout = new QGridLayout;

    QGroupBox *modeGroup = new QGroupBox(tr("Selection Mode"));
    modeGroup->setAlignment(Qt::AlignCenter);
    QRadioButton *queryRadioButton = new QRadioButton(tr("&query"));
    QRadioButton *submitRadioButton = new QRadioButton(tr("&submit"));
    queryRadioButton->setChecked(true);
    mode = QUERY_MODE;
    QVBoxLayout *modeLayout = new QVBoxLayout;
    modeLayout->addWidget(queryRadioButton);
    modeLayout->addWidget(submitRadioButton);
    modeLayout->addStretch(1);
    modeGroup->setLayout(modeLayout);

    connect(queryRadioButton, SIGNAL(clicked()), this, SLOT(changeToQueryMode()));
    connect(submitRadioButton, SIGNAL(clicked()), this, SLOT(changeToSubmitMode()));

    QLabel *fileLabel = new QLabel(tr("Find Files"));
    fileComboBox = new QComboBox();
    fileComboBox->setLineEdit(new QLineEdit);
    fileComboBox->setFrame(false);
    fileComboBox->setMinimumContentsLength(32);
    QPushButton *browseFileButton = new QPushButton(tr("&browse..."));
    queryButton = new QPushButton("&Hash && Send");
    gridSelectionLayout->addWidget(modeGroup, 0, 0, Qt::AlignLeft);
    gridSelectionLayout->addWidget(fileLabel, 0, 1, Qt::AlignCenter);
    gridSelectionLayout->addWidget(fileComboBox, 0, 2, 0, 3, Qt::AlignCenter);
    gridSelectionLayout->addWidget(browseFileButton, 0, 5, Qt::AlignCenter);
    gridSelectionLayout->addWidget(queryButton, 0, 6, Qt::AlignCenter);

    connect(browseFileButton, SIGNAL(clicked()), this, SLOT(browseforfile()));

    QGroupBox *selectionGroupBox = new QGroupBox(tr("Select and Hash Files"));
    selectionGroupBox->setAlignment(Qt::AlignCenter);
    selectionGroupBox->setLayout(gridSelectionLayout);

    mainLayout->addWidget(selectionGroupBox);

    connect(queryButton, SIGNAL(clicked()), this, SLOT(submit()));

    QGroupBox *displayBox = new QGroupBox(tr("Display Results"));

    QVBoxLayout *displayLayout = new QVBoxLayout;
    displayText = new QPlainTextEdit;
    displayText->setReadOnly(true);
    progressBar = new QProgressBar;
    progressBar->setVisible(false);
    displayLayout->addWidget(displayText);
    displayLayout->addWidget(progressBar);
    displayBox->setLayout(displayLayout);
    displayBox->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(displayBox);

    QPushButton *clearDisplayButton = new QPushButton(tr("&Clear Display Text"));
    displayLayout->addWidget(clearDisplayButton);
    connect(clearDisplayButton,SIGNAL(clicked()), this, SLOT(clearDisplay()));
    
    setCentralWidget(mainWidget);
    mainWidget->setLayout(mainLayout);
    show();

    try {
	ctx = new zmq::context_t(1);
    } catch (zmq::error_t err){
	QMessageBox::critical(this,tr("AudioScout Error"),tr(err.what()) ,QMessageBox::Ok);
    }

    sendThread = new SendThread();
    connect(sendThread,SIGNAL(appendedText(QString)), this, SLOT(appendPlainText(QString)));
    connect(sendThread,SIGNAL(changedLevel(int)), this, SLOT(changeProgressBarLevel(int)));
    connect(sendThread,SIGNAL(changedRange(int, int)),this,SLOT(changeProgressBarRange(int,int)));
    connect(sendThread,SIGNAL(activatedProgress(bool)), this, SLOT(activateProgressBar(bool)));
    connect(sendThread,SIGNAL(postedError(QString)), this, SLOT(appendPlainText(QString)));
    connect(sendThread,SIGNAL(doneWithTask()), this, SLOT(finishWithTask()));
    connect(sendThread,SIGNAL(postedError(QString)), this, SLOT(postError(QString)));

    audioInput = NULL;

    return;
}
MainWindow::~MainWindow(){}

void MainWindow::connectToServer(){
    bool ok;

    QString text = QInputDialog::getText(this, tr("Server Address"), tr("address:"),\
					 QLineEdit::Normal,serverAddress, &ok);

    if (ok && !text.isEmpty()){
	try {
	    zmq::socket_t skt(*ctx, ZMQ_REQ);
	    skt.connect(text.toUtf8().data());
	    serverAddress.clear();
	    serverAddress.append(text);
	    QString msgstr = QString("connected to ") + serverAddress;
	    QMessageBox::information(this,tr("AudioScout Connection"), msgstr, QMessageBox::Ok);
	} catch (zmq::error_t err){
	    QString errstr = QString(err.what());
	    QString msgstr = QString("error %1").arg(errstr);
	    QMessageBox::warning(this,tr("AudioScout Error"), msgstr, QMessageBox::Ok);
	}
    }
}

void MainWindow::changeToQueryMode(){
    mode = QUERY_MODE;
    fileComboBox->lineEdit()->clear();
    fileComboBox->clear();
    files.clear();
}
void MainWindow::changeToSubmitMode(){
    mode = SUBMIT_MODE;
    fileComboBox->lineEdit()->clear();
    fileComboBox->clear();
    files.clear();
}
void MainWindow::about(){
    QMessageBox msgbox(this);
    msgbox.setWindowTitle("AudioScout");
    msgbox.setText("Aetilius, Inc.");
    msgbox.setInformativeText("AudioScout content indexing application");
    msgbox.setTextFormat(Qt::RichText);
    msgbox.exec();
}
void MainWindow::createActions(){
    connectAct = new QAction(tr("&connect"), this);
    connect(connectAct, SIGNAL(triggered()), this , SLOT(connectToServer()));
    aboutAct = new QAction(tr("&about"), this);
    setSampleIntervalAct = new QAction("&Sample Interval", this);
    setGainAct = new QAction(tr("&Gain"), this);
    connect(setSampleIntervalAct, SIGNAL(triggered()), this, SLOT(changeSampleInterval()));
    connect(aboutAct, SIGNAL(triggered()), this, SLOT(about()));
    connect(setGainAct, SIGNAL(triggered()), this, SLOT(changeGain()));
}
void MainWindow::createMenus(){
    connectMenu = menuBar()->addMenu(tr("&Settings"));
    connectMenu->addAction(connectAct);
    connectMenu->addAction(setSampleIntervalAct);
    connectMenu->addAction(setGainAct);
    aboutMenu = menuBar()->addMenu(tr("&about"));
    aboutMenu->addAction(aboutAct);
}
void MainWindow::changeSampleInterval(){
    bool ok;
    int sd = QInputDialog::getInt(this, tr("Sample Interval"),tr("sample time (secs):"),\
					sample_duration, 0, 120, 1, &ok);
    if (ok){
	sample_duration = sd;
    }
}
void MainWindow::changeGain(){
    bool ok;
    double gain = QInputDialog::getDouble(this, tr("Set Amplitude Gain"), tr("amp gain"),\
					 signal_gain, 1.0, 10.0, 2, &ok);
    if (ok){
	signal_gain = (float)gain;
    }
}

void MainWindow::browseforfile(){
    files.clear();
    fileComboBox->lineEdit()->clear();
    fileComboBox->clear();

    QFileDialog dialog(this);
    dialog.setDirectory("~/");
    dialog.setViewMode(QFileDialog::List);
    
    QStringList filters;
    filters << "Audio Files (*.mp3 *.wav *.ogg *.flac *.aiff)";
    dialog.setNameFilters(filters);
    dialog.setFileMode(QFileDialog::ExistingFiles);
    if (dialog.exec()) {
	files.clear();
	QStringList results  = dialog.selectedFiles();
	files.append(results);
	fileComboBox->clear();
	fileComboBox->addItems(files);
	if (files.size() > 0) fileComboBox->lineEdit()->setText(files.front());
    }
}

void MainWindow::readSettings(){

    QSettings settings("Aetilius", "AudioScout");
    settings.beginGroup("MainWindow");
    move(settings.value("pos", QPoint(200,200)).toPoint());
    resize(settings.value("size", QSize(600, 450)).toSize());
    serverAddress.clear();
    serverAddress.append(settings.value("server").toString());
    sample_duration = settings.value("duration", 15).toInt();
    signal_gain = settings.value("gain", 1.0).toFloat();
    settings.endGroup();

}
void MainWindow::writeSettings(){
    QSettings settings("Aetilius", "AudioScout");

    settings.beginGroup("MainWindow");
    settings.setValue("pos", pos());
    settings.setValue("size", size());
    settings.setValue("server", QVariant(serverAddress));
    settings.setValue("duration", sample_duration);
    settings.setValue("gain", signal_gain);
    settings.endGroup();
}

void MainWindow::submit(){
    if (!sendThread->isRunning()){
	if (!files.empty() && !serverAddress.isNull() && !serverAddress.isEmpty()){
	    displayText->clear();
	    float nbsecs = (mode == QUERY_MODE) ? 30.0f : 0.0f;
	    sendThread->startWithArgs(mode, ctx, serverAddress, files, nbsecs);
	    queryButton->setText("&Stop");
	} 
    } else {
	int ret = QMessageBox::warning(this, tr("AudioScout Warning"),\
                  "Are you sure? (This is unsafe way to terminate the action. Use with caution", 
			     QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

	if (ret == QMessageBox::Yes){
	    sendThread->terminate();
	    queryButton->setText("&Hash && Send");
	    progressBar->setVisible(false);
	}
    } 
}

void MainWindow::sampleAudio(){
    if (!sendThread->isRunning()){
	if (!audioInput){
	    QAudioFormat format;
	    format.setFrequency(6000);
	    format.setChannels(1);
	    format.setSampleSize(16);
	    format.setCodec("audio/pcm");
	    format.setByteOrder(QAudioFormat::LittleEndian);
	    format.setSampleType(QAudioFormat::SignedInt);

	    int currentIndex = devComboBox->currentIndex();
	    QVariant var = devComboBox->itemData(currentIndex);
	    QAudioDeviceInfo info = var.value<QAudioDeviceInfo>();
	    if (!info.isFormatSupported(format)){
		QMessageBox::warning(this, tr("Auscout warning"),\
				     tr("format not supported - aborting"));
		return;
	    }
	    
	    audioInput = new QAudioInput(info, format, this);
	    connect(audioInput,SIGNAL(stateChanged(QAudio::State)),\
                                 this, SLOT(audioStateChanged(QAudio::State)));
	}
    } else {
	QMessageBox::warning(this,tr("Auscout Warning"),tr("Operation in progress"));
	return;
    }
    
    sampledData = new QVector<float>();
    sampledData->reserve(sample_duration*6000*1);
    ioDev = audioInput->start();
    if (ioDev->open(QIODevice::ReadWrite)){
	this->appendPlainText(tr("Start sampling ..."));
	connect(ioDev, SIGNAL(readyRead()), this, SLOT(readSamples())); 
    } else {
	this->appendPlainText(tr("unable to open io device to start sampling. error."));
    }
}

void MainWindow::readSamples(){

    QByteArray currentSamples = ioDev->readAll();
    qint16 *data = (qint16*)currentSamples.data();
    int size = currentSamples.size()/2;
    float sum = 0.0f;
    for (int i = 0;i < size;i++){
	float tmp = signal_gain*((float)data[i]/32767.0f);
        sum += tmp*tmp;
	sampledData->append(tmp);
    }
    float rms = sqrt(sum/(float)size);
    meter->setLevel(rms);
    meter->repaint();

    if (audioInput->processedUSecs() >= sample_duration*1000000){
	float seconds = (float)audioInput->processedUSecs()/1000000.0f;
	QString msgStr = QString("%1 seconds processed").arg(seconds);
	appendPlainText(msgStr);
	audioInput->stop();
	ioDev->close();
    }
}

void MainWindow::audioStateChanged(QAudio::State newState){
    switch (newState){
    case QAudio::SuspendedState:
	break;
    case QAudio::StoppedState :
	if (audioInput->error() != QAudio::NoError){
	    /* handle the error */
	    delete sampledData;
	} else {
	    /* finished with sampling */
	    meter->setLevel(0);
	    meter->update();
	    audioInput->reset();

	    float *buf = sampledData->data();
	    unsigned int buflen = sampledData->size();
	    appendPlainText(tr("Done sampling."));
	    quint32 *hash = NULL;
	    quint32 nbframes;
	    AudioHashStInfo hash_st;
	    appendPlainText(tr("calculating audio hash ..."));
	    audiohash(buf,&hash,NULL,NULL,NULL,&nbframes,NULL,NULL,buflen,0,6000,&hash_st);
	    ph_hashst_free(&hash_st);

	    appendPlainText(tr("looking up ..."));
	    sendThread->startWithArgs(SAMPLE_MODE, ctx, serverAddress, hash, nbframes);
	    sampledData->clear();
	    delete sampledData;
	}
	break;
    case QAudio::ActiveState :
	break;
    case QAudio::IdleState:
	break;
    default:
	break;
    }
}

void free_func(void *data, void *hint){
    Q_UNUSED(hint);
    free(data);
}

void MainWindow::appendPlainText(QString text){
    displayText->appendPlainText(text);
}

void MainWindow::changeProgressBarLevel(int level){
    progressBar->setValue(level);
}

void MainWindow::changeProgressBarRange(int min, int max){
    progressBar->setRange(min, max);

}

void MainWindow::activateProgressBar(bool visible){
    progressBar->setVisible(visible);
}

void MainWindow::finishWithTask(){
    queryButton->setText("&Hash && Send");
}

void MainWindow::postError(QString errorString){
    displayText->appendPlainText(errorString);
}
void MainWindow::clearDisplay(){
    displayText->clear();
}

void MainWindow::closeEvent(QCloseEvent *event){
    delete ctx;
    if (sendThread->isRunning()){
	sendThread->terminate();
    }
    delete sendThread;
    if (audioInput){
	delete audioInput;
    }
    deviceList.clear();
    writeSettings();
    event->accept();
}
