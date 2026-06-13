#pragma once

#include <QDialog>
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QRadioButton>
#include <QCheckBox>
#include <QDialogButtonBox>

class ConnectionDialog : public QDialog {
    Q_OBJECT
public:
    enum ConnectionType { Serial, TCP };

    explicit ConnectionDialog(QWidget *parent = nullptr);

    ConnectionType connectionType() const;
    QString serialPort() const;
    QString tcpHost() const;
    quint16 tcpPort() const;
    int pollIntervalMs() const;
    bool debugLogging() const;

    void setConnectionType(ConnectionType type);
    void setSerialPort(const QString &port);
    void setTcpHost(const QString &host);
    void setTcpPort(quint16 port);
    void setPollInterval(int ms);

private:
    void refreshPorts();

    QRadioButton *m_serialRadio;
    QRadioButton *m_tcpRadio;
    QComboBox *m_portCombo;
    QLineEdit *m_hostEdit;
    QSpinBox *m_tcpPortSpin;
    QSpinBox *m_pollSpin;
    QCheckBox *m_debugCheck;
    QDialogButtonBox *m_buttons;
};
