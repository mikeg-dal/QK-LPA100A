#include "core/LP100AProtocol.h"
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
    // Ref = Fwd * ((SWR - 1) / (SWR + 1))^2
    if (swr <= 1.0 || power <= 0.0) return 0.0;
    double gamma = (swr - 1.0) / (swr + 1.0);
    return power * gamma * gamma;
}

double LP100AData::alarmSWR() const {
    // LP-100A alarm set points: 0=Off, 1=1.5, 2=2.0, 3=2.5, 4=3.0, 5=User
    switch (alarmSetPoint) {
        case 0: return 0.0;
        case 1: return 1.5;
        case 2: return 2.0;
        case 3: return 2.5;
        case 4: return 3.0;
        case 5: return -1.0;  // User-defined (value not reported in poll)
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
}

void LP100AProtocol::startPolling(int intervalMs) {
    m_pollTimer.start(intervalMs);
    poll(); // Immediate first poll
}

void LP100AProtocol::stopPolling() {
    m_pollTimer.stop();
}

bool LP100AProtocol::isPolling() const {
    return m_pollTimer.isActive();
}

int LP100AProtocol::pollInterval() const {
    return m_pollTimer.interval();
}

void LP100AProtocol::setPollInterval(int ms) {
    bool wasActive = m_pollTimer.isActive();
    if (wasActive) m_pollTimer.stop();
    if (wasActive) m_pollTimer.start(ms);
}

void LP100AProtocol::poll() {
    m_transport->write(QByteArray("P"));
}

void LP100AProtocol::incrementAlarm() {
    m_transport->write(QByteArray("A"));
    QTimer::singleShot(CommandDelayMs, this, &LP100AProtocol::poll);
}

void LP100AProtocol::incrementMode() {
    m_transport->write(QByteArray("M"));
    QTimer::singleShot(CommandDelayMs, this, &LP100AProtocol::poll);
}

void LP100AProtocol::togglePeakAvgTune() {
    m_transport->write(QByteArray("F"));
    QTimer::singleShot(CommandDelayMs, this, &LP100AProtocol::poll);
}

void LP100AProtocol::onDataReceived(const QByteArray &data) {
    m_buffer.append(data);

    // Process complete lines (responses start with ';')
    while (true) {
        int semiIdx = m_buffer.indexOf(';');
        if (semiIdx < 0) {
            // No start marker yet — discard junk before it
            m_buffer.clear();
            break;
        }

        // Discard anything before the semicolon
        if (semiIdx > 0) {
            m_buffer.remove(0, semiIdx);
        }

        // Look for end of response: CR, LF, or next semicolon
        int endIdx = -1;
        for (int i = 1; i < m_buffer.size(); ++i) {
            char c = m_buffer.at(i);
            if (c == '\r' || c == '\n' || c == ';') {
                endIdx = i;
                break;
            }
        }

        if (endIdx < 0) {
            // Incomplete response, wait for more data
            break;
        }

        QByteArray line = m_buffer.mid(0, endIdx);
        m_buffer.remove(0, endIdx);

        // Skip any trailing CR/LF
        while (!m_buffer.isEmpty() && (m_buffer.at(0) == '\r' || m_buffer.at(0) == '\n')) {
            m_buffer.remove(0, 1);
        }

        emit rawResponse(QString::fromLatin1(line));
        parseResponse(line);
    }
}

void LP100AProtocol::onPollTimer() {
    if (m_transport->isOpen()) {
        poll();
    }
}

bool LP100AProtocol::parseResponse(const QByteArray &line) {
    // Format: ;1457.00,49.3,005.0,2,N8LP  ,0,2,61.6,1.02
    // Fields: Power, Z, Phase, AlarmSetPoint, Callsign(6), PowerRange, PeakHoldMode, dBm, SWR

    QString str = QString::fromLatin1(line);
    if (!str.startsWith(';')) {
        emit parseError("Response doesn't start with ';': " + str);
        return false;
    }

    str = str.mid(1); // Remove leading ';'
    QStringList fields = str.split(',');

    if (fields.size() < 9) {
        emit parseError(QString("Expected 9 fields, got %1: %2").arg(fields.size()).arg(str));
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
    if (!ok) { emit parseError("Bad alarm: " + fields[3]); return false; }

    data.callsign = fields[4].trimmed();

    data.powerRange = fields[5].trimmed().toInt(&ok);
    if (!ok) { emit parseError("Bad range: " + fields[5]); return false; }

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
