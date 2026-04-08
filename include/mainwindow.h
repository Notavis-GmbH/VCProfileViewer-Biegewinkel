#pragma once

#include <QMainWindow>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QRadioButton>
#include <QSlider>
#include <QCheckBox>
#include <QToolButton>
#include <QSettings>
#include <memory>

#include "vcprotocol.h"
#include "sensorworker.h"
#include "profilewidget.h"
#include "jsonplayer.h"

// ──────────────────────────────────────────────
//  Source mode
// ──────────────────────────────────────────────
enum class SourceMode { LiveSensor, JsonPlayback };

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    // Live sensor
    void onConnectClicked();
    void onDisconnectClicked();
    void onSensorData(const std::vector<ProfilePoint> &points);
    void onAngleReady(AngleResult result);
    void onSensorError(const QString &msg);
    void onSensorConnected();
    void onSensorDisconnected();

    // ROI
    void onRoi1Changed();
    void onRoi2Changed();
    void onRoiDrawn(int roiIndex, float xStart, float xEnd);

    // JSON Playback
    void onSourceModeChanged();
    void onBrowseJsonFolder();
    void onPlayClicked();
    void onStopClicked();
    void onStepForwardClicked();
    void onStepBackClicked();
    void onSpeedSliderChanged(int value);
    void onJsonFrameChanged(int index, int total);
    void onJsonFolderLoaded(int frameCount);
    void onJsonPlaybackStarted();
    void onJsonPlaybackStopped();

private:
    void buildUi();
    void buildSensorGroup(QWidget *parent, class QVBoxLayout *layout);
    void buildRoiGroup(QWidget *parent, class QVBoxLayout *layout);
    void buildPlaybackGroup(QWidget *parent, class QVBoxLayout *layout);
    void buildStatusBar();

    void applySourceMode();
    void updateConnectButtons(bool connected);
    void updatePlayButtons(bool playing);
    void saveSettings();
    void loadSettings();
    QString settingsPath() const;

    // ── Widgets ───────────────────────────────
    ProfileWidget  *m_profileWidget = nullptr;

    // Source selector
    QRadioButton   *m_rbLive       = nullptr;
    QRadioButton   *m_rbPlayback   = nullptr;

    // Sensor group
    QGroupBox      *m_sensorGroup  = nullptr;
    QLineEdit      *m_editIp       = nullptr;
    QLineEdit      *m_editPort     = nullptr;
    QPushButton    *m_btnConnect   = nullptr;
    QPushButton    *m_btnDisconnect= nullptr;
    QLabel         *m_lblSensorStatus = nullptr;

    // ROI group
    QGroupBox      *m_roiGroup     = nullptr;
    QDoubleSpinBox *m_roi1Start    = nullptr;
    QDoubleSpinBox *m_roi1End      = nullptr;
    QDoubleSpinBox *m_roi2Start    = nullptr;
    QDoubleSpinBox *m_roi2End      = nullptr;

    // Result display
    QLabel         *m_lblAngle     = nullptr;
    QLabel         *m_lblPhi1      = nullptr;
    QLabel         *m_lblPhi2      = nullptr;

    // JSON playback group
    QGroupBox      *m_playbackGroup= nullptr;
    QLineEdit      *m_editFolder   = nullptr;
    QPushButton    *m_btnBrowse    = nullptr;
    QPushButton    *m_btnPlay      = nullptr;
    QPushButton    *m_btnStop      = nullptr;
    QPushButton    *m_btnStepBack  = nullptr;
    QPushButton    *m_btnStepFwd   = nullptr;
    QSlider        *m_speedSlider  = nullptr;
    QLabel         *m_lblSpeed     = nullptr;
    QLabel         *m_lblFrameInfo = nullptr;

    // ── Backend objects ───────────────────────
    SensorWorker   *m_sensorWorker = nullptr;  // SensorWorker is a QThread itself
    JsonPlayer     *m_jsonPlayer   = nullptr;

    SourceMode      m_sourceMode   = SourceMode::LiveSensor;
};
