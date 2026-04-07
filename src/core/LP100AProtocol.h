#pragma once

#include <QString>
#include <QObject>
#include <QTimer>
#include "ITransport.h"

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

    // Commands
    void poll();
    void incrementAlarm();
    void togglePeakAvgTune();

    const LP100AData &lastData() const { return m_lastData; }

signals:
    void dataUpdated(const LP100AData &data);
    void rawResponse(const QString &response);
    void parseError(const QString &error);

private slots:
    void onDataReceived(const QByteArray &data);
    void onPollTimer();

private:
    bool parseResponse(const QByteArray &line);

    ITransport *m_transport;
    QTimer m_pollTimer;
    LP100AData m_lastData;
    QByteArray m_buffer;
};
