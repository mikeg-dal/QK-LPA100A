#include "plot/PlotWidget.h"
#include "plot/SweepEngine.h"
#include "plot/SmithChartWidget.h"
#include "core/HamlibRig.h"
#include "core/LP100AProtocol.h"
#include "core/SerialTransport.h"
#include "Style.h"
#include <QStackedWidget>
#include <QtCharts/QLegendMarker>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSettings>
#include <QApplication>
#include <QLineEdit>
#include <QCloseEvent>
#include <QFileDialog>
#include <QGroupBox>
#include <cmath>

PlotWidget::PlotWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(4);
    mainLayout->setContentsMargins(8, 4, 8, 4);

    createChart();
    m_smithChart = new SmithChartWidget;
    m_chartStack = new QStackedWidget;
    m_chartStack->addWidget(m_chartView);       // Page 0: Single chart
    m_chartStack->addWidget(m_dualChartWidget);  // Page 1: Dual stacked charts
    m_chartStack->addWidget(m_smithChart);       // Page 2: Smith Chart

    createControls();

    // QGroupBox stylesheet for dark theme
    const QString groupStyle = QString(
        "QGroupBox { border: 1px solid %1; border-radius: 4px;"
        " margin-top: 10px; padding: 6px 6px 4px 6px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 8px;"
        " padding: 0 4px; color: %2; font-weight: bold; font-size: %3px; }")
        .arg(Style::Color::PanelBorder, Style::Color::AccentAmber)
        .arg(Style::Font::Medium);

    // Display mode combo as chart title (centered above chart)
    auto *titleRow = new QHBoxLayout;
    titleRow->addStretch();
    titleRow->addWidget(m_displayCombo);
    titleRow->addStretch();
    mainLayout->addLayout(titleRow);

    mainLayout->addWidget(m_chartStack, 1);

    // === Sweep Section ===
    auto *sweepGroup = new QGroupBox("Sweep");
    sweepGroup->setStyleSheet(groupStyle);
    auto *sweepLayout = new QVBoxLayout(sweepGroup);
    sweepLayout->setSpacing(2);
    sweepLayout->setContentsMargins(6, 8, 6, 4);

    auto *sweepRow1 = new QHBoxLayout;
    sweepRow1->setSpacing(4);
    sweepRow1->addWidget(new QLabel("Start"));
    sweepRow1->addWidget(m_startFreq);
    sweepRow1->addSpacing(12);
    sweepRow1->addWidget(new QLabel("Step"));
    sweepRow1->addSpacing(4);
    sweepRow1->addWidget(m_stepCombo);
    sweepRow1->addStretch();
    sweepLayout->addLayout(sweepRow1);

    auto *sweepRow2 = new QHBoxLayout;
    sweepRow2->setSpacing(4);
    sweepRow2->addWidget(new QLabel("Stop"));
    sweepRow2->addWidget(m_stopFreq);
    sweepRow2->addSpacing(12);
    sweepRow2->addWidget(new QLabel("Sample"));
    sweepRow2->addSpacing(4);
    sweepRow2->addWidget(m_sampleCombo);
    sweepRow2->addSpacing(8);
    sweepRow2->addWidget(new QLabel("Settle"));
    sweepRow2->addSpacing(4);
    sweepRow2->addWidget(m_settleCombo);
    sweepRow2->addStretch();
    sweepLayout->addLayout(sweepRow2);

    mainLayout->addWidget(sweepGroup);

    // === Rig Section ===
    auto *rigGroup = new QGroupBox("Rig");
    rigGroup->setStyleSheet(groupStyle);
    auto *rigLayout = new QVBoxLayout(rigGroup);
    rigLayout->setSpacing(2);
    rigLayout->setContentsMargins(6, 8, 6, 4);

    auto *rigRow1 = new QHBoxLayout;
    rigRow1->setSpacing(4);
    rigRow1->addWidget(m_rigCombo);
    rigRow1->addWidget(m_rigPortCombo, 1);
    rigRow1->addWidget(m_rigBaudCombo);
    rigRow1->addWidget(m_rigConnectBtn);
    rigLayout->addLayout(rigRow1);

    auto *rigRow2 = new QHBoxLayout;
    rigRow2->setSpacing(4);
    rigRow2->addWidget(new QLabel("TX Mode"));
    rigRow2->addSpacing(4);
    rigRow2->addWidget(m_txModeCombo);
    rigRow2->addSpacing(12);
    rigRow2->addWidget(m_powerSpin);
    rigRow2->addStretch();
    rigLayout->addLayout(rigRow2);

    mainLayout->addWidget(rigGroup);

    // === Actions Row ===
    auto *actionsRow = new QHBoxLayout;
    actionsRow->setSpacing(4);
    actionsRow->addWidget(m_runBtn);
    actionsRow->addWidget(m_resetBtn);
    actionsRow->addWidget(m_stopBtn);
    actionsRow->addSpacing(12);
    actionsRow->addWidget(m_signCheck);
    actionsRow->addWidget(m_splineCheck);
    actionsRow->addWidget(m_bestFitCheck);
    actionsRow->addStretch();

    auto *exportGroup = new QGroupBox("Export");
    exportGroup->setStyleSheet(groupStyle);
    auto *exportLayout = new QHBoxLayout(exportGroup);
    exportLayout->setSpacing(4);
    exportLayout->setContentsMargins(6, 8, 6, 4);
    exportLayout->addWidget(m_exportCsvBtn);
    exportLayout->addWidget(m_exportImgBtn);
    actionsRow->addWidget(exportGroup);
    mainLayout->addLayout(actionsRow);

    // === Live Readout ===
    auto *readoutRow = new QHBoxLayout;
    readoutRow->setSpacing(6);
    auto addReadout = [&](const QString &name, QLabel *&label) {
        auto *n = new QLabel(name);
        QFont nf(Style::Font::Family); nf.setPixelSize(Style::Font::Small);
        n->setFont(nf);
        n->setStyleSheet(QString("color: %1;").arg(Style::Color::TextGray));
        readoutRow->addWidget(n);
        label = new QLabel("--");
        QFont vf(Style::Font::Monospace); vf.setPixelSize(Style::Font::Medium); vf.setBold(true);
        label->setFont(vf);
        label->setStyleSheet(QString("color: %1;").arg(Style::Color::TextWhite));
        label->setMinimumWidth(36);
        readoutRow->addWidget(label);
    };
    addReadout("Freq", m_freqLabel);
    addReadout("Pwr", m_pwrLabel);
    addReadout("Z", m_zLabel);
    addReadout("Ph", m_phLabel);
    addReadout("R", m_rLabel);
    addReadout("X", m_xLabel);
    addReadout("SWR", m_swrLabel);
    readoutRow->addStretch();
    mainLayout->addLayout(readoutRow);

    // === Status Bar ===
    auto *statusRow = new QHBoxLayout;
    statusRow->setSpacing(4);
    m_rawLabel = new QLabel;
    QFont rawFont(Style::Font::Monospace); rawFont.setPixelSize(Style::Font::Tiny);
    m_rawLabel->setFont(rawFont);
    m_rawLabel->setStyleSheet(
        QString("color: %1; background: %2; border: 1px solid %3; padding: 1px 3px;")
            .arg(Style::Color::InactiveGray, Style::Color::DarkBackground, Style::Color::PanelBorder));

    m_statusLabel = new QLabel("Ready");
    m_statusLabel->setFont(rawFont);
    m_statusLabel->setStyleSheet(QString("color: %1;").arg(Style::Color::TextGray));

    statusRow->addWidget(m_rawLabel, 1);
    statusRow->addWidget(m_statusLabel);
    mainLayout->addLayout(statusRow);

    // === Connections ===
    connect(m_displayCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PlotWidget::onDisplayModeChanged);
    connect(m_splineCheck, &QCheckBox::toggled, this, &PlotWidget::updateChart);
    connect(m_bestFitCheck, &QCheckBox::toggled, this, &PlotWidget::updateChart);
    connect(m_signCheck, &QCheckBox::toggled, this, [this](bool checked) {
        if (checked && !m_data.isEmpty()) { m_data.autoCorrectSigns(); updateChart(); }
    });

    // Rig connect button
    connect(m_rigConnectBtn, &QPushButton::clicked, this, [this]() {
        if (m_rig && m_rig->isOpen()) {
            emit rigDisconnectRequested();
            m_rigConnectBtn->setText("Connect Rig");
            m_statusLabel->setText("Rig disconnected");
        } else {
            // Get rig model ID — findText handles editable combo text matching
            int idx = m_rigCombo->currentIndex();
            int modelId = (idx >= 0) ? m_rigCombo->itemData(idx).toInt() : 0;
            if (modelId == 0) {
                // Try to find by text match
                QString typed = m_rigCombo->currentText().trimmed();
                for (int i = 0; i < m_rigCombo->count(); ++i) {
                    if (m_rigCombo->itemText(i).contains(typed, Qt::CaseInsensitive)) {
                        modelId = m_rigCombo->itemData(i).toInt();
                        break;
                    }
                }
            }
            if (modelId == 0) {
                m_statusLabel->setText("Select a rig model first");
                return;
            }
            // Read port directly from the line edit to avoid stale combo state
            QString port = m_rigPortCombo->lineEdit()
                ? m_rigPortCombo->lineEdit()->text().trimmed()
                : m_rigPortCombo->currentText().trimmed();
            if (port.isEmpty()) {
                m_statusLabel->setText("Enter a port or IP:port");
                return;
            }
            int baud = m_rigBaudCombo->currentText().toInt();
            m_statusLabel->setText(QString("Connecting model=%1 port=%2 baud=%3...")
                .arg(modelId).arg(port).arg(baud));
            QApplication::processEvents();
            emit rigConnectRequested(modelId, port, baud);

            if (m_rig && m_rig->isOpen()) {
                m_rigConnectBtn->setText("Disconnect Rig");
                m_statusLabel->setText("Rig: " + m_rig->rigName());
            } else {
                // Show the actual Hamlib error
                QString err = m_rig ? m_rig->errorString() : "m_rig is null";
                m_statusLabel->setText("FAILED: " + err);
            }
        }
    });

    // Run button
    connect(m_runBtn, &QPushButton::clicked, this, [this]() {
        if (!m_rig || !m_rig->isOpen()) {
            m_statusLabel->setText("Rig not connected");
            return;
        }
        if (!m_lp100a) {
            m_statusLabel->setText("LP-100A not connected");
            return;
        }
        if (m_sweepEngine && m_sweepEngine->isRunning()) return;

        m_data.clear();
        updateChart();

        SweepParams params;
        params.startMHz = startFreqMHz();
        params.stopMHz = stopFreqMHz();
        params.stepKHz = stepKHz();
        params.settleTimeMs = m_settleCombo->currentData().toInt();
        params.sampleTimeMs = m_sampleCombo->currentData().toInt();
        params.txMode = m_txModeCombo->currentText();
        params.rfPower = m_powerSpin->value() / 100.0;  // UI shows %, Hamlib wants 0.0-1.0

        m_sweepEngine = new SweepEngine(m_rig, m_lp100a, this);
        connect(m_sweepEngine, &SweepEngine::pointMeasured, this, [this](const SweepPoint &pt) {
            m_data.addPoint(pt); updateChart(); updateReadout(pt);
        });
        connect(m_sweepEngine, &SweepEngine::progress, this,
                [this](int cur, int total, double freq) {
            m_statusLabel->setText(QString("Sweeping %1 MHz (%2/%3)")
                .arg(freq, 0, 'f', 3).arg(cur).arg(total));
        });
        connect(m_sweepEngine, &SweepEngine::sweepFinished, this, [this]() {
            m_statusLabel->setText(QString("Complete — %1 pts").arg(m_data.count()));
            if (m_signCheck->isChecked()) { m_data.autoCorrectSigns(); updateChart(); }
            m_sweepEngine->deleteLater(); m_sweepEngine = nullptr;
        });
        connect(m_sweepEngine, &SweepEngine::sweepError, this, [this](const QString &err) {
            m_statusLabel->setText("Error: " + err);
            m_sweepEngine->deleteLater(); m_sweepEngine = nullptr;
        });
        m_sweepEngine->start(params);
    });

    connect(m_stopBtn, &QPushButton::clicked, this, [this]() {
        if (m_sweepEngine) m_sweepEngine->stop();
        m_statusLabel->setText("Stopped");
    });

    connect(m_resetBtn, &QPushButton::clicked, this, [this]() {
        m_data.clear();
        m_chart->zoomReset();
        updateChart(); saveSettings();
        m_statusLabel->setText("Ready");
    });

    // Export buttons
    connect(m_exportCsvBtn, &QPushButton::clicked, this, [this]() {
        if (m_data.isEmpty()) { m_statusLabel->setText("No data to export"); return; }
        QString path = QFileDialog::getSaveFileName(this, "Export CSV", "", "CSV Files (*.csv)");
        if (!path.isEmpty()) {
            m_data.exportCsv(path);
            m_statusLabel->setText("Exported: " + path);
        }
    });
    connect(m_exportImgBtn, &QPushButton::clicked, this, [this]() {
        QString path = QFileDialog::getSaveFileName(this, "Save Image", "", "PNG Images (*.png)");
        if (!path.isEmpty()) {
            QWidget *active = (m_displayMode == SmithChart)
                ? static_cast<QWidget *>(m_smithChart)
                : static_cast<QWidget *>(m_chartView);
            QPixmap pixmap = active->grab();
            pixmap.save(path);
            m_statusLabel->setText("Saved: " + path);
        }
    });

    // Restore saved settings
    loadSettings();
}

void PlotWidget::closeEvent(QCloseEvent *event) {
    saveSettings();
    QWidget::closeEvent(event);
}

void PlotWidget::setLP100A(LP100AProtocol *lp100a) {
    if (m_lp100a) {
        disconnect(m_lp100a, nullptr, m_rawLabel, nullptr);
    }
    m_lp100a = lp100a;
    if (m_lp100a) {
        connect(m_lp100a, &LP100AProtocol::rawResponse, m_rawLabel, &QLabel::setText);
    }
}

void PlotWidget::saveSettings() {
    QSettings s("AI5QK", "QK-LP100A");
    s.setValue("plot/startFreq", m_startFreq->value());
    s.setValue("plot/stopFreq", m_stopFreq->value());
    s.setValue("plot/stepIdx", m_stepCombo->currentIndex());
    s.setValue("plot/displayIdx", m_displayCombo->currentIndex());
    s.setValue("plot/sampleIdx", m_sampleCombo->currentIndex());
    s.setValue("plot/settleIdx", m_settleCombo->currentIndex());
    s.setValue("plot/txMode", m_txModeCombo->currentText());
    s.setValue("plot/rfPowerPct", m_powerSpin->value());
    s.setValue("plot/sign", m_signCheck->isChecked());
    s.setValue("plot/spline", m_splineCheck->isChecked());
    s.setValue("plot/bestFit", m_bestFitCheck->isChecked());
    s.setValue("plot/rigIdx", m_rigCombo->currentIndex());
    s.setValue("plot/rigPort", m_rigPortCombo->currentText());
    s.setValue("plot/rigBaud", m_rigBaudCombo->currentText());
}

void PlotWidget::loadSettings() {
    QSettings s("AI5QK", "QK-LP100A");
    m_startFreq->setValue(s.value("plot/startFreq", 28.0).toDouble());
    m_stopFreq->setValue(s.value("plot/stopFreq", 29.7).toDouble());
    m_stepCombo->setCurrentIndex(s.value("plot/stepIdx", 2).toInt());
    m_displayCombo->setCurrentIndex(s.value("plot/displayIdx", 2).toInt());
    m_sampleCombo->setCurrentIndex(s.value("plot/sampleIdx", 2).toInt());
    m_settleCombo->setCurrentIndex(s.value("plot/settleIdx", 3).toInt());
    m_txModeCombo->setCurrentText(s.value("plot/txMode", "CW").toString());
    m_powerSpin->setValue(s.value("plot/rfPowerPct", 10).toDouble());
    m_signCheck->setChecked(s.value("plot/sign", true).toBool());
    m_splineCheck->setChecked(s.value("plot/spline", false).toBool());
    m_bestFitCheck->setChecked(s.value("plot/bestFit", false).toBool());

    int rigIdx = s.value("plot/rigIdx", -1).toInt();
    if (rigIdx >= 0 && rigIdx < m_rigCombo->count())
        m_rigCombo->setCurrentIndex(rigIdx);

    QString rigPort = s.value("plot/rigPort", "").toString();
    if (!rigPort.isEmpty())
        m_rigPortCombo->setCurrentText(rigPort);

    QString rigBaud = s.value("plot/rigBaud", "38400").toString();
    m_rigBaudCombo->setCurrentText(rigBaud);
}

void PlotWidget::createControls() {
    m_startFreq = new QDoubleSpinBox;
    m_startFreq->setRange(0.1, 60.0);
    m_startFreq->setDecimals(3);
    m_startFreq->setSuffix(" MHz");
    m_startFreq->setValue(28.0);
    m_startFreq->setFixedWidth(130);

    m_stopFreq = new QDoubleSpinBox;
    m_stopFreq->setRange(0.1, 60.0);
    m_stopFreq->setDecimals(3);
    m_stopFreq->setSuffix(" MHz");
    m_stopFreq->setValue(29.7);
    m_stopFreq->setFixedWidth(130);

    m_stepCombo = new QComboBox;
    m_stepCombo->addItem("1 kHz", 1);
    m_stepCombo->addItem("2 kHz", 2);
    m_stepCombo->addItem("5 kHz", 5);
    m_stepCombo->addItem("10 kHz", 10);
    m_stepCombo->addItem("25 kHz", 25);
    m_stepCombo->addItem("50 kHz", 50);
    m_stepCombo->addItem("100 kHz", 100);
    m_stepCombo->addItem("200 kHz", 200);
    m_stepCombo->addItem("500 kHz", 500);
    m_stepCombo->setCurrentIndex(5); // 50 kHz default

    m_displayCombo = new QComboBox;
    m_displayCombo->addItem("R + jX", RplusJX);
    m_displayCombo->addItem("Z Magnitude & Phase", ZPhase);
    m_displayCombo->addItem("Standing Wave Ratio", SwrMode);
    m_displayCombo->addItem("Return Loss", ReturnLoss);
    m_displayCombo->addItem("Reflection Coefficient", ReflCoeff);
    m_displayCombo->addItem("Smith Chart", SmithChart);
    m_displayCombo->setCurrentIndex(2);
    QFont comboFont(Style::Font::Family);
    comboFont.setPixelSize(14);
    comboFont.setBold(true);
    m_displayCombo->setFont(comboFont);

    m_displayCombo->setMinimumWidth(220);
    m_displayCombo->setStyleSheet(QString(
        "QComboBox { background: %1; color: %2; border: 1px solid %3;"
        " border-radius: 4px; padding: 2px 8px; }"
        "QComboBox::drop-down { border: none; }"
        "QComboBox QAbstractItemView { background: %1; color: %2;"
        " selection-background-color: %4; min-width: 220px; }")
        .arg(Style::Color::DarkBackground, Style::Color::TextWhite,
             Style::Color::PanelBorder, Style::Color::AccentAmber));

    m_runBtn = new QPushButton("Run");
    m_resetBtn = new QPushButton("Reset");
    m_stopBtn = new QPushButton("Stop");
    for (auto *btn : {m_runBtn, m_resetBtn, m_stopBtn}) {
        btn->setStyleSheet(Style::buttonCompact());
        btn->setFixedHeight(Style::Layout::CompactButtonHeight);
    }

    m_signCheck = new QCheckBox("Sign");
    m_splineCheck = new QCheckBox("Spline");
    m_bestFitCheck = new QCheckBox("Best Fit");
    m_signCheck->setChecked(true);

    m_exportCsvBtn = new QPushButton("CSV");
    m_exportImgBtn = new QPushButton("Image");
    m_exportCsvBtn->setStyleSheet(Style::buttonCompact());
    m_exportImgBtn->setStyleSheet(Style::buttonCompact());
    m_exportCsvBtn->setFixedHeight(Style::Layout::CompactButtonHeight);
    m_exportImgBtn->setFixedHeight(Style::Layout::CompactButtonHeight);

    // Sweep settings
    m_sampleCombo = new QComboBox;
    m_sampleCombo->addItem("100ms", 100);
    m_sampleCombo->addItem("500ms", 500);
    m_sampleCombo->addItem("1 sec", 1000);
    m_sampleCombo->addItem("2 sec", 2000);
    m_sampleCombo->addItem("3 sec", 3000);
    m_sampleCombo->setCurrentIndex(2);

    m_settleCombo = new QComboBox;
    m_settleCombo->addItem("50ms", 50);
    m_settleCombo->addItem("100ms", 100);
    m_settleCombo->addItem("500ms", 500);
    m_settleCombo->addItem("1 sec", 1000);
    m_settleCombo->addItem("2 sec", 2000);
    m_settleCombo->addItem("3 sec", 3000);
    m_settleCombo->addItem("5 sec", 5000);
    m_settleCombo->setCurrentIndex(3);

    m_txModeCombo = new QComboBox;
    m_txModeCombo->addItems({"CW", "AM", "FSK", "USB", "LSB"});

    // Rig setup (inline, like the Windows Plot bottom row)
    m_rigCombo = new QComboBox;
    m_rigCombo->setEditable(true);
    m_rigCombo->setInsertPolicy(QComboBox::NoInsert);
    m_rigCombo->setMinimumWidth(140);
    // Populate rig list
    auto rigs = HamlibRig::availableRigs();
    for (const auto &rig : rigs) {
        QString display = QString("%1 %2").arg(rig.manufacturer, rig.model).trimmed();
        m_rigCombo->addItem(display, rig.modelId);
        m_rigList.append({rig.modelId, display});
    }
    // Default to K4 if available
    for (int i = 0; i < m_rigCombo->count(); ++i) {
        if (m_rigCombo->itemData(i).toInt() == 2047) {
            m_rigCombo->setCurrentIndex(i);
            break;
        }
    }

    m_rigPortCombo = new QComboBox;
    m_rigPortCombo->setEditable(true);
    m_rigPortCombo->setMinimumWidth(160);
    for (const auto &port : SerialTransport::availablePorts())
        m_rigPortCombo->addItem(port);

    m_rigBaudCombo = new QComboBox;
    m_rigBaudCombo->addItems({"4800", "9600", "19200", "38400", "57600", "115200"});
    m_rigBaudCombo->setCurrentText("38400");

    m_powerSpin = new QDoubleSpinBox;
    m_powerSpin->setRange(1, 100);
    m_powerSpin->setSingleStep(5);
    m_powerSpin->setValue(10);
    m_powerSpin->setDecimals(0);
    m_powerSpin->setSuffix("% Pwr");

    m_rigConnectBtn = new QPushButton("Connect Rig");
    m_rigConnectBtn->setStyleSheet(Style::buttonCompact());
    m_rigConnectBtn->setFixedHeight(Style::Layout::CompactButtonHeight);
}

void PlotWidget::createChart() {
    m_chart = new QChart;
    m_chart->setBackgroundBrush(QColor(Style::Color::Background));
    m_chart->setPlotAreaBackgroundBrush(QColor(Style::Color::DarkBackground));
    m_chart->setPlotAreaBackgroundVisible(true);
    m_chart->legend()->hide();
    m_chart->setMargins(QMargins(4, 4, 4, 4));

    m_chart->setTitle(""); // Title replaced by display mode combo above chart

    m_axisX = new QValueAxis;
    m_axisX->setTitleText("Frequency - MHz");
    m_axisX->setLabelFormat("%.3f");
    m_axisX->setRange(28.0, 29.8);
    m_axisX->setGridLineColor(QColor(Style::Color::PanelBorder));
    m_axisX->setLabelsColor(QColor(Style::Color::TextGray));
    m_axisX->setTitleBrush(QColor(Style::Color::TextGray));
    m_chart->addAxis(m_axisX, Qt::AlignBottom);

    m_axisY1 = new QValueAxis;
    m_axisY1->setRange(1.0, 3.0);
    m_axisY1->setLabelFormat("%.1f");
    m_axisY1->setGridLineColor(QColor(Style::Color::PanelBorder));
    m_axisY1->setLabelsColor(QColor(Style::Color::TextGray));
    m_axisY1->setTitleBrush(QColor(Style::Color::TextGray));
    m_chart->addAxis(m_axisY1, Qt::AlignLeft);

    m_axisY2 = new QValueAxis;
    m_axisY2->setGridLineColor(QColor(Style::Color::PanelBorder));
    m_axisY2->setLabelsColor(QColor(Style::Color::TextGray));
    m_axisY2->setTitleBrush(QColor(Style::Color::TextGray));
    m_chart->addAxis(m_axisY2, Qt::AlignRight);
    m_axisY2->setVisible(false);

    m_chartView = new QChartView(m_chart);
    m_chartView->setRenderHint(QPainter::Antialiasing);
    m_chartView->setRubberBand(QChartView::RectangleRubberBand);
    m_chartView->setMinimumSize(200, 150);
    m_chartView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // === Dual chart for Z/Phase and R+jX ===
    auto makeSubChart = [this](const QString &, const QString &yLabel) {
        auto *chart = new QChart;
        chart->setBackgroundBrush(QColor(Style::Color::Background));
        chart->setPlotAreaBackgroundBrush(QColor(Style::Color::DarkBackground));
        chart->setPlotAreaBackgroundVisible(true);
        chart->setMargins(QMargins(4, 2, 4, 2));
        // Legend shows series name (Magnitude/Phase/Resistance/Reactance)
        chart->legend()->show();
        chart->legend()->setAlignment(Qt::AlignTop);
        chart->legend()->setLabelColor(QColor(Style::Color::TextGray));
        QFont lf(Style::Font::Family);
        lf.setPixelSize(Style::Font::Small);
        chart->legend()->setFont(lf);

        auto *axisX = new QValueAxis;
        axisX->setLabelFormat("%.3f");
        axisX->setRange(28.0, 29.8);
        axisX->setGridLineColor(QColor(Style::Color::PanelBorder));
        axisX->setLabelsColor(QColor(Style::Color::TextGray));
        axisX->setTitleBrush(QColor(Style::Color::TextGray));
        chart->addAxis(axisX, Qt::AlignBottom);

        auto *axisY = new QValueAxis;
        axisY->setTitleText(yLabel);
        axisY->setLabelFormat("%.1f");
        axisY->setGridLineColor(QColor(Style::Color::PanelBorder));
        axisY->setLabelsColor(QColor(Style::Color::TextGray));
        axisY->setTitleBrush(QColor(Style::Color::TextGray));
        chart->addAxis(axisY, Qt::AlignLeft);

        auto *view = new QChartView(chart);
        view->setRenderHint(QPainter::Antialiasing);
        view->setRubberBand(QChartView::RectangleRubberBand);

        return std::make_tuple(chart, view, axisX, axisY);
    };

    auto [ct, cvt, axt, ayt] = makeSubChart("Magnitude", "Ohms");
    m_chartTop = ct; m_chartViewTop = cvt; m_axisXTop = axt; m_axisYTop = ayt;

    auto [cb, cvb, axb, ayb] = makeSubChart("Phase", "Degrees");
    m_chartBot = cb; m_chartViewBot = cvb; m_axisXBot = axb; m_axisYBot = ayb;

    // Top chart: hide X axis labels (shared with bottom)
    m_axisXTop->setLabelsVisible(false);
    m_axisXTop->setTitleText("");

    // Bottom chart: show frequency label
    m_axisXBot->setTitleText("Frequency - MHz");

    // Main title label above both charts
    m_dualTitle = new QLabel("Z Magnitude & Phase");
    auto *dualTitle = m_dualTitle;
    dualTitle->setAlignment(Qt::AlignCenter);
    QFont dtFont(Style::Font::Family);
    dtFont.setPixelSize(14);
    dtFont.setBold(true);
    dualTitle->setFont(dtFont);
    dualTitle->setStyleSheet(QString("color: %1;").arg(Style::Color::TextWhite));

    m_dualChartWidget = new QWidget;
    auto *dualLayout = new QVBoxLayout(m_dualChartWidget);
    dualLayout->setSpacing(0);
    dualLayout->setContentsMargins(0, 0, 0, 0);
    dualLayout->addWidget(dualTitle);
    dualLayout->addWidget(m_chartViewTop, 1);
    dualLayout->addWidget(m_chartViewBot, 1);

    // Adjust axis label precision when zoom changes range
    connect(m_axisX, &QValueAxis::rangeChanged, this, [this](qreal min, qreal max) {
        double span = max - min;
        if (span < 0.1)       m_axisX->setLabelFormat("%.4f");
        else if (span < 1.0)  m_axisX->setLabelFormat("%.3f");
        else                  m_axisX->setLabelFormat("%.2f");
        m_axisX->setTickCount(qBound(4, static_cast<int>(span / 0.01) + 1, 12));
    });
    connect(m_axisY1, &QValueAxis::rangeChanged, this, [this](qreal min, qreal max) {
        double span = max - min;
        if (span < 0.5)       m_axisY1->setLabelFormat("%.3f");
        else if (span < 5.0)  m_axisY1->setLabelFormat("%.2f");
        else                  m_axisY1->setLabelFormat("%.1f");
    });
    connect(m_axisY2, &QValueAxis::rangeChanged, this, [this](qreal min, qreal max) {
        double span = max - min;
        if (span < 5.0)  m_axisY2->setLabelFormat("%.2f");
        else             m_axisY2->setLabelFormat("%.1f");
    });
}

double PlotWidget::startFreqMHz() const { return m_startFreq->value(); }
double PlotWidget::stopFreqMHz() const { return m_stopFreq->value(); }
double PlotWidget::stepKHz() const { return m_stepCombo->currentData().toDouble(); }

void PlotWidget::onDisplayModeChanged() {
    m_displayMode = static_cast<DisplayMode>(m_displayCombo->currentData().toInt());
    updateChart();
}

void PlotWidget::updateReadout(const SweepPoint &pt) {
    m_freqLabel->setText(QString::number(pt.freqMHz, 'f', 3));
    m_pwrLabel->setText(QString::number(pt.power, 'f', 1));
    m_zLabel->setText(QString::number(pt.impedance, 'f', 1));
    m_phLabel->setText(QString::number(pt.phase, 'f', 1));
    m_rLabel->setText(QString::number(pt.resistance, 'f', 1));
    m_xLabel->setText(QString::number(pt.reactance, 'f', 1));
    m_swrLabel->setText(QString::number(pt.swr, 'f', 2));
}

void PlotWidget::updateChart() {
    // Switch between single chart, dual chart, and Smith Chart
    if (m_displayMode == SmithChart) {
        m_chartStack->setCurrentIndex(2);
        m_smithChart->setData(&m_data);
        return;
    }
    if (m_displayMode == ZPhase || m_displayMode == RplusJX) {
        m_chartStack->setCurrentIndex(1);
        m_chartTop->removeAllSeries();
        m_chartBot->removeAllSeries();

        double fMin = m_startFreq->value();
        double fMax = m_stopFreq->value();
        double pad = qMax(0.005, stepKHz() / 2000.0);
        m_axisXTop->setRange(fMin - pad, fMax + pad);
        m_axisXBot->setRange(fMin - pad, fMax + pad);

        if (m_displayMode == ZPhase) {
            m_dualTitle->setText("Z Magnitude & Phase");
            m_chartTop->setTitle(""); // Title is in m_dualTitle label
            m_chartBot->setTitle("");
            m_axisYTop->setTitleText("Ohms");
            m_axisYBot->setTitleText("Degrees");
            m_axisYTop->setRange(0, 100);
            m_axisYBot->setRange(-90, 90);
        } else {
            m_dualTitle->setText("R + jX");
            m_chartTop->setTitle("");
            m_chartBot->setTitle("");
            m_axisYTop->setTitleText("Ohms");
            m_axisYBot->setTitleText("Ohms");
            m_axisYTop->setRange(0, 100);
            m_axisYBot->setRange(-50, 50);
        }
    } else {
        m_chartStack->setCurrentIndex(0);
    }

    m_chart->removeAllSeries();
    m_chart->legend()->hide();
    m_axisY2->setVisible(false);

    switch (m_displayMode) {
        case SwrMode:
            m_axisY1->setTitleText("SWR"); m_axisY1->setRange(1.0, 3.0); break;
        case RplusJX:
            m_axisY1->setTitleText("Ohms"); m_axisY2->setTitleText("Ohms");
            m_axisY2->setVisible(true);
            m_axisY1->setRange(0, 100); m_axisY2->setRange(-50, 50);
            m_chart->legend()->show(); break;
        case ZPhase:
            m_axisY1->setTitleText("Ohms"); m_axisY2->setTitleText("Degrees");
            m_axisY2->setVisible(true);
            m_axisY1->setRange(0, 100); m_axisY2->setRange(-90, 90);
            m_chart->legend()->show(); break;
        case ReturnLoss:
            m_axisY1->setTitleText("dB"); m_axisY1->setRange(0, 30); break;
        case ReflCoeff:
            m_axisY1->setTitleText("|Γ|"); m_axisY1->setRange(0, 1.0); break;
        case SmithChart:
            break; // Handled above
    }

    double fMin = m_startFreq->value();
    double fMax = m_stopFreq->value();

    if (!m_data.isEmpty()) {
        fMin = m_data.at(0).freqMHz;
        fMax = m_data.at(m_data.count() - 1).freqMHz;
    }

    m_axisX->setLabelFormat("%.3f");  // Always kHz resolution for ham radio
    double stepMHz = stepKHz() / 1000.0;
    double pad = qMax(0.005, stepMHz / 2.0);  // Pad by half a step
    m_axisX->setRange(fMin - pad, fMax + pad);

    if (m_data.isEmpty()) return;

    switch (m_displayMode) {
        case SwrMode:     plotSWR(); break;
        case RplusJX:     plotRplusJX(); break;
        case ZPhase:      plotZPhase(); break;
        case ReturnLoss:  plotReturnLoss(); break;
        case ReflCoeff:   plotReflCoeff(); break;
        case SmithChart:  break;
    }
}

// Helper: add a data series (line or spline or best-fit) + scatter dots
void PlotWidget::addSeries(const QVector<double> &xData, const QVector<double> &yData,
                           const QColor &color, QValueAxis *yAxis) {
    // Scatter dots (raw data points)
    auto *dots = new QScatterSeries;
    dots->setMarkerSize(6);
    dots->setColor(color);
    dots->setBorderColor(color);
    for (int i = 0; i < xData.size(); ++i)
        dots->append(xData[i], yData[i]);

    // Line/spline/best-fit curve
    QXYSeries *curve;
    if (m_bestFitCheck->isChecked() && xData.size() >= 5) {
        // 4th order polynomial best fit
        auto coeffs = SweepData::polyFit(xData, yData, 4);
        curve = new QLineSeries;
        double xMin = xData.first(), xMax = xData.last();
        int numPts = 200;
        for (int i = 0; i <= numPts; ++i) {
            double x = xMin + (xMax - xMin) * i / numPts;
            double y = 0;
            for (int j = 0; j < coeffs.size(); ++j)
                y += coeffs[j] * std::pow(x, j);
            curve->append(x, y);
        }
    } else if (m_splineCheck->isChecked()) {
        curve = new QSplineSeries;
        for (int i = 0; i < xData.size(); ++i)
            curve->append(xData[i], yData[i]);
    } else {
        curve = new QLineSeries;
        for (int i = 0; i < xData.size(); ++i)
            curve->append(xData[i], yData[i]);
    }
    curve->setPen(QPen(color, 2));

    m_chart->addSeries(curve);
    m_chart->addSeries(dots);
    curve->attachAxis(m_axisX);
    curve->attachAxis(yAxis);
    dots->attachAxis(m_axisX);
    dots->attachAxis(yAxis);
}

void PlotWidget::plotSWR() {
    QVector<double> x, y;
    double yMin = 999, yMax = 0;
    for (int i = 0; i < m_data.count(); ++i) {
        x.append(m_data.at(i).freqMHz);
        y.append(m_data.at(i).swr);
        yMin = qMin(yMin, m_data.at(i).swr);
        yMax = qMax(yMax, m_data.at(i).swr);
    }
    addSeries(x, y, QColor(Style::Color::AccentCyan), m_axisY1);
    m_axisY1->setRange(qMax(1.0, yMin - 0.2), yMax + 0.2);
}

// Helper to add series to a specific chart (for dual chart mode)
void PlotWidget::addDualSeries(QChart *chart, QValueAxis *axisX, QValueAxis *axisY,
                                const QVector<double> &xData, const QVector<double> &yData,
                                const QColor &color, const QString &name) {
    QXYSeries *curve;
    if (m_bestFitCheck->isChecked() && xData.size() >= 5) {
        auto coeffs = SweepData::polyFit(xData, yData, 4);
        curve = new QLineSeries;
        double xMin = xData.first(), xMax = xData.last();
        for (int i = 0; i <= 200; ++i) {
            double x = xMin + (xMax - xMin) * i / 200.0;
            double y = 0;
            for (int j = 0; j < coeffs.size(); ++j) y += coeffs[j] * std::pow(x, j);
            curve->append(x, y);
        }
    } else if (m_splineCheck->isChecked()) {
        curve = new QSplineSeries;
        for (int i = 0; i < xData.size(); ++i) curve->append(xData[i], yData[i]);
    } else {
        curve = new QLineSeries;
        for (int i = 0; i < xData.size(); ++i) curve->append(xData[i], yData[i]);
    }
    curve->setName(name);
    curve->setPen(QPen(color, 2));

    auto *dots = new QScatterSeries;
    dots->setMarkerSize(5); dots->setColor(color); dots->setBorderColor(color);
    for (int i = 0; i < xData.size(); ++i) dots->append(xData[i], yData[i]);

    chart->addSeries(curve); chart->addSeries(dots);
    curve->attachAxis(axisX); curve->attachAxis(axisY);
    dots->attachAxis(axisX); dots->attachAxis(axisY);
    // Hide scatter dots from legend
    auto markers = chart->legend()->markers(dots);
    if (!markers.isEmpty()) markers.first()->setVisible(false);
}

void PlotWidget::plotRplusJX() {

    QVector<double> freq, rData, xData;
    double rMin=999, rMax=-999, xMin=999, xMax=-999;
    for (int i = 0; i < m_data.count(); ++i) {
        freq.append(m_data.at(i).freqMHz);
        rData.append(m_data.at(i).resistance);
        xData.append(m_data.at(i).reactance);
        rMin=qMin(rMin,m_data.at(i).resistance); rMax=qMax(rMax,m_data.at(i).resistance);
        xMin=qMin(xMin,m_data.at(i).reactance); xMax=qMax(xMax,m_data.at(i).reactance);
    }

    double fMin = freq.first(), fMax = freq.last();
    double pad = qMax(0.005, stepKHz() / 2000.0);
    m_axisXTop->setRange(fMin - pad, fMax + pad);
    m_axisXBot->setRange(fMin - pad, fMax + pad);
    m_axisYTop->setRange(qMax(0.0, rMin-10), rMax+10);
    m_axisYBot->setRange(xMin-10, xMax+10);

    addDualSeries(m_chartTop, m_axisXTop, m_axisYTop, freq, rData, QColor(Style::Color::AccentCyan), "Resistance");
    addDualSeries(m_chartBot, m_axisXBot, m_axisYBot, freq, xData, QColor(Style::Color::StatusRed), "Reactance");
}

void PlotWidget::plotZPhase() {

    QVector<double> freq, zData, pData;
    double zMin=999, zMax=0, pMin=999, pMax=-999;
    for (int i = 0; i < m_data.count(); ++i) {
        freq.append(m_data.at(i).freqMHz);
        zData.append(m_data.at(i).impedance);
        pData.append(m_data.at(i).phase);
        zMin=qMin(zMin,m_data.at(i).impedance); zMax=qMax(zMax,m_data.at(i).impedance);
        pMin=qMin(pMin,m_data.at(i).phase); pMax=qMax(pMax,m_data.at(i).phase);
    }

    double fMin = freq.first(), fMax = freq.last();
    double pad = qMax(0.005, stepKHz() / 2000.0);
    m_axisXTop->setRange(fMin - pad, fMax + pad);
    m_axisXBot->setRange(fMin - pad, fMax + pad);
    m_axisYTop->setRange(qMax(0.0, zMin-10), zMax+10);
    m_axisYBot->setRange(pMin-10, pMax+10);

    addDualSeries(m_chartTop, m_axisXTop, m_axisYTop, freq, zData, QColor(Style::Color::AccentCyan), "Magnitude");
    addDualSeries(m_chartBot, m_axisXBot, m_axisYBot, freq, pData, QColor(Style::Color::StatusRed), "Phase");
}

void PlotWidget::plotReturnLoss() {
    QVector<double> freq, rl;
    double yMin=999, yMax=0;
    for (int i = 0; i < m_data.count(); ++i) {
        freq.append(m_data.at(i).freqMHz);
        double val = SweepData::returnLoss(m_data.at(i).swr);
        rl.append(val);
        yMin=qMin(yMin,val); yMax=qMax(yMax,val);
    }
    addSeries(freq, rl, QColor(Style::Color::AccentCyan), m_axisY1);
    m_axisY1->setRange(qMax(0.0, yMin-2), yMax+2);
}

void PlotWidget::plotReflCoeff() {
    QVector<double> freq, gamma;
    for (int i = 0; i < m_data.count(); ++i) {
        freq.append(m_data.at(i).freqMHz);
        gamma.append(SweepData::reflectionCoeff(m_data.at(i).swr));
    }
    addSeries(freq, gamma, QColor(Style::Color::AccentCyan), m_axisY1);
    m_axisY1->setRange(0, 1.0);
}
