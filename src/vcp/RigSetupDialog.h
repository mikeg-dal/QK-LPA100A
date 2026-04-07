#pragma once

#include <QDialog>
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QDialogButtonBox>
#include "core/HamlibRig.h"

class RigSetupDialog : public QDialog {
    Q_OBJECT
public:
    explicit RigSetupDialog(QWidget *parent = nullptr);

    int rigModelId() const;
    QString rigDisplayName() const;
    QString port() const;        // Serial port path or TCP host:port
    int baudRate() const;
    QString txMode() const;
    double rfPower() const;
    int sampleTimeMs() const;

    void loadFromSettings();
    void saveToSettings();

private:
    void populateRigList();

    QComboBox *m_rigCombo;
    QComboBox *m_portCombo;
    QComboBox *m_baudCombo;
    QComboBox *m_txModeCombo;
    QDoubleSpinBox *m_powerSpin;
    QComboBox *m_sampleTimeCombo;
    QDialogButtonBox *m_buttons;

    QList<HamlibRig::RigInfo> m_rigs;
};
