#include "mainwindow.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QSplitter>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QDoubleSpinBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QSlider>
#include <QFileDialog>
#include <QMessageBox>
#include <QThread>
#include <QCloseEvent>
#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <QApplication>
#include <QFont>
#include <QFrame>
#include <QScrollArea>
#include <QStatusBar>
#include <QSizePolicy>

// ──────────────────────────────────────────────────────────────────────────────
//  Construction
// ──────────────────────────────────────────────────────────────────────────────
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("VC 3D Profile Viewer  v2.0");
    setMinimumSize(1100, 700);

    // JSON player (no thread needed – all Qt slots, no blocking I/O)
    m_jsonPlayer = new JsonPlayer(this);

    buildUi();
    buildStatusBar();
    loadSettings();
    applySourceMode();
}

MainWindow::~MainWindow()
{
    if (m_sensorWorker && m_sensorWorker->isRunning()) {
        m_sensorWorker->stopCapture();
        m_sensorWorker->wait(3000);
    }
}

// ──────────────────────────────────────────────────────────────────────────────
//  UI construction
// ──────────────────────────────────────────────────────────────────────────────
void MainWindow::buildUi()
{
    // ── Central split: left panel + profile chart ─────────────────────────
    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);
    setCentralWidget(splitter);

    // ── LEFT PANEL ────────────────────────────────────────────────────────
    QScrollArea *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setMinimumWidth(280);
    scroll->setMaximumWidth(320);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    QWidget     *leftPanel  = new QWidget;
    QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(8, 8, 8, 8);
    leftLayout->setSpacing(10);

    // Source selector
    QGroupBox *sourceGroup = new QGroupBox("Datenquelle");
    QHBoxLayout *srcLayout = new QHBoxLayout(sourceGroup);
    m_rbLive     = new QRadioButton("Live Sensor");
    m_rbPlayback = new QRadioButton("JSON Wiedergabe");
    m_rbLive->setChecked(true);
    srcLayout->addWidget(m_rbLive);
    srcLayout->addWidget(m_rbPlayback);

    QButtonGroup *srcBtnGrp = new QButtonGroup(this);
    srcBtnGrp->addButton(m_rbLive,     0);
    srcBtnGrp->addButton(m_rbPlayback, 1);

    connect(m_rbLive,     &QRadioButton::toggled, this, &MainWindow::onSourceModeChanged);
    connect(m_rbPlayback, &QRadioButton::toggled, this, &MainWindow::onSourceModeChanged);

    leftLayout->addWidget(sourceGroup);

    // Sensor group
    m_sensorGroup = new QGroupBox("Sensor (Live)");
    buildSensorGroup(leftPanel, leftLayout);

    // ROI group
    m_roiGroup = new QGroupBox("ROI Einstellungen");
    buildRoiGroup(leftPanel, leftLayout);

    // Playback group
    m_playbackGroup = new QGroupBox("JSON Wiedergabe");
    buildPlaybackGroup(leftPanel, leftLayout);

    // Result display
    QGroupBox *resultGroup = new QGroupBox("Messergebnis");
    QVBoxLayout *resultLayout = new QVBoxLayout(resultGroup);

    m_lblAngle = new QLabel("—");
    m_lblAngle->setAlignment(Qt::AlignCenter);
    QFont angleFont = m_lblAngle->font();
    angleFont.setPointSize(26);
    angleFont.setBold(true);
    m_lblAngle->setFont(angleFont);
    m_lblAngle->setStyleSheet("color: #00e676; background: #1a1a2e; border-radius: 6px; padding: 8px;");

    m_lblPhi1 = new QLabel("Phi 1: —");
    m_lblPhi2 = new QLabel("Phi 2: —");
    m_lblPhi1->setStyleSheet("color: #00b4ff;");
    m_lblPhi2->setStyleSheet("color: #ff8c00;");

    QLabel *lblAngleCaption = new QLabel("Biegewinkel");
    lblAngleCaption->setAlignment(Qt::AlignCenter);
    lblAngleCaption->setStyleSheet("color: #888; font-size: 11px;");

    resultLayout->addWidget(lblAngleCaption);
    resultLayout->addWidget(m_lblAngle);
    resultLayout->addWidget(m_lblPhi1);
    resultLayout->addWidget(m_lblPhi2);
    leftLayout->addWidget(resultGroup);

    leftLayout->addStretch();

    scroll->setWidget(leftPanel);
    splitter->addWidget(scroll);

    // ── PROFILE CHART ─────────────────────────────────────────────────────
    m_profileWidget = new ProfileWidget(this);
    splitter->addWidget(m_profileWidget);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);

    // Connect ROI draw from chart to spinboxes
    connect(m_profileWidget, &ProfileWidget::roiChanged,
            this,            &MainWindow::onRoiDrawn);
}

void MainWindow::buildSensorGroup(QWidget * /*parent*/, QVBoxLayout *layout)
{
    QFormLayout *form = new QFormLayout(m_sensorGroup);
    form->setLabelAlignment(Qt::AlignRight);

    m_editIp   = new QLineEdit("192.168.3.15");
    m_editPort = new QLineEdit("1096");
    m_editPort->setMaximumWidth(80);

    form->addRow("IP-Adresse:", m_editIp);
    form->addRow("Port:",       m_editPort);

    QHBoxLayout *btnLayout = new QHBoxLayout;
    m_btnConnect    = new QPushButton("▶  Verbinden");
    m_btnDisconnect = new QPushButton("■  Trennen");
    m_btnDisconnect->setEnabled(false);
    m_btnConnect->setStyleSheet("background:#1a5c2e; color:white; padding:5px 10px; border-radius:4px;");
    m_btnDisconnect->setStyleSheet("background:#5c1a1a; color:white; padding:5px 10px; border-radius:4px;");
    btnLayout->addWidget(m_btnConnect);
    btnLayout->addWidget(m_btnDisconnect);
    form->addRow(btnLayout);

    m_lblSensorStatus = new QLabel("Getrennt");
    m_lblSensorStatus->setStyleSheet("color:#ff5555; font-weight:bold;");
    form->addRow("Status:", m_lblSensorStatus);

    layout->addWidget(m_sensorGroup);

    connect(m_btnConnect,    &QPushButton::clicked, this, &MainWindow::onConnectClicked);
    connect(m_btnDisconnect, &QPushButton::clicked, this, &MainWindow::onDisconnectClicked);
}

void MainWindow::buildRoiGroup(QWidget * /*parent*/, QVBoxLayout *layout)
{
    QFormLayout *form = new QFormLayout(m_roiGroup);
    form->setLabelAlignment(Qt::AlignRight);

    // ROI 1
    QLabel *lbl1 = new QLabel("<b><span style='color:#00b4ff'>ROI 1</span></b>");
    lbl1->setTextFormat(Qt::RichText);
    form->addRow(lbl1);

    m_roi1Start = new QDoubleSpinBox;
    m_roi1End   = new QDoubleSpinBox;
    for (auto *sb : {m_roi1Start, m_roi1End}) {
        sb->setRange(-200.0, 200.0);
        sb->setSuffix(" mm");
        sb->setDecimals(1);
        sb->setSingleStep(1.0);
    }
    m_roi1Start->setValue(-100.0);
    m_roi1End->setValue(0.0);
    form->addRow("Start:", m_roi1Start);
    form->addRow("Ende:",  m_roi1End);

    // Separator
    QFrame *sep = new QFrame;
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet("color:#444;");
    form->addRow(sep);

    // ROI 2
    QLabel *lbl2 = new QLabel("<b><span style='color:#ff8c00'>ROI 2</span></b>");
    lbl2->setTextFormat(Qt::RichText);
    form->addRow(lbl2);

    m_roi2Start = new QDoubleSpinBox;
    m_roi2End   = new QDoubleSpinBox;
    for (auto *sb : {m_roi2Start, m_roi2End}) {
        sb->setRange(-200.0, 200.0);
        sb->setSuffix(" mm");
        sb->setDecimals(1);
        sb->setSingleStep(1.0);
    }
    m_roi2Start->setValue(0.0);
    m_roi2End->setValue(100.0);
    form->addRow("Start:", m_roi2Start);
    form->addRow("Ende:",  m_roi2End);

    QLabel *lblHint = new QLabel("Tipp: ROI auch per Maus-Drag im Profil ziehen");
    lblHint->setStyleSheet("color:#666; font-size:10px;");
    lblHint->setWordWrap(true);
    form->addRow(lblHint);

    layout->addWidget(m_roiGroup);

    connect(m_roi1Start, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MainWindow::onRoi1Changed);
    connect(m_roi1End,   QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MainWindow::onRoi1Changed);
    connect(m_roi2Start, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MainWindow::onRoi2Changed);
    connect(m_roi2End,   QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MainWindow::onRoi2Changed);
}

void MainWindow::buildPlaybackGroup(QWidget * /*parent*/, QVBoxLayout *layout)
{
    QVBoxLayout *vbl = new QVBoxLayout(m_playbackGroup);

    // Folder picker
    QHBoxLayout *folderRow = new QHBoxLayout;
    m_editFolder = new QLineEdit;
    m_editFolder->setPlaceholderText("JSON-Ordner wählen…");
    m_editFolder->setReadOnly(true);
    m_btnBrowse = new QPushButton("…");
    m_btnBrowse->setFixedWidth(32);
    folderRow->addWidget(m_editFolder);
    folderRow->addWidget(m_btnBrowse);
    vbl->addLayout(folderRow);

    // Frame info
    m_lblFrameInfo = new QLabel("Kein Ordner geladen");
    m_lblFrameInfo->setStyleSheet("color:#aaa; font-size:11px;");
    m_lblFrameInfo->setAlignment(Qt::AlignCenter);
    vbl->addWidget(m_lblFrameInfo);

    // Playback buttons
    QHBoxLayout *btnRow = new QHBoxLayout;
    m_btnStepBack = new QPushButton("◀◀");
    m_btnPlay     = new QPushButton("▶  Play");
    m_btnStop     = new QPushButton("■  Stop");
    m_btnStepFwd  = new QPushButton("▶▶");

    m_btnPlay->setStyleSheet("background:#1a5c2e; color:white; padding:4px 8px; border-radius:4px;");
    m_btnStop->setStyleSheet("background:#5c3a1a; color:white; padding:4px 8px; border-radius:4px;");

    for (auto *b : {m_btnStepBack, m_btnPlay, m_btnStop, m_btnStepFwd})
        btnRow->addWidget(b);
    vbl->addLayout(btnRow);

    // Speed slider
    QHBoxLayout *speedRow = new QHBoxLayout;
    QLabel *lblSpeedCaption = new QLabel("Geschwindigkeit:");
    m_speedSlider = new QSlider(Qt::Horizontal);
    m_speedSlider->setRange(1, 10);   // 1 = slow (2000ms), 10 = fast (100ms)
    m_speedSlider->setValue(5);       // default ~500ms
    m_lblSpeed = new QLabel("1.0×");
    m_lblSpeed->setMinimumWidth(35);
    speedRow->addWidget(lblSpeedCaption);
    speedRow->addWidget(m_speedSlider);
    speedRow->addWidget(m_lblSpeed);
    vbl->addLayout(speedRow);

    layout->addWidget(m_playbackGroup);

    // Connections
    connect(m_btnBrowse,  &QPushButton::clicked, this, &MainWindow::onBrowseJsonFolder);
    connect(m_btnPlay,    &QPushButton::clicked, this, &MainWindow::onPlayClicked);
    connect(m_btnStop,    &QPushButton::clicked, this, &MainWindow::onStopClicked);
    connect(m_btnStepBack,&QPushButton::clicked, this, &MainWindow::onStepBackClicked);
    connect(m_btnStepFwd, &QPushButton::clicked, this, &MainWindow::onStepForwardClicked);
    connect(m_speedSlider,&QSlider::valueChanged, this, &MainWindow::onSpeedSliderChanged);

    // JsonPlayer signals
    connect(m_jsonPlayer, &JsonPlayer::profileReady,    m_profileWidget, &ProfileWidget::updateProfile);
    connect(m_jsonPlayer, &JsonPlayer::frameChanged,    this, &MainWindow::onJsonFrameChanged);
    connect(m_jsonPlayer, &JsonPlayer::folderLoaded,    this, &MainWindow::onJsonFolderLoaded);
    connect(m_jsonPlayer, &JsonPlayer::playbackStarted, this, &MainWindow::onJsonPlaybackStarted);
    connect(m_jsonPlayer, &JsonPlayer::playbackStopped, this, &MainWindow::onJsonPlaybackStopped);
}

void MainWindow::buildStatusBar()
{
    statusBar()->showMessage("Bereit – Quelle wählen und verbinden");
}

// ──────────────────────────────────────────────────────────────────────────────
//  Source mode
// ──────────────────────────────────────────────────────────────────────────────
void MainWindow::applySourceMode()
{
    bool isLive = (m_sourceMode == SourceMode::LiveSensor);

    m_sensorGroup->setEnabled(isLive);
    m_playbackGroup->setEnabled(!isLive);

    if (!isLive) {
        // Stop live sensor if running
        if (m_sensorWorker && m_sensorWorker->isRunning()) {
            m_sensorWorker->stopCapture();
            m_sensorWorker->wait(3000);
            updateConnectButtons(false);
        }
        statusBar()->showMessage("JSON Wiedergabe – Ordner laden und Play drücken");
    } else {
        m_jsonPlayer->stop();
        updatePlayButtons(false);
        statusBar()->showMessage("Live Sensor – IP/Port eingeben und Verbinden drücken");
    }
}

void MainWindow::onSourceModeChanged()
{
    m_sourceMode = m_rbLive->isChecked() ? SourceMode::LiveSensor : SourceMode::JsonPlayback;
    applySourceMode();
}

// ──────────────────────────────────────────────────────────────────────────────
//  Sensor slots
// ──────────────────────────────────────────────────────────────────────────────
void MainWindow::onConnectClicked()
{
    if (m_sensorWorker && m_sensorWorker->isRunning()) return;

    QString ip   = m_editIp->text().trimmed();
    int     port = m_editPort->text().trimmed().toInt();

    RoiRect roi1, roi2;
    roi1.xMin = m_roi1Start->value(); roi1.xMax = m_roi1End->value(); roi1.valid = (roi1.xMax > roi1.xMin);
    roi2.xMin = m_roi2Start->value(); roi2.xMax = m_roi2End->value(); roi2.valid = (roi2.xMax > roi2.xMin);

    m_sensorWorker = new SensorWorker(this);
    connect(m_sensorWorker, &SensorWorker::profileReady,    this, &MainWindow::onSensorData);
    connect(m_sensorWorker, &SensorWorker::angleReady,      this, &MainWindow::onAngleReady);
    connect(m_sensorWorker, &SensorWorker::errorOccurred,   this, &MainWindow::onSensorError);
    connect(m_sensorWorker, &SensorWorker::connected,       this, &MainWindow::onSensorConnected);
    connect(m_sensorWorker, &SensorWorker::disconnected,    this, &MainWindow::onSensorDisconnected);

    connect(m_sensorWorker, &QThread::finished, m_sensorWorker, &QObject::deleteLater);

    m_sensorWorker->startCapture(ip, m_editPort->text().toUShort(), roi1, roi2);
    statusBar()->showMessage("Verbinde mit " + ip + ":" + QString::number(port) + " …");
}

void MainWindow::onDisconnectClicked()
{
    if (!m_sensorWorker) return;
    m_sensorWorker->stopCapture();
    m_sensorWorker->wait(3000);
    m_sensorWorker = nullptr;
    updateConnectButtons(false);
    statusBar()->showMessage("Verbindung getrennt");
}

void MainWindow::onSensorData(const std::vector<ProfilePoint> &points)
{
    m_profileWidget->updateProfile(points);
}

void MainWindow::onAngleReady(AngleResult result)
{
    if (!result.valid) return;
    m_lblPhi1->setText(QString("Phi 1: %1°").arg(result.phi, 0, 'f', 2));
    m_lblPhi2->setText(QString("Phi 2: —"));  // second ROI from separate parse
    m_lblAngle->setText(QString("%1°").arg(result.phi, 0, 'f', 2));
}

void MainWindow::onSensorError(const QString &msg)
{
    statusBar()->showMessage("Fehler: " + msg);
    m_lblSensorStatus->setText("Fehler");
    m_lblSensorStatus->setStyleSheet("color:#ff5555; font-weight:bold;");
}

void MainWindow::onSensorConnected()
{
    updateConnectButtons(true);
    m_lblSensorStatus->setText("Verbunden");
    m_lblSensorStatus->setStyleSheet("color:#00e676; font-weight:bold;");
    statusBar()->showMessage("Sensor verbunden – Profildaten werden empfangen");
}

void MainWindow::onSensorDisconnected()
{
    updateConnectButtons(false);
    m_lblSensorStatus->setText("Getrennt");
    m_lblSensorStatus->setStyleSheet("color:#ff5555; font-weight:bold;");
}

void MainWindow::updateConnectButtons(bool connected)
{
    m_btnConnect->setEnabled(!connected);
    m_btnDisconnect->setEnabled(connected);
}

// ──────────────────────────────────────────────────────────────────────────────
//  ROI slots
// ──────────────────────────────────────────────────────────────────────────────
void MainWindow::onRoi1Changed()
{
    RoiRect roi1; roi1.xMin = m_roi1Start->value(); roi1.xMax = m_roi1End->value();
    roi1.valid = (roi1.xMax > roi1.xMin);
    RoiRect roi2; roi2.xMin = m_roi2Start->value(); roi2.xMax = m_roi2End->value();
    roi2.valid = (roi2.xMax > roi2.xMin);
    m_profileWidget->setRoi(0, roi1);
    if (m_sensorWorker && m_sourceMode == SourceMode::LiveSensor)
        m_sensorWorker->updateRois(roi1, roi2);
}

void MainWindow::onRoi2Changed()
{
    RoiRect roi1; roi1.xMin = m_roi1Start->value(); roi1.xMax = m_roi1End->value();
    roi1.valid = (roi1.xMax > roi1.xMin);
    RoiRect roi2; roi2.xMin = m_roi2Start->value(); roi2.xMax = m_roi2End->value();
    roi2.valid = (roi2.xMax > roi2.xMin);
    m_profileWidget->setRoi(1, roi2);
    if (m_sensorWorker && m_sourceMode == SourceMode::LiveSensor)
        m_sensorWorker->updateRois(roi1, roi2);
}

void MainWindow::onRoiDrawn(int roiIndex, RoiRect r)
{
    float xStart = static_cast<float>(r.xMin);
    float xEnd   = static_cast<float>(r.xMax);
    if (roiIndex == 0) {
        QSignalBlocker b1(m_roi1Start), b2(m_roi1End);
        m_roi1Start->setValue(xStart);
        m_roi1End->setValue(xEnd);
        if (m_sensorWorker && m_sourceMode == SourceMode::LiveSensor) {
            RoiRect r2; r2.xMin = m_roi2Start->value(); r2.xMax = m_roi2End->value();
            m_sensorWorker->updateRois(r, r2);
        }
    } else {
        QSignalBlocker b1(m_roi2Start), b2(m_roi2End);
        m_roi2Start->setValue(xStart);
        m_roi2End->setValue(xEnd);
        if (m_sensorWorker && m_sourceMode == SourceMode::LiveSensor) {
            RoiRect r1; r1.xMin = m_roi1Start->value(); r1.xMax = m_roi1End->value();
            m_sensorWorker->updateRois(r1, r);
        }
    }
    statusBar()->showMessage(QString("ROI %1 gesetzt: %2 … %3 mm")
                                 .arg(roiIndex + 1)
                                 .arg(xStart, 0, 'f', 1)
                                 .arg(xEnd,   0, 'f', 1));
}

// ──────────────────────────────────────────────────────────────────────────────
//  JSON Playback slots
// ──────────────────────────────────────────────────────────────────────────────
void MainWindow::onBrowseJsonFolder()
{
    QString dir = QFileDialog::getExistingDirectory(
        this, "JSON-Ordner wählen",
        m_editFolder->text().isEmpty() ?
            QDir(QApplication::applicationDirPath()).filePath("Data") :
            m_editFolder->text());

    if (dir.isEmpty()) return;

    m_editFolder->setText(dir);
    m_jsonPlayer->loadFolder(dir);
}

void MainWindow::onPlayClicked()
{
    if (m_jsonPlayer->frameCount() == 0) {
        statusBar()->showMessage("Kein Ordner geladen – bitte zuerst JSON-Ordner wählen");
        return;
    }
    m_jsonPlayer->play();
}

void MainWindow::onStopClicked()
{
    m_jsonPlayer->stop();
}

void MainWindow::onStepForwardClicked()
{
    m_jsonPlayer->stepForward();
}

void MainWindow::onStepBackClicked()
{
    m_jsonPlayer->stepBack();
}

void MainWindow::onSpeedSliderChanged(int value)
{
    // value 1..10 → interval 2000ms..100ms (inverse)
    // speed multiplier: 1=0.5x, 5=1x, 10=2x displayed nicely
    int intervalMs = 2100 - value * 200;  // 1→1900, 5→1100... let's do a nicer curve
    // Simple mapping: value 1=2000ms, 3=1000ms, 5=500ms, 7=250ms, 10=100ms
    static const int intervals[] = {2000, 1500, 1000, 750, 500, 350, 250, 200, 150, 100};
    intervalMs = intervals[value - 1];

    m_jsonPlayer->setIntervalMs(intervalMs);

    float hz = 1000.0f / intervalMs;
    m_lblSpeed->setText(QString("%1 Hz").arg(hz, 0, 'f', 1));
}

void MainWindow::onJsonFrameChanged(int index, int total)
{
    m_lblFrameInfo->setText(QString("Frame %1 / %2").arg(index + 1).arg(total));
}

void MainWindow::onJsonFolderLoaded(int frameCount)
{
    m_lblFrameInfo->setText(QString("%1 Dateien geladen").arg(frameCount));
    statusBar()->showMessage(QString("JSON-Ordner geladen: %1 Frames – Play drücken zum Abspielen").arg(frameCount));
    m_btnPlay->setEnabled(frameCount > 0);
    m_btnStepBack->setEnabled(frameCount > 0);
    m_btnStepFwd->setEnabled(frameCount > 0);
}

void MainWindow::onJsonPlaybackStarted()
{
    updatePlayButtons(true);
    statusBar()->showMessage("JSON Wiedergabe läuft…");
}

void MainWindow::onJsonPlaybackStopped()
{
    updatePlayButtons(false);
    statusBar()->showMessage("JSON Wiedergabe gestoppt");
}

void MainWindow::updatePlayButtons(bool playing)
{
    m_btnPlay->setEnabled(!playing);
    m_btnStop->setEnabled(playing);
}

// ──────────────────────────────────────────────────────────────────────────────
//  Settings (QSettings → Devices/default.ini)
// ──────────────────────────────────────────────────────────────────────────────
QString MainWindow::settingsPath() const
{
    return QDir(QApplication::applicationDirPath()).filePath("Devices/default.ini");
}

void MainWindow::saveSettings()
{
    QSettings s(settingsPath(), QSettings::IniFormat);
    s.setValue("Sensor/IP",      m_editIp->text());
    s.setValue("Sensor/Port",    m_editPort->text());
    s.setValue("ROI1/Start",     m_roi1Start->value());
    s.setValue("ROI1/End",       m_roi1End->value());
    s.setValue("ROI2/Start",     m_roi2Start->value());
    s.setValue("ROI2/End",       m_roi2End->value());
    s.setValue("Playback/Folder",m_editFolder->text());
    s.setValue("Playback/Speed", m_speedSlider->value());
    s.setValue("Source/Mode",    static_cast<int>(m_sourceMode));
}

void MainWindow::loadSettings()
{
    QString path = settingsPath();

    // Create Devices folder if it doesn't exist
    QDir dir(QApplication::applicationDirPath());
    dir.mkpath("Devices");
    dir.mkpath("Data");

    if (!QFile::exists(path)) return;

    QSettings s(path, QSettings::IniFormat);
    m_editIp->setText(s.value("Sensor/IP",   "192.168.3.15").toString());
    m_editPort->setText(s.value("Sensor/Port","1096").toString());
    m_roi1Start->setValue(s.value("ROI1/Start", -100.0).toDouble());
    m_roi1End->setValue(s.value("ROI1/End",       0.0).toDouble());
    m_roi2Start->setValue(s.value("ROI2/Start",   0.0).toDouble());
    m_roi2End->setValue(s.value("ROI2/End",      100.0).toDouble());

    QString folder = s.value("Playback/Folder", "").toString();
    if (!folder.isEmpty()) m_editFolder->setText(folder);

    m_speedSlider->setValue(s.value("Playback/Speed", 5).toInt());
    onSpeedSliderChanged(m_speedSlider->value());  // refresh label

    int mode = s.value("Source/Mode", 0).toInt();
    if (mode == 1) {
        m_rbPlayback->setChecked(true);
        m_sourceMode = SourceMode::JsonPlayback;
    }

    // Sync ROIs to chart – valid=true so they are drawn immediately
    { RoiRect r; r.xMin = m_roi1Start->value(); r.xMax = m_roi1End->value(); r.valid = (r.xMax > r.xMin); m_profileWidget->setRoi(0, r); }
    { RoiRect r; r.xMin = m_roi2Start->value(); r.xMax = m_roi2End->value(); r.valid = (r.xMax > r.xMin); m_profileWidget->setRoi(1, r); }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveSettings();
    if (m_sensorWorker && m_sensorWorker->isRunning()) {
        m_sensorWorker->stopCapture();
        m_sensorWorker->wait(3000);
    }
    m_jsonPlayer->stop();
    event->accept();
}
