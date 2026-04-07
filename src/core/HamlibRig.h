#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <hamlib/rig.h>

// Direct Hamlib C API wrapper — our app IS the rig controller.
// No external rigctld needed. Opens the rig directly via serial or network.
class HamlibRig : public QObject {
    Q_OBJECT
public:
    explicit HamlibRig(QObject *parent = nullptr);
    ~HamlibRig();

    // Rig model info (from Hamlib's built-in list)
    struct RigInfo {
        int modelId;
        QString manufacturer;
        QString model;
        QString version;
        QString status;
    };

    // Get the full list of supported rigs (call once, cache result)
    static QList<RigInfo> availableRigs();

    // Connection
    bool open(int modelId, const QString &port, int baudRate);
    void close();
    bool isOpen() const;

    // Rig control
    bool setFrequency(double freqHz);
    double getFrequency();
    bool setPTT(bool on);
    bool getPTT();
    bool setMode(const QString &mode, int passband = 0);
    QString getMode();
    bool setRFPower(double level); // 0.0-1.0
    bool sendMorse(const QString &text);

    // Info
    QString errorString() const { return m_errorString; }
    QString rigName() const;

signals:
    void opened();
    void closed();
    void errorOccurred(const QString &msg);

private:
    RIG *m_rig = nullptr;
    QString m_errorString;

    void setError(int retcode, const QString &context);
};
