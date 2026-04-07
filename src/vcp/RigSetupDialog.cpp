#include "vcp/RigSetupDialog.h"
#include "core/SerialTransport.h"
#include "Style.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QCompleter>
#include <QPushButton>
#include <QSettings>

RigSetupDialog::RigSetupDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Rig Setup");
    setMinimumWidth(420);

    auto *layout = new QVBoxLayout(this);

    // --- Rig model ---
    auto *rigGroup = new QGroupBox("Rig Model");
    auto *rigLayout = new QFormLayout(rigGroup);

    m_rigCombo = new QComboBox;
    m_rigCombo->setEditable(true);
    m_rigCombo->setInsertPolicy(QComboBox::NoInsert);
    m_rigCombo->completer()->setFilterMode(Qt::MatchContains);
    m_rigCombo->completer()->setCaseSensitivity(Qt::CaseInsensitive);
    rigLayout->addRow("Model:", m_rigCombo);

    populateRigList();
    layout->addWidget(rigGroup);

    // --- Connection ---
    auto *connGroup = new QGroupBox("Connection");
    auto *connLayout = new QFormLayout(connGroup);

    m_portCombo = new QComboBox;
    m_portCombo->setEditable(true);
    // Populate with available serial ports
    for (const auto &port : SerialTransport::availablePorts()) {
        m_portCombo->addItem(port);
    }
    m_portCombo->setCurrentText("");

    m_baudCombo = new QComboBox;
    m_baudCombo->addItems({"4800", "9600", "19200", "38400", "57600", "115200"});
    m_baudCombo->setCurrentText("115200");

    connLayout->addRow("Port:", m_portCombo);
    connLayout->addRow("Baud:", m_baudCombo);

    auto *hint = new QLabel("Select serial port for your rig.\n"
                            "For TCP rigs, enter hostname:port in the Port field.");
    hint->setWordWrap(true);
    QFont hintFont(Style::Font::Family);
    hintFont.setPixelSize(Style::Font::Small);
    hint->setFont(hintFont);
    hint->setStyleSheet(QString("color: %1;").arg(Style::Color::TextGray));
    connLayout->addRow(hint);

    layout->addWidget(connGroup);

    // --- Sweep settings ---
    auto *sweepGroup = new QGroupBox("Sweep Settings");
    auto *sweepLayout = new QFormLayout(sweepGroup);

    m_txModeCombo = new QComboBox;
    m_txModeCombo->addItems({"CW", "AM", "FSK", "USB", "LSB"});

    m_powerSpin = new QDoubleSpinBox;
    m_powerSpin->setRange(0.01, 1.0);
    m_powerSpin->setSingleStep(0.05);
    m_powerSpin->setValue(0.10);
    m_powerSpin->setDecimals(2);

    m_sampleTimeCombo = new QComboBox;
    m_sampleTimeCombo->addItem("100 ms", 100);
    m_sampleTimeCombo->addItem("200 ms", 200);
    m_sampleTimeCombo->addItem("300 ms", 300);
    m_sampleTimeCombo->addItem("500 ms", 500);
    m_sampleTimeCombo->addItem("1 sec", 1000);
    m_sampleTimeCombo->setCurrentIndex(2);

    sweepLayout->addRow("TX Mode:", m_txModeCombo);
    sweepLayout->addRow("RF Power:", m_powerSpin);
    sweepLayout->addRow("Sample Time:", m_sampleTimeCombo);

    layout->addWidget(sweepGroup);

    // --- Buttons ---
    m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    m_buttons->button(QDialogButtonBox::Ok)->setText("Connect");
    connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(m_buttons);

    loadFromSettings();
}

void RigSetupDialog::populateRigList() {
    m_rigs = HamlibRig::availableRigs();

    m_rigCombo->clear();
    for (const auto &rig : m_rigs) {
        QString display = QString("%1 %2").arg(rig.manufacturer, rig.model).trimmed();
        m_rigCombo->addItem(display, rig.modelId);
    }
}

int RigSetupDialog::rigModelId() const {
    return m_rigCombo->currentData().toInt();
}

QString RigSetupDialog::rigDisplayName() const {
    return m_rigCombo->currentText();
}

QString RigSetupDialog::port() const {
    return m_portCombo->currentText().trimmed();
}

int RigSetupDialog::baudRate() const {
    return m_baudCombo->currentText().toInt();
}

QString RigSetupDialog::txMode() const {
    return m_txModeCombo->currentText();
}

double RigSetupDialog::rfPower() const {
    return m_powerSpin->value();
}

int RigSetupDialog::sampleTimeMs() const {
    return m_sampleTimeCombo->currentData().toInt();
}

void RigSetupDialog::loadFromSettings() {
    QSettings s("AI5QK", "QK-LP100A");
    int rigId = s.value("rig/modelId", 2047).toInt();
    QString rigPort = s.value("rig/port", "").toString();
    int baud = s.value("rig/baudRate", 115200).toInt();
    QString txMode = s.value("rig/txMode", "CW").toString();
    double power = s.value("rig/rfPower", 0.10).toDouble();
    int sampleMs = s.value("rig/sampleTimeMs", 300).toInt();

    for (int i = 0; i < m_rigCombo->count(); ++i) {
        if (m_rigCombo->itemData(i).toInt() == rigId) {
            m_rigCombo->setCurrentIndex(i);
            break;
        }
    }

    m_portCombo->setCurrentText(rigPort);
    m_baudCombo->setCurrentText(QString::number(baud));
    m_txModeCombo->setCurrentText(txMode);
    m_powerSpin->setValue(power);

    for (int i = 0; i < m_sampleTimeCombo->count(); ++i) {
        if (m_sampleTimeCombo->itemData(i).toInt() == sampleMs) {
            m_sampleTimeCombo->setCurrentIndex(i);
            break;
        }
    }
}

void RigSetupDialog::saveToSettings() {
    QSettings s("AI5QK", "QK-LP100A");
    s.setValue("rig/modelId", rigModelId());
    s.setValue("rig/port", port());
    s.setValue("rig/baudRate", baudRate());
    s.setValue("rig/txMode", txMode());
    s.setValue("rig/rfPower", rfPower());
    s.setValue("rig/sampleTimeMs", sampleTimeMs());
}
