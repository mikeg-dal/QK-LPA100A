#include "vcp/VCPMainWindow.h"
#include "vcp/ConnectionDialog.h"
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

VCPMainWindow::VCPMainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("QK-LP100A Virtual Control Panel");

    createUI();
    createMenus();
    loadSettings();
    applyViewStyle();

    // Auto-reconnect if previously connected (deferred to next event loop iteration)
    if (m_wasConnected && (m_useTcp ? !m_tcpHost.isEmpty() : !m_serialPort.isEmpty())) {
        QTimer::singleShot(0, this, &VCPMainWindow::connectToDevice);
    }
}

VCPMainWindow::~VCPMainWindow() {
    disconnect(this);  // Sever all incoming signals before teardown
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

    // Control buttons (always visible)
    auto *btnRow = new QHBoxLayout;
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
    mainLayout->addLayout(btnRow);

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

    // Raw input string (Full view only)
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

    // Status bar
    m_statusLabel = new QLabel("Disconnected");
    QFont statusFont(Style::Font::Family);
    statusFont.setPixelSize(Style::Font::Small);
    m_statusLabel->setFont(statusFont);
    m_statusLabel->setStyleSheet(QString("color: %1;").arg(Style::Color::TextGray));
    statusBar()->addWidget(m_statusLabel, 1);
    statusBar()->setFixedHeight(Style::Layout::StatusBarHeight);

    // Signal connections
    connect(m_rangeBtn, &QPushButton::clicked, this, &VCPMainWindow::onRangeClicked);
    connect(m_alarmBtn, &QPushButton::clicked, this, &VCPMainWindow::onAlarmClicked);
    connect(m_modeBtn, &QPushButton::clicked, this, &VCPMainWindow::onModeClicked);

    setStyleSheet(
        QString("QMainWindow { background: %1; } QStatusBar { background: %2; }")
            .arg(Style::Color::Background, Style::Color::DarkBackground));
}

void VCPMainWindow::createMenus() {
    auto *fileMenu = menuBar()->addMenu("File");
    m_connectAction = fileMenu->addAction("Connect...", this, &VCPMainWindow::showConnectionDialog);
    m_disconnectAction = fileMenu->addAction("Disconnect", this, &VCPMainWindow::disconnectFromDevice);
    m_disconnectAction->setEnabled(false);
    fileMenu->addSeparator();
    fileMenu->addAction("Quit", QKeySequence::Quit, qApp, &QApplication::quit);

    auto *styleMenu = menuBar()->addMenu("Style");
    auto *styleGroup = new QActionGroup(this);
    styleGroup->setExclusive(true);

    auto *compactAction = styleMenu->addAction("Compact");
    compactAction->setCheckable(true);
    compactAction->setActionGroup(styleGroup);
    connect(compactAction, &QAction::triggered, this, [this]() { setViewStyle(Compact); });

    auto *standardAction = styleMenu->addAction("Standard");
    standardAction->setCheckable(true);
    standardAction->setActionGroup(styleGroup);
    connect(standardAction, &QAction::triggered, this, [this]() { setViewStyle(Standard); });

    auto *fullAction = styleMenu->addAction("Full");
    fullAction->setCheckable(true);
    fullAction->setChecked(true);
    fullAction->setActionGroup(styleGroup);
    connect(fullAction, &QAction::triggered, this, [this]() { setViewStyle(Full); });

    auto *helpMenu = menuBar()->addMenu("Help");
    helpMenu->addAction("About QK-LP100A...", this, [this]() {
        QMessageBox::about(this, "QK-LP100A",
            QString("QK-LP100A Virtual Control Panel\nVersion %1\n\n"
                    "TelePost LP-100A Interface\nAI5QK")
                .arg(QApplication::applicationVersion()));
    });
}

void VCPMainWindow::setViewStyle(ViewStyle style) {
    m_viewStyle = style;
    applyViewStyle();
    saveSettings();
}

void VCPMainWindow::applyViewStyle() {
    m_impedanceSection->setVisible(m_viewStyle >= Standard);
    m_rawLabel->setVisible(m_viewStyle == Full);
    centralWidget()->adjustSize();
    adjustSize();
}

void VCPMainWindow::showConnectionDialog() {
    ConnectionDialog dlg(this);
    dlg.setSerialPort(m_serialPort);
    dlg.setTcpHost(m_tcpHost);
    dlg.setTcpPort(m_tcpPort);
    dlg.setPollInterval(m_pollInterval);

    if (dlg.exec() == QDialog::Accepted) {
        m_useTcp = (dlg.connectionType() == ConnectionDialog::TCP);
        m_serialPort = dlg.serialPort();
        m_tcpHost = dlg.tcpHost();
        m_tcpPort = dlg.tcpPort();
        m_pollInterval = dlg.pollIntervalMs();
        saveSettings();
        connectToDevice();
    }
}

void VCPMainWindow::connectToDevice() {
    // Safe teardown: disconnect signals first, then deleteLater
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

    if (!m_transport->open()) {
        m_statusLabel->setText("Failed to connect");
        return;
    }
    // Trust the connectionChanged signal — don't call onConnectionChanged directly
    // (SerialTransport emits it synchronously from open(), TcpTransport emits async)
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
    if (connected) {
        m_statusLabel->setText(m_useTcp
            ? QString("Connected: %1:%2").arg(m_tcpHost).arg(m_tcpPort)
            : QString("Connected: %1").arg(m_serialPort));
    } else {
        m_statusLabel->setText("Disconnected");
    }
}

void VCPMainWindow::onDataUpdated(const LP100AData &data) {
    m_powerGauge->setValue(data.power);
    m_refGauge->setValue(data.reflectedPower());
    updatePowerRange(data.powerRange);

    m_modeLabel->setText(data.modeString());
    m_swrGauge->setValue(data.swr);
    m_swrGauge->setAlarmPoint(data.alarmSWR());

    m_zLabel->setText(QString("%1\u03A9").arg(data.impedance, 0, 'f', 1));
    m_phaseLabel->setText(QString("%1\u00B0").arg(data.phase, 0, 'f', 1));
    m_rLabel->setText(QString("%1\u03A9").arg(data.resistance(), 0, 'f', 1));
    m_xLabel->setText(QString("%1\u03A9").arg(data.reactance(), 0, 'f', 1));
    m_dBmLabel->setText(QString("%1dBm").arg(data.dBm, 0, 'f', 1));

    if (!data.callsign.trimmed().isEmpty())
        m_callsignLabel->setText(data.callsign.trimmed());

    // Alarm button — only update stylesheet on state change
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
void VCPMainWindow::onParseError(const QString &err) { m_statusLabel->setText("Parse error: " + err); }

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
