#pragma once

#include <QString>
#include <QObject>
#include <QTimer>
#include <QQueue>
#include <QByteArray>
#include "ITransport.h"
#include "Style.h"

struct LP100AData {
    double power = 0.0;
    double impedance = 0.0;
    double phase = 0.0;
    int alarmSetPoint = 0;     // 0=off, 1=1.5, 2=2.0, 3=2.5, 4=3.0, 5=User
    QString callsign;
    int powerRange = 0;        // 0=High, 1=Mid, 2=Low
    int peakHoldMode = 0;      // 0=Average, 1=Peak Hold, 2=Tune
    double dBm = 0.0;
    double swr = 1.0;

    // Derived values
    double resistance() const;
    double reactance() const;
    double reflectedPower() const;

    // Alarm helpers
    double alarmSWR() const;      // Returns SWR threshold, -1 for User, 0 for Off
    QString alarmString() const;  // "Off", "1.5", ..., "User"
    QString powerRangeString() const;
    QString modeString() const;
};

class LP100AProtocol : public QObject {
    Q_OBJECT
public:
    explicit LP100AProtocol(ITransport *transport, QObject *parent = nullptr);

    void startPolling(int intervalMs);
    void stopPolling();
    bool isPolling() const;
    void setPollInterval(int intervalMs);  // Live interval change (no reconnect)

    // Commands
    void poll();
    void incrementAlarm();
    void togglePeakAvgTune();

    const LP100AData &lastData() const { return m_lastData; }

signals:
    void dataUpdated(const LP100AData &data);
    void rawResponse(const QString &response);
    void parseError(const QString &error);
    void responseTimedOut();  // No reply within the watchdog window (device silent)

private slots:
    void onDataReceived(const QByteArray &data);
    void onPollTimer();
    void onResponseTimeout();
    void onCommandSettle();  // After A/F settle delay, poll to read the new device state

private:
    bool parseResponse(const QByteArray &line);
    void writeToLine(const QByteArray &bytes);  // Central write: marks line busy + arms watchdog
    void enqueueCommand(const QByteArray &cmd);
    void dispatchQueued();                       // Send next queued command if the line is idle

    ITransport *m_transport;
    QTimer m_pollTimer;
    QTimer m_responseTimeout;            // Single owned, cancelable watchdog
    QTimer m_commandTimer;               // Settle delay between an A/F command and its follow-up poll
    LP100AData m_lastData;
    QByteArray m_buffer;
    QQueue<QByteArray> m_commandQueue;   // Pending user commands ("A","F"), FIFO
    bool m_awaitingResponse = false;     // A write is outstanding → serial line busy
    int m_pollIntervalMs = Style::Protocol::DefaultPollMs;  // Cached for live edit + watchdog
};
