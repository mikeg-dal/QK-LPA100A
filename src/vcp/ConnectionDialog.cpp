#include "vcp/ConnectionDialog.h"
#include "core/SerialTransport.h"
#include "Style.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QFormLayout>
#include <QPushButton>

ConnectionDialog::ConnectionDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Connection Settings");
    setMinimumWidth(380);

    auto *layout = new QVBoxLayout(this);

    // --- Serial group ---
    m_serialRadio = new QRadioButton("Serial Port");
    m_serialRadio->setChecked(true);

    auto *serialGroup = new QGroupBox;
    auto *serialLayout = new QFormLayout(serialGroup);

    m_portCombo = new QComboBox;
    m_portCombo->setEditable(false);

    auto *refreshBtn = new QPushButton("Refresh");
    auto *portRow = new QHBoxLayout;
    portRow->addWidget(m_portCombo, 1);
    portRow->addWidget(refreshBtn);

    serialLayout->addRow("Port:", portRow);

    // --- TCP group ---
    m_tcpRadio = new QRadioButton("TCP/IP (Lantronix / Serial Server)");

    auto *tcpGroup = new QGroupBox;
    auto *tcpLayout = new QFormLayout(tcpGroup);

    m_hostEdit = new QLineEdit;
    m_hostEdit->setPlaceholderText("192.168.1.100");

    m_tcpPortSpin = new QSpinBox;
    m_tcpPortSpin->setRange(Style::Protocol::MinTcpPort, Style::Protocol::MaxTcpPort);
    m_tcpPortSpin->setValue(Style::Protocol::DefaultTcpPort);

    tcpLayout->addRow("Host:", m_hostEdit);
    tcpLayout->addRow("Port:", m_tcpPortSpin);

    // --- Polling ---
    auto *pollGroup = new QGroupBox("Polling");
    auto *pollLayout = new QFormLayout(pollGroup);

    m_pollSpin = new QSpinBox;
    m_pollSpin->setRange(Style::Protocol::MinPollMs, Style::Protocol::MaxPollMs);
    m_pollSpin->setValue(Style::Protocol::DefaultPollMs);
    m_pollSpin->setSuffix(" ms");
    m_pollSpin->setSingleStep(Style::Protocol::PollStep);

    pollLayout->addRow("Interval:", m_pollSpin);

    // --- Buttons ---
    m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

    layout->addWidget(m_serialRadio);
    layout->addWidget(serialGroup);
    layout->addSpacing(8);
    layout->addWidget(m_tcpRadio);
    layout->addWidget(tcpGroup);
    layout->addSpacing(8);
    layout->addWidget(pollGroup);
    layout->addWidget(m_buttons);

    // Enable/disable groups based on radio selection
    auto updateGroups = [=]() {
        serialGroup->setEnabled(m_serialRadio->isChecked());
        tcpGroup->setEnabled(m_tcpRadio->isChecked());
    };
    connect(m_serialRadio, &QRadioButton::toggled, this, updateGroups);
    updateGroups();

    connect(refreshBtn, &QPushButton::clicked, this, &ConnectionDialog::refreshPorts);
    connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    refreshPorts();
}

ConnectionDialog::ConnectionType ConnectionDialog::connectionType() const {
    return m_serialRadio->isChecked() ? Serial : TCP;
}

QString ConnectionDialog::serialPort() const {
    return m_portCombo->currentText();
}

QString ConnectionDialog::tcpHost() const {
    return m_hostEdit->text();
}

quint16 ConnectionDialog::tcpPort() const {
    return static_cast<quint16>(m_tcpPortSpin->value());
}

int ConnectionDialog::pollIntervalMs() const {
    return m_pollSpin->value();
}

void ConnectionDialog::setSerialPort(const QString &port) {
    int idx = m_portCombo->findText(port);
    if (idx >= 0) m_portCombo->setCurrentIndex(idx);
}

void ConnectionDialog::setTcpHost(const QString &host) {
    m_hostEdit->setText(host);
}

void ConnectionDialog::setTcpPort(quint16 port) {
    m_tcpPortSpin->setValue(port);
}

void ConnectionDialog::setPollInterval(int ms) {
    m_pollSpin->setValue(ms);
}

void ConnectionDialog::refreshPorts() {
    m_portCombo->clear();
    for (const auto &port : SerialTransport::availablePorts()) {
        m_portCombo->addItem(port);
    }
}
