#pragma once

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QActionGroup>
#include "core/ITransport.h"
#include "core/LP100AProtocol.h"
#include "vcp/PowerGauge.h"
#include "vcp/SWRGauge.h"

class QFrame;
class QAction;

class VCPMainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit VCPMainWindow(QWidget *parent = nullptr);
    ~VCPMainWindow();

    enum ViewStyle {
        Compact,   // Meters + buttons only
        Standard,  // + Impedance values
        Full       // + Raw input string
    };

private slots:
    void onDataUpdated(const LP100AData &data);
    void onConnectionChanged(bool connected);
    void onRawResponse(const QString &resp);
    void onParseError(const QString &err);

    void showConnectionDialog();
    void connectToDevice();
    void disconnectFromDevice();

    void onRangeClicked();
    void onAlarmClicked();
    void onModeClicked();

    void setViewStyle(ViewStyle style);

private:
    void createUI();
    void createMenus();
    void saveSettings();
    void loadSettings();
    void updateConnectionUI(bool connected);
    void updatePowerRange(int rangeCode);
    void applyViewStyle();

    ITransport *m_transport = nullptr;
    LP100AProtocol *m_protocol = nullptr;

    QString m_serialPort;
    QString m_tcpHost;
    quint16 m_tcpPort = 10001;
    int m_pollInterval = 80;
    bool m_useTcp = false;
    bool m_wasConnected = false;

    ViewStyle m_viewStyle = Full;

    // Meters
    PowerGauge *m_powerGauge;
    PowerGauge *m_refGauge;
    SWRGauge *m_swrGauge;

    // Labels
    QLabel *m_callsignLabel;
    QLabel *m_modeLabel;
    QLabel *m_statusLabel;
    QLabel *m_rawLabel;

    // Impedance
    QWidget *m_impedanceSection;
    QLabel *m_zLabel;
    QLabel *m_phaseLabel;
    QLabel *m_rLabel;
    QLabel *m_xLabel;
    QLabel *m_dBmLabel;

    // Buttons (always visible)
    QPushButton *m_rangeBtn;
    QPushButton *m_alarmBtn;
    QPushButton *m_modeBtn;

    // Menu actions
    QAction *m_connectAction;
    QAction *m_disconnectAction;

    double m_currentMaxPower = 25.0;
    bool m_autoRange = true;
    int m_lastAlarmSetPoint = -1;
    bool m_lastAlarmTripped = false;
};
