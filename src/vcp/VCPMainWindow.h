#pragma once

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include "core/ITransport.h"
#include "core/LP100AProtocol.h"
#include "core/HamlibRig.h"
#include "vcp/PowerGauge.h"
#include "vcp/SWRGauge.h"

class QAction;
class QFile;

class VCPMainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit VCPMainWindow(QWidget *parent = nullptr);
    ~VCPMainWindow();

    enum ViewStyle { Compact, Standard, Full };

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

    void openPlotWindow();

private:
    void createUI();
    void createMenus();
    void saveSettings();
    void loadSettings();
    void updateConnectionUI(bool connected);
    void updatePowerRange(int rangeCode);
    void applyViewStyle();
    void debugLog(const QString &msg);

    ITransport *m_transport = nullptr;
    LP100AProtocol *m_protocol = nullptr;

    QString m_serialPort;
    QString m_tcpHost;
    quint16 m_tcpPort = 0;   // Loaded from settings; default in Style::Protocol
    int m_pollInterval = 0;
    bool m_useTcp = false;
    bool m_wasConnected = false;
    ViewStyle m_viewStyle = Full;

    PowerGauge *m_powerGauge;
    PowerGauge *m_refGauge;
    SWRGauge *m_swrGauge;

    QLabel *m_callsignLabel;
    QLabel *m_modeLabel;
    QLabel *m_statusLabel;
    QLabel *m_rawLabel;

    QWidget *m_buttonSection;
    QWidget *m_impedanceSection;
    QLabel *m_zLabel;
    QLabel *m_phaseLabel;
    QLabel *m_rLabel;
    QLabel *m_xLabel;
    QLabel *m_dBmLabel;

    QPushButton *m_rangeBtn;
    QPushButton *m_alarmBtn;
    QPushButton *m_modeBtn;

    // Menu actions
    QAction *m_connectAction;
    QAction *m_disconnectAction;
    QAction *m_compactAction;
    QAction *m_standardAction;
    QAction *m_fullAction;

    // Plot window (separate from VCP)
    QWidget *m_plotWindow = nullptr;
    HamlibRig *m_hamlib = nullptr;
    QLabel *m_rigStatusLabel;

    double m_currentMaxPower = 25.0;
    bool m_autoRange = true;
    int m_rangeIdx = 3;  // Start at Auto (index 3 of 4 choices)
    int m_lastAlarmSetPoint = -1;
    bool m_lastAlarmTripped = false;
    double m_lastReportedPower = 0.0;
    bool m_debugLogging = false;
    QFile *m_debugFile = nullptr;
};
