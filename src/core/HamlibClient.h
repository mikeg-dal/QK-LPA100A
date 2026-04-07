#pragma once

#include <QObject>
#include <QTcpSocket>
#include <QQueue>

// TCP client for Hamlib's rigctld daemon.
// Provides async rig control: frequency, PTT, mode, power level.
// Protocol: line-oriented text, responses end with "RPRT N" (0=success).
class HamlibClient : public QObject {
    Q_OBJECT
public:
    explicit HamlibClient(QObject *parent = nullptr);

    void connectToRigctld(const QString &host, quint16 port);
    void disconnect();
    bool isConnected() const;

    // Rig control — all async, results via signals
    void setFrequency(qint64 freqHz);
    void getFrequency();
    void setPTT(bool on);
    void getPTT();
    void setMode(const QString &mode, int passband = 0);
    void getMode();
    void setRFPower(double level); // 0.0 - 1.0

    static constexpr quint16 DefaultPort = 4532;

    // Rig model info (parsed from rigctld --list)
    struct RigInfo {
        int modelId;
        QString manufacturer;
        QString model;
        QString status; // "Stable", "Beta", etc.
    };

    // Parse the rig list from rigctld --list output (runs rigctld synchronously)
    static QList<RigInfo> availableRigs();

signals:
    void connected();
    void disconnected();
    void errorOccurred(const QString &msg);
    void frequencyChanged(qint64 freqHz);
    void pttChanged(bool on);
    void modeChanged(const QString &mode, int passband);
    void commandCompleted(bool success, int errorCode);

private slots:
    void onReadyRead();
    void onSocketError(QAbstractSocket::SocketError err);

private:
    enum class PendingCmd { SetFreq, GetFreq, SetPTT, GetPTT, SetMode, GetMode, Other };

    void sendCommand(const QByteArray &cmd, PendingCmd type);
    void processResponse(const QByteArray &line);

    QTcpSocket m_socket;
    QByteArray m_buffer;
    QQueue<PendingCmd> m_pending;
    QStringList m_responseLines;
};
