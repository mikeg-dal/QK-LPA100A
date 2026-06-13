#include "core/LP100AProtocol.h"
#include "Style.h"
#include <QDebug>
#include <cmath>

// --- LP100AData ---

double LP100AData::resistance() const {
    double phaseRad = phase * M_PI / 180.0;
    return impedance * std::cos(phaseRad);
}

double LP100AData::reactance() const {
    double phaseRad = phase * M_PI / 180.0;
    return impedance * std::sin(phaseRad);
}

double LP100AData::reflectedPower() const {
    if (swr <= 1.0 || power <= 0.0) return 0.0;
    double gamma = (swr - 1.0) / (swr + 1.0);
    return power * gamma * gamma;
}

double LP100AData::alarmSWR() const {
    switch (alarmSetPoint) {
        case 0: return 0.0;   // Off
        case 1: return 1.5;
        case 2: return 2.0;
        case 3: return 2.5;
        case 4: return 3.0;
        case 5: return -1.0;  // User-defined (value not in poll response)
        default: return 0.0;
    }
}

QString LP100AData::alarmString() const {
    switch (alarmSetPoint) {
        case 0: return "Off";
        case 1: return "1.5";
        case 2: return "2.0";
        case 3: return "2.5";
        case 4: return "3.0";
        case 5: return "User";
        default: return "?";
    }
}

QString LP100AData::powerRangeString() const {
    switch (powerRange) {
        case 0: return "High";
        case 1: return "Mid";
        case 2: return "Low";
        default: return "?";
    }
}

QString LP100AData::modeString() const {
    switch (peakHoldMode) {
        case 0: return "Avg";
        case 1: return "Peak";
        case 2: return "Tune";
        default: return "?";
    }
}

// --- LP100AProtocol ---

LP100AProtocol::LP100AProtocol(ITransport *transport, QObject *parent)
    : QObject(parent)
    , m_transport(transport)
{
    connect(m_transport, &ITransport::dataReceived,
            this, &LP100AProtocol::onDataReceived);
    connect(&m_pollTimer, &QTimer::timeout,
            this, &LP100AProtocol::onPollTimer);

    m_responseTimeout.setSingleShot(true);
    connect(&m_responseTimeout, &QTimer::timeout,
            this, &LP100AProtocol::onResponseTimeout);

    m_commandTimer.setSingleShot(true);
    connect(&m_commandTimer, &QTimer::timeout,
            this, &LP100AProtocol::onCommandSettle);
}

void LP100AProtocol::startPolling(int intervalMs) {
    m_pollIntervalMs = qBound(Style::Protocol::MinPollMs, intervalMs, Style::Protocol::MaxPollMs);
    m_awaitingResponse = false;
    m_commandQueue.clear();
    m_buffer.clear();
    m_pollTimer.start(m_pollIntervalMs);
    poll();
}

void LP100AProtocol::stopPolling() {
    m_pollTimer.stop();
    m_responseTimeout.stop();
    m_commandTimer.stop();
    m_awaitingResponse = false;
    m_commandQueue.clear();
}

bool LP100AProtocol::isPolling() const {
    return m_pollTimer.isActive();
}

void LP100AProtocol::setPollInterval(int intervalMs) {
    m_pollIntervalMs = qBound(Style::Protocol::MinPollMs, intervalMs, Style::Protocol::MaxPollMs);
    if (m_pollTimer.isActive())
        m_pollTimer.setInterval(m_pollIntervalMs);
}

// Central write choke point: every outbound byte marks the line busy and arms
// the watchdog, so commands and polls can never overlap a response in flight.
void LP100AProtocol::writeToLine(const QByteArray &bytes) {
    m_transport->write(bytes);
    m_awaitingResponse = true;
    m_responseTimeout.start(m_pollIntervalMs + Style::Protocol::ResponseTimeoutMarginMs);
}

void LP100AProtocol::poll() {
    writeToLine(QByteArray("P"));
}

void LP100AProtocol::incrementAlarm() {
    enqueueCommand(QByteArray("A"));
}

void LP100AProtocol::togglePeakAvgTune() {
    enqueueCommand(QByteArray("F"));
}

void LP100AProtocol::enqueueCommand(const QByteArray &cmd) {
    m_commandQueue.enqueue(cmd);
    dispatchQueued();  // Send now if the line is already idle; else onDataReceived will retry
}

void LP100AProtocol::dispatchQueued() {
    if (m_awaitingResponse || m_commandQueue.isEmpty())
        return;
    // A/F are fire-and-forget — the device sends no reply, so we don't arm the
    // response watchdog here. Mark the line busy (blocks poll ticks), then after
    // a short settle delay poll to read the new state. Issued only on an idle line.
    m_transport->write(m_commandQueue.dequeue());
    m_awaitingResponse = true;
    m_commandTimer.start(Style::Protocol::CommandDelayMs);
}

void LP100AProtocol::onDataReceived(const QByteArray &data) {
    m_buffer.append(data);

    bool consumedLine = false;
    while (true) {
        int semiIdx = m_buffer.indexOf(';');
        if (semiIdx < 0) {
            // Don't clear — keep partial data for next chunk
            // Only discard obvious junk (non-response bytes before any semicolon)
            break;
        }

        if (semiIdx > 0)
            m_buffer.remove(0, semiIdx);

        int endIdx = -1;
        for (int i = 1; i < m_buffer.size(); ++i) {
            char c = m_buffer.at(i);
            if (c == '\r' || c == '\n' || c == ';') {
                endIdx = i;
                break;
            }
        }

        if (endIdx < 0)
            break;

        QByteArray line = m_buffer.mid(0, endIdx);
        m_buffer.remove(0, endIdx);

        while (!m_buffer.isEmpty() && (m_buffer.at(0) == '\r' || m_buffer.at(0) == '\n'))
            m_buffer.remove(0, 1);

        emit rawResponse(QString::fromLatin1(line));
        parseResponse(line);
        consumedLine = true;
    }

    // A complete response was framed → the serial line is now idle. Disarm the
    // watchdog and flush any queued user command immediately, so commands are
    // written only between responses and never collide with one in flight.
    if (consumedLine) {
        m_awaitingResponse = false;
        m_responseTimeout.stop();
        dispatchQueued();
    }
}

void LP100AProtocol::onPollTimer() {
    if (!m_transport->isOpen())
        return;
    if (m_awaitingResponse)
        return;  // Line busy: skip this tick rather than stacking a second "P"
    poll();
}

void LP100AProtocol::onResponseTimeout() {
    // No reply arrived within the watchdog window. Unstick the line so the
    // poll loop and command queue can't deadlock on a silent device.
    m_awaitingResponse = false;
    emit responseTimedOut();
    dispatchQueued();
}

void LP100AProtocol::onCommandSettle() {
    // The device has had time to process the A/F command. The line is still
    // marked busy (m_awaitingResponse stayed true); poll now to read the new
    // state — poll() re-arms the response watchdog, and onDataReceived clears
    // the busy flag and flushes the next queued command when the reply lands.
    if (m_transport->isOpen())
        poll();
    else
        m_awaitingResponse = false;
}

bool LP100AProtocol::parseResponse(const QByteArray &line) {
    QString str = QString::fromLatin1(line);
    if (!str.startsWith(';')) {
        emit parseError("Response doesn't start with ';': " + str);
        return false;
    }

    str = str.mid(1);
    QStringList fields = str.split(',');

    if (fields.size() < 9) {
        emit parseError(QString("Expected 9 fields, got %1").arg(fields.size()));
        return false;
    }

    bool ok;
    LP100AData data;

    data.power = fields[0].trimmed().toDouble(&ok);
    if (!ok) { emit parseError("Bad power: " + fields[0]); return false; }

    data.impedance = fields[1].trimmed().toDouble(&ok);
    if (!ok) { emit parseError("Bad impedance: " + fields[1]); return false; }

    data.phase = fields[2].trimmed().toDouble(&ok);
    if (!ok) { emit parseError("Bad phase: " + fields[2]); return false; }

    data.alarmSetPoint = fields[3].trimmed().toInt(&ok);
    if (!ok || data.alarmSetPoint < 0 || data.alarmSetPoint > 5) {
        emit parseError("Bad alarm: " + fields[3]);
        return false;
    }

    data.callsign = fields[4].trimmed();

    data.powerRange = fields[5].trimmed().toInt(&ok);
    if (!ok || data.powerRange < 0 || data.powerRange > 2) {
        emit parseError("Bad range: " + fields[5]);
        return false;
    }

    data.peakHoldMode = fields[6].trimmed().toInt(&ok);
    if (!ok) { emit parseError("Bad mode: " + fields[6]); return false; }

    data.dBm = fields[7].trimmed().toDouble(&ok);
    if (!ok) { emit parseError("Bad dBm: " + fields[7]); return false; }

    data.swr = fields[8].trimmed().toDouble(&ok);
    if (!ok) { emit parseError("Bad SWR: " + fields[8]); return false; }

    m_lastData = data;
    emit dataUpdated(data);
    return true;
}
