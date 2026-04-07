#pragma once

#include <QString>
#include <QObject>
#include <QTimer>
#include "ITransport.h"

struct LP100AData {
    double power = 0.0;
    double impedance = 0.0;
    double phase = 0.0;
    int alarmSetPoint = 0;     // 0=off, 1=1.5, 2=2.0, 3=2.5, 4=3.0
    QString callsign;
    int powerRange = 0;        // 0=High, 1=Mid, 2=Low
    int peakHoldMode = 0;      // 0=Average, 1=Peak Hold
    double dBm = 0.0;
    double swr = 1.0;

    // Derived values
    double resistance() const;
    double reactance() const;
    double reflectedPower() const;  // Calculated from power and SWR

    // Alarm set point as a double (returns -1 for User setting)
    double alarmSWR() const;
    QString alarmString() const;
    QString powerRangeString() const;
    QString modeString() const;
};

class LP100AProtocol : public QObject {
    Q_OBJECT
public:
    explicit LP100AProtocol(ITransport *transport, QObject *parent = nullptr);

    void startPolling(int intervalMs = 80);
    void stopPolling();
    bool isPolling() const;
    int pollInterval() const;
    void setPollInterval(int ms);

    // Commands
    void poll();
    void incrementAlarm();
    void incrementMode();
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

    // Delay between sending a command (A/M/F) and polling for the result,
    // giving the LP-100A firmware time to process the command.
    static constexpr int CommandDelayMs = 50;
};
