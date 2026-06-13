#include "vcp/VCPMainWindow.h"
#include "vcp/ConnectionDialog.h"
#include "plot/PlotWidget.h"
#include "core/SerialTransport.h"
#include "core/TcpTransport.h"
#include "Style.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>
#include <QApplication>
#include <QActionGroup>
#include <QTimer>
#include <QStandardPaths>
#include <QDir>
#include <QTextStream>
#include <QDateTime>
#include <QSettings>
#include <QFile>
#include <QAction>

VCPMainWindow::VCPMainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("QK-LP100A");
    resize(Style::Layout::VCPCompactW, Style::Layout::VCPCompactH);

    m_hamlib = new HamlibRig(this);
    connect(m_hamlib, &HamlibRig::errorOccurred, this, [this](const QString &err) {
        m_rigStatusLabel->setText("Rig: " + err);
    });

    createUI();
    createMenus();
    loadSettings();
    applyViewStyle();

    // Sync style menu with loaded setting
    switch (m_viewStyle) {
        case Compact:  m_compactAction->setChecked(true); break;
        case Standard: m_standardAction->setChecked(true); break;
        case Full:     m_fullAction->setChecked(true); break;
    }

    if (m_wasConnected && (m_useTcp ? !m_tcpHost.isEmpty() : !m_serialPort.isEmpty())) {
        QTimer::singleShot(0, this, &VCPMainWindow::connectToDevice);
    }
}

VCPMainWindow::~VCPMainWindow() {
    disconnect(this);
    saveSettings();
    if (m_protocol) m_protocol->stopPolling();
    if (m_transport && m_transport->isOpen()) m_transport->close();
}

void VCPMainWindow::createUI() {
    auto *central = new QWidget;
    setCentralWidget(central);
    auto *mainLayout = new QVBoxLayout(central);
    mainLayout->setSpacing(Style::Layout::MainSpacing);
    mainLayout->setContentsMargins(Style::Layout::MainMarginH, Style::Layout::MainMarginTop,
                                   Style::Layout::MainMarginH, Style::Layout::MainMarginBottom);

    // Callsign + Mode row
    auto *topRow = new QHBoxLayout;
    topRow->setSpacing(Style::Layout::MainMarginH);

    m_callsignLabel = new QLabel("LP-100A");
    QFont callFont(Style::Font::Family);
    callFont.setPixelSize(Style::Font::Callsign);
    callFont.setBold(true);
    m_callsignLabel->setFont(callFont);
    m_callsignLabel->setStyleSheet(
        QString("color: %1; background: %2; border: 1px solid %3; "
                "border-radius: %4px; padding: %5px %6px;")
            .arg(Style::Color::AccentAmber, Style::Color::DarkBackground,
                 Style::Color::PanelBorder)
            .arg(Style::Layout::BannerRadius)
            .arg(Style::Layout::BannerPadV).arg(Style::Layout::BannerPadH));

    m_modeLabel = new QLabel("Avg");
    QFont modeFont(Style::Font::Family);
    modeFont.setPixelSize(Style::Font::Medium);
    m_modeLabel->setFont(modeFont);
    m_modeLabel->setStyleSheet(QString("color: %1;").arg(Style::Color::TextGray));

    topRow->addStretch();
    topRow->addWidget(m_callsignLabel);
    topRow->addStretch();
    topRow->addWidget(m_modeLabel);
    mainLayout->addLayout(topRow);

    // Meters
    m_powerGauge = new PowerGauge;
    m_powerGauge->setLabel("Pwr");
    m_powerGauge->setMaxValue(Style::PowerRange::Low);
    m_powerGauge->setFixedHeight(Style::Layout::MeterRowHeight);
    mainLayout->addWidget(m_powerGauge);

    m_refGauge = new PowerGauge;
    m_refGauge->setLabel("Ref");
    m_refGauge->setMaxValue(Style::PowerRange::Low);
    m_refGauge->setFixedHeight(Style::Layout::MeterRowHeight);
    mainLayout->addWidget(m_refGauge);

    m_swrGauge = new SWRGauge;
    m_swrGauge->setFixedHeight(Style::Layout::MeterRowHeight);
    mainLayout->addWidget(m_swrGauge);

    // Control buttons (hideable section)
    m_buttonSection = new QWidget;
    auto *btnRow = new QHBoxLayout(m_buttonSection);
    btnRow->setSpacing(Style::Layout::ButtonSpacing);
    btnRow->setContentsMargins(0, Style::Layout::ImpGridMarginTop, 0, 0);

    m_rangeBtn = new QPushButton("Range: Auto");
    m_alarmBtn = new QPushButton("Alarm: Off");
    m_modeBtn = new QPushButton("Avg");

    for (auto *btn : {m_rangeBtn, m_alarmBtn, m_modeBtn}) {
        btn->setStyleSheet(Style::buttonCompact());
        btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        btn->setFixedHeight(Style::Layout::CompactButtonHeight);
    }

    btnRow->addWidget(m_rangeBtn);
    btnRow->addWidget(m_alarmBtn);
    btnRow->addWidget(m_modeBtn);
    mainLayout->addWidget(m_buttonSection);

    // Impedance section
    m_impedanceSection = new QWidget;
    auto *impGrid = new QGridLayout(m_impedanceSection);
    impGrid->setContentsMargins(0, Style::Layout::ImpGridMarginTop, 0, 0);
    impGrid->setHorizontalSpacing(Style::Layout::ImpGridHSpacing);
    impGrid->setVerticalSpacing(Style::Layout::ImpGridVSpacing);

    QFont fieldFont(Style::Font::Family);
    fieldFont.setPixelSize(Style::Font::Normal);
    QFont valueFont(Style::Font::Monospace);
    valueFont.setPixelSize(Style::Font::Medium);
    valueFont.setBold(true);

    auto makeLabel = [&](const QString &text) {
        auto *l = new QLabel(text);
        l->setFont(fieldFont);
        l->setStyleSheet(QString("color: %1;").arg(Style::Color::TextGray));
        return l;
    };
    auto makeValue = [&](const QString &text) {
        auto *l = new QLabel(text);
        l->setFont(valueFont);
        l->setStyleSheet(QString("color: %1;").arg(Style::Color::TextWhite));
        l->setAlignment(Qt::AlignRight);
        l->setMinimumWidth(65);
        return l;
    };

    m_zLabel = makeValue("--");
    m_phaseLabel = makeValue("--");
    m_rLabel = makeValue("--");
    m_xLabel = makeValue("--");
    m_dBmLabel = makeValue("--");

    impGrid->addWidget(makeLabel("Z:"), 0, 0);
    impGrid->addWidget(m_zLabel, 0, 1);
    impGrid->addWidget(makeLabel("PH:"), 0, 2);
    impGrid->addWidget(m_phaseLabel, 0, 3);
    impGrid->addWidget(makeLabel("dBm:"), 0, 4);
    impGrid->addWidget(m_dBmLabel, 0, 5);
    impGrid->addWidget(makeLabel("R:"), 1, 0);
    impGrid->addWidget(m_rLabel, 1, 1);
    impGrid->addWidget(makeLabel("X:"), 1, 2);
    impGrid->addWidget(m_xLabel, 1, 3);
    mainLayout->addWidget(m_impedanceSection);

    // Raw label
    m_rawLabel = new QLabel;
    QFont rawFont(Style::Font::Monospace);
    rawFont.setPixelSize(Style::Font::Tiny);
    m_rawLabel->setFont(rawFont);
    m_rawLabel->setStyleSheet(
        QString("color: %1; background: %2; border: 1px solid %3; padding: 1px 3px;")
            .arg(Style::Color::InactiveGray, Style::Color::DarkBackground,
                 Style::Color::PanelBorder));
    m_rawLabel->setFixedHeight(Style::Layout::RawLabelHeight);
    mainLayout->addWidget(m_rawLabel);

    // --- Status bar ---
    QFont statusFont(Style::Font::Monospace);
    statusFont.setPixelSize(Style::Font::Small);

    m_statusLabel = new QLabel("Disconnected");
    m_statusLabel->setFont(statusFont);
    m_statusLabel->setContentsMargins(4, 0, 0, 0);
    m_statusLabel->setStyleSheet(QString("color: %1;").arg(Style::Color::TextGray));

    m_rigStatusLabel = new QLabel("Rig: Disconnected");
    m_rigStatusLabel->setFont(statusFont);
    m_rigStatusLabel->setContentsMargins(0, 0, 4, 0);
    m_rigStatusLabel->setStyleSheet(QString("color: %1;").arg(Style::Color::TextGray));

    statusBar()->addWidget(m_statusLabel, 1);
    statusBar()->addPermanentWidget(m_rigStatusLabel);
    statusBar()->setFixedHeight(Style::Layout::StatusBarHeight);

    // Button signals
    connect(m_rangeBtn, &QPushButton::clicked, this, &VCPMainWindow::onRangeClicked);
    connect(m_alarmBtn, &QPushButton::clicked, this, &VCPMainWindow::onAlarmClicked);
    connect(m_modeBtn, &QPushButton::clicked, this, &VCPMainWindow::onModeClicked);

    setStyleSheet(
        QString("QMainWindow { background: %1; } QStatusBar { background: %2; }")
            .arg(Style::Color::Background, Style::Color::DarkBackground));
}

void VCPMainWindow::createMenus() {
    // File menu
    auto *fileMenu = menuBar()->addMenu("File");
    m_connectAction = fileMenu->addAction("LP-100A Connect", this, &VCPMainWindow::connectFromMenu);
    m_disconnectAction = fileMenu->addAction("LP-100A Disconnect", this, &VCPMainWindow::disconnectFromDevice);
    m_disconnectAction->setEnabled(false);
    m_settingsAction = fileMenu->addAction("Settings...", this, &VCPMainWindow::showSettingsDialog);
    fileMenu->addSeparator();
    fileMenu->addAction("Open Plot Window", this, &VCPMainWindow::openPlotWindow);
    fileMenu->addSeparator();
    fileMenu->addAction("Quit", QKeySequence::Quit, qApp, &QApplication::quit);

    // Style menu
    auto *styleMenu = menuBar()->addMenu("Style");
    auto *styleGroup = new QActionGroup(this);
    styleGroup->setExclusive(true);

    m_compactAction = styleMenu->addAction("Compact");
    m_compactAction->setCheckable(true);
    m_compactAction->setActionGroup(styleGroup);
    connect(m_compactAction, &QAction::triggered, this, [this]() { setViewStyle(Compact); });

    m_standardAction = styleMenu->addAction("Standard");
    m_standardAction->setCheckable(true);
    m_standardAction->setActionGroup(styleGroup);
    connect(m_standardAction, &QAction::triggered, this, [this]() { setViewStyle(Standard); });

    m_fullAction = styleMenu->addAction("Full");
    m_fullAction->setCheckable(true);
    m_fullAction->setActionGroup(styleGroup);
    connect(m_fullAction, &QAction::triggered, this, [this]() { setViewStyle(Full); });

    // Help menu
    auto *helpMenu = menuBar()->addMenu("Help");
    helpMenu->addAction("About QK-LP100A...", this, [this]() {
        QMessageBox::about(this, "QK-LP100A",
            QString("QK-LP100A\nVersion %1\n\n"
                    "Virtual Control Panel & Plot\n"
                    "for TelePost LP-100A\n\nAI5QK")
                .arg(QApplication::applicationVersion()));
    });
}

// --- Mode switching ---

void VCPMainWindow::openPlotWindow() {
    if (m_plotWindow) {
        m_plotWindow->raise();
        m_plotWindow->activateWindow();
        return;
    }

    auto *plotWidget = new PlotWidget;
    plotWidget->setRig(m_hamlib);
    plotWidget->setLP100A(m_protocol);
    plotWidget->setWindowTitle("QK-LP100A — Plot");
    plotWidget->setAttribute(Qt::WA_DeleteOnClose);
    plotWidget->resize(Style::Layout::PlotDefaultW, Style::Layout::PlotDefaultH);
    plotWidget->setStyleSheet(
        QString("background: %1;").arg(Style::Color::Background));

    connect(plotWidget, &PlotWidget::rigConnectRequested, this,
            [this](int modelId, const QString &port, int baudRate) {
        m_rigStatusLabel->setText("Rig: Connecting...");
        m_rigStatusLabel->setStyleSheet(QString("color: %1;").arg(Style::Color::TextGray));
        if (m_hamlib->open(modelId, port, baudRate)) {
            m_rigStatusLabel->setText("Rig: " + m_hamlib->rigName());
            m_rigStatusLabel->setStyleSheet(QString("color: %1;").arg(Style::Color::StatusGreen));
        } else {
            m_rigStatusLabel->setText("Rig: " + m_hamlib->errorString());
            m_rigStatusLabel->setStyleSheet(QString("color: %1;").arg(Style::Color::StatusRed));
        }
    });
    connect(plotWidget, &PlotWidget::rigDisconnectRequested, this, [this]() {
        m_hamlib->close();
        m_rigStatusLabel->setText("Rig: Disconnected");
        m_rigStatusLabel->setStyleSheet(QString("color: %1;").arg(Style::Color::TextGray));
    });
    connect(plotWidget, &QObject::destroyed, this, [this]() {
        m_plotWindow = nullptr;
    });

    m_plotWindow = plotWidget;
    plotWidget->show();
}

// --- Rig control ---

// --- View styles ---

void VCPMainWindow::setViewStyle(ViewStyle style) {
    m_viewStyle = style;
    applyViewStyle();
    saveSettings();
}

void VCPMainWindow::applyViewStyle() {
    // Compact: meters only (no buttons, no impedance, no raw string)
    // Standard: meters + buttons
    // Full: meters + buttons + impedance + raw string
    m_buttonSection->setVisible(m_viewStyle >= Standard);
    m_impedanceSection->setVisible(m_viewStyle == Full);
    m_rawLabel->setVisible(m_viewStyle == Full);

    // Remove size constraints so we can shrink freely
    setMinimumSize(0, 0);
    setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    centralWidget()->adjustSize();

    switch (m_viewStyle) {
        case Compact:  setFixedSize(Style::Layout::VCPCompactW, Style::Layout::VCPCompactH); break;
        case Standard: setFixedSize(Style::Layout::VCPStandardW, Style::Layout::VCPStandardH); break;
        case Full:
            resize(sizeHint());
            break;
    }
}

// --- LP-100A connection ---

void VCPMainWindow::connectFromMenu() {
    // Connect directly using saved settings — no dialog. If nothing is
    // configured yet (no serial port chosen for a serial connection), fall
    // back to opening Settings first.
    if (!m_useTcp && m_serialPort.isEmpty()) {
        showSettingsDialog();
        return;
    }
    connectToDevice();
}

void VCPMainWindow::showSettingsDialog() {
    ConnectionDialog dlg(this);
    dlg.setConnectionType(m_useTcp ? ConnectionDialog::TCP : ConnectionDialog::Serial);
    dlg.setSerialPort(m_serialPort);
    dlg.setTcpHost(m_tcpHost);
    dlg.setTcpPort(m_tcpPort);
    dlg.setPollInterval(m_pollInterval);

    if (dlg.exec() != QDialog::Accepted)
        return;

    // Capture the old endpoint so we only reconnect if it actually changed.
    const bool    oldUseTcp = m_useTcp;
    const QString oldSerial = m_serialPort;
    const QString oldHost   = m_tcpHost;
    const quint16 oldPort   = m_tcpPort;

    m_useTcp = (dlg.connectionType() == ConnectionDialog::TCP);
    m_serialPort = dlg.serialPort();
    m_tcpHost = dlg.tcpHost();
    m_tcpPort = dlg.tcpPort();
    m_pollInterval = dlg.pollIntervalMs();

    // Debug logging
    m_debugLogging = dlg.debugLogging();
    QSettings ds("AI5QK", "QK-LP100A");
    ds.setValue("debug/enabled", m_debugLogging);
    if (m_debugLogging && !m_debugFile) {
        QString logPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                          + "/debug.log";
        QDir().mkpath(QFileInfo(logPath).absolutePath());
        m_debugFile = new QFile(logPath, this);
        m_debugFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
        debugLog("=== Debug logging started ===");
        debugLog("Log file: " + logPath);
    } else if (!m_debugLogging && m_debugFile) {
        debugLog("=== Debug logging stopped ===");
        m_debugFile->close();
        m_debugFile->deleteLater();
        m_debugFile = nullptr;
    }
    SWRGauge::setDebugEnabled(m_debugLogging);
    saveSettings();

    // Apply changes. If not connected, settings just take effect on next Connect.
    const bool connected = m_transport && m_transport->isOpen();
    if (!connected)
        return;

    const bool endpointChanged = (m_useTcp != oldUseTcp)
        || (m_useTcp && (m_tcpHost != oldHost || m_tcpPort != oldPort))
        || (!m_useTcp && m_serialPort != oldSerial);

    if (endpointChanged) {
        connectToDevice();  // Auto-reconnect to apply the new endpoint
    } else if (m_protocol) {
        m_protocol->setPollInterval(m_pollInterval);  // Live, no reconnect
    }
}

void VCPMainWindow::connectToDevice() {
    if (m_protocol) {
        disconnect(m_protocol, nullptr, this, nullptr);
        m_protocol->stopPolling();
        m_protocol->deleteLater();
        m_protocol = nullptr;
    }
    if (m_transport) {
        disconnect(m_transport, nullptr, this, nullptr);
        m_transport->close();
        m_transport->deleteLater();
        m_transport = nullptr;
    }

    if (m_useTcp) {
        auto *tcp = new TcpTransport(this);
        tcp->setHost(m_tcpHost);
        tcp->setPort(m_tcpPort);
        m_transport = tcp;
    } else {
        auto *serial = new SerialTransport(this);
        serial->setPortName(m_serialPort);
        m_transport = serial;
    }

    m_protocol = new LP100AProtocol(m_transport, this);
    connect(m_transport, &ITransport::connectionChanged, this, &VCPMainWindow::onConnectionChanged);
    connect(m_transport, &ITransport::errorOccurred, this, [this](const QString &err) {
        m_statusLabel->setText("Error: " + err);
    });
    connect(m_protocol, &LP100AProtocol::dataUpdated, this, &VCPMainWindow::onDataUpdated);
    connect(m_protocol, &LP100AProtocol::rawResponse, this, &VCPMainWindow::onRawResponse);
    connect(m_protocol, &LP100AProtocol::parseError, this, &VCPMainWindow::onParseError);
    connect(m_protocol, &LP100AProtocol::responseTimedOut, this, &VCPMainWindow::onResponseTimedOut);

    // Update plot widget with the protocol reference
    if (auto *pw = qobject_cast<PlotWidget *>(m_plotWindow))
        pw->setLP100A(m_protocol);

    if (!m_transport->open()) {
        m_statusLabel->setText("Failed to connect");
    }
}

void VCPMainWindow::disconnectFromDevice() {
    if (m_protocol) m_protocol->stopPolling();
    if (m_transport) m_transport->close();
    updateConnectionUI(false);
}

void VCPMainWindow::onConnectionChanged(bool connected) {
    updateConnectionUI(connected);
    if (connected && m_protocol) m_protocol->startPolling(m_pollInterval);
}

void VCPMainWindow::updateConnectionUI(bool connected) {
    m_connectAction->setEnabled(!connected);
    m_disconnectAction->setEnabled(connected);
    if (!connected) m_silent = false;
    if (connected) {
        QString text = m_useTcp ? QString("LP-100A: %1:%2").arg(m_tcpHost).arg(m_tcpPort)
                                : QString("LP-100A: %1").arg(m_serialPort);
        m_statusLabel->setText(text);
        m_statusLabel->setStyleSheet(QString("color: %1;").arg(Style::Color::StatusGreen));
    } else {
        m_statusLabel->setText("Disconnected");
        m_statusLabel->setStyleSheet(QString("color: %1;").arg(Style::Color::TextGray));
    }
}

// --- Data handling ---

void VCPMainWindow::onDataUpdated(const LP100AData &data) {
    // Device is responding again — restore the green connected status.
    if (m_silent) {
        m_silent = false;
        updateConnectionUI(true);
    }

    m_powerGauge->setValue(data.power);
    m_refGauge->setValue(data.reflectedPower());
    updatePowerRange(data.powerRange);

    // Mode suffix on power readout: "w" for Avg, "W" for Peak, "T" for Tune
    // Matches the LP-100A front panel convention
    bool isPeakMode = (data.peakHoldMode == 1);
    bool isTuneMode = (data.peakHoldMode == 2);
    m_powerGauge->setPeakMode(isPeakMode);
    m_powerGauge->setTuneMode(isTuneMode);
    m_refGauge->setPeakMode(isPeakMode);
    m_refGauge->setTuneMode(isTuneMode);
    switch (data.peakHoldMode) {
        case 0: m_powerGauge->setModeSuffix("w"); break;  // Average
        case 1: m_powerGauge->setModeSuffix("W"); break;  // Peak
        case 2: m_powerGauge->setModeSuffix("T"); break;  // Tune
    }

    m_modeLabel->setText(data.modeString());

    // SWR validity logic
    bool swrValid;
    if (data.swr > 1.005) {
        swrValid = data.power > 0.05;
    } else {
        swrValid = data.power > 2.0 && m_lastReportedPower > 2.0;
    }

    // Debug: log every poll where power or SWR changed
    static double lastLogPwr = -1, lastLogSwr = -1;
    if (m_debugLogging && (data.power != lastLogPwr || data.swr != lastLogSwr)) {
        debugLog(QString("[POLL] Pwr=%1 SWR=%2 Mode=%3 swrValid=%4 lastPwr=%5")
            .arg(data.power).arg(data.swr).arg(data.modeString())
            .arg(swrValid).arg(m_lastReportedPower));
        lastLogPwr = data.power;
        lastLogSwr = data.swr;
    }

    m_lastReportedPower = data.power;
    m_swrGauge->setPowerPresent(swrValid);
    m_swrGauge->setTuneMode(isTuneMode);
    m_swrGauge->setPeakMode(isPeakMode);
    m_swrGauge->setValue(data.swr);
    m_swrGauge->setAlarmPoint(data.alarmSWR());

    m_zLabel->setText(QString("%1\u03A9").arg(data.impedance, 0, 'f', 1));
    m_phaseLabel->setText(QString("%1\u00B0").arg(data.phase, 0, 'f', 1));
    m_rLabel->setText(QString("%1\u03A9").arg(data.resistance(), 0, 'f', 1));
    m_xLabel->setText(QString("%1\u03A9").arg(data.reactance(), 0, 'f', 1));
    m_dBmLabel->setText(QString("%1dBm").arg(data.dBm, 0, 'f', 1));

    if (!data.callsign.trimmed().isEmpty())
        m_callsignLabel->setText(data.callsign.trimmed());

    bool alarmTripped = data.alarmSWR() > 0 && data.swr >= data.alarmSWR()
                        && data.power > Style::SWR::AlarmPowerThreshold;
    if (data.alarmSetPoint != m_lastAlarmSetPoint) {
        m_lastAlarmSetPoint = data.alarmSetPoint;
        m_alarmBtn->setText(QString("Alarm: %1").arg(data.alarmString()));
    }
    if (alarmTripped != m_lastAlarmTripped) {
        m_lastAlarmTripped = alarmTripped;
        m_alarmBtn->setStyleSheet(alarmTripped ? Style::buttonCompactAlarm()
                                               : Style::buttonCompact());
    }
    m_modeBtn->setText(data.modeString());
}

void VCPMainWindow::updatePowerRange(int rangeCode) {
    if (!m_autoRange) return;
    double maxPwr;
    switch (rangeCode) {
        case 0: maxPwr = Style::PowerRange::High; break;
        case 1: maxPwr = Style::PowerRange::Mid;  break;
        case 2: maxPwr = Style::PowerRange::Low;   break;
        default: maxPwr = Style::PowerRange::Mid;
    }
    if (maxPwr != m_currentMaxPower) {
        m_currentMaxPower = maxPwr;
        m_powerGauge->setMaxValue(maxPwr);
        m_refGauge->setMaxValue(maxPwr);
        m_rangeBtn->setText("Range: Auto");
    }
}

void VCPMainWindow::onRawResponse(const QString &resp) { m_rawLabel->setText(resp); }

void VCPMainWindow::debugLog(const QString &msg) {
    if (!m_debugLogging || !m_debugFile || !m_debugFile->isOpen()) return;
    QTextStream out(m_debugFile);
    out << QDateTime::currentDateTime().toString("hh:mm:ss.zzz") << " " << msg << "\n";
    out.flush();
}
void VCPMainWindow::onParseError(const QString &err) { m_statusLabel->setText("Parse error: " + err); }

void VCPMainWindow::onResponseTimedOut() {
    // Transport is connected but the LP-100A isn't answering polls — flag amber
    // instead of leaving a misleading green "connected" status.
    if (m_silent) return;  // Already flagged; don't restyle on every timeout
    m_silent = true;
    QString endpoint = m_useTcp ? QString("%1:%2").arg(m_tcpHost).arg(m_tcpPort) : m_serialPort;
    m_statusLabel->setText(QString("LP-100A: %1 — no response").arg(endpoint));
    m_statusLabel->setStyleSheet(QString("color: %1;").arg(Style::Color::StatusAmber));
}

void VCPMainWindow::onRangeClicked() {
    static constexpr double ranges[] = {
        Style::PowerRange::Low, Style::PowerRange::Mid, Style::PowerRange::High, 0.0
    };
    static constexpr const char *labels[] = {"25", "250", "2500", "Auto"};
    constexpr int numRanges = static_cast<int>(std::size(ranges));

    m_rangeIdx = (m_rangeIdx + 1) % numRanges;
    m_autoRange = (m_rangeIdx == numRanges - 1);
    if (!m_autoRange) {
        m_currentMaxPower = ranges[m_rangeIdx];
        m_powerGauge->setMaxValue(m_currentMaxPower);
        m_refGauge->setMaxValue(m_currentMaxPower);
    }
    m_rangeBtn->setText(QString("Range: %1").arg(labels[m_rangeIdx]));
}

void VCPMainWindow::onAlarmClicked() { if (m_protocol) m_protocol->incrementAlarm(); }
void VCPMainWindow::onModeClicked() { if (m_protocol) m_protocol->togglePeakAvgTune(); }

// --- Settings ---

void VCPMainWindow::saveSettings() {
    QSettings s("AI5QK", "QK-LP100A");
    s.setValue("serial/port", m_serialPort);
    s.setValue("tcp/host", m_tcpHost);
    s.setValue("tcp/port", m_tcpPort);
    s.setValue("poll/interval", m_pollInterval);
    s.setValue("connection/useTcp", m_useTcp);
    s.setValue("connection/wasConnected", m_transport && m_transport->isOpen());
    s.setValue("view/style", static_cast<int>(m_viewStyle));
    s.setValue("window/geometry", saveGeometry());
}

void VCPMainWindow::loadSettings() {
    QSettings s("AI5QK", "QK-LP100A");
    m_serialPort = s.value("serial/port", "").toString();
    m_tcpHost = s.value("tcp/host", Style::Protocol::DefaultTcpHost).toString();
    m_tcpPort = s.value("tcp/port", Style::Protocol::DefaultTcpPort).toUInt();
    m_pollInterval = s.value("poll/interval", Style::Protocol::DefaultPollMs).toInt();
    m_useTcp = s.value("connection/useTcp", false).toBool();
    m_wasConnected = s.value("connection/wasConnected", false).toBool();
    m_viewStyle = static_cast<ViewStyle>(s.value("view/style", Full).toInt());

    QByteArray geo = s.value("window/geometry").toByteArray();
    if (!geo.isEmpty()) restoreGeometry(geo);
}
