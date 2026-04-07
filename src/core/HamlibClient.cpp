#include "core/HamlibClient.h"

HamlibClient::HamlibClient(QObject *parent)
    : QObject(parent)
{
    connect(&m_socket, &QTcpSocket::readyRead,
            this, &HamlibClient::onReadyRead);
    connect(&m_socket, &QTcpSocket::connected,
            this, &HamlibClient::connected);
    connect(&m_socket, &QTcpSocket::disconnected,
            this, &HamlibClient::disconnected);
    connect(&m_socket, &QAbstractSocket::errorOccurred,
            this, &HamlibClient::onSocketError);
}

void HamlibClient::connectToRigctld(const QString &host, quint16 port) {
    if (m_socket.state() != QAbstractSocket::UnconnectedState)
        m_socket.abort();
    m_buffer.clear();
    m_pending.clear();
    m_responseLines.clear();
    m_socket.connectToHost(host, port);
}

void HamlibClient::disconnect() {
    m_socket.disconnectFromHost();
}

bool HamlibClient::isConnected() const {
    return m_socket.state() == QAbstractSocket::ConnectedState;
}

void HamlibClient::sendCommand(const QByteArray &cmd, PendingCmd type) {
    if (!isConnected()) {
        emit errorOccurred("Not connected to rigctld");
        return;
    }
    m_pending.enqueue(type);
    m_socket.write(cmd + "\n");
}

void HamlibClient::setFrequency(qint64 freqHz) {
    sendCommand("F " + QByteArray::number(freqHz), PendingCmd::SetFreq);
}

void HamlibClient::getFrequency() {
    sendCommand("f", PendingCmd::GetFreq);
}

void HamlibClient::setPTT(bool on) {
    sendCommand("T " + QByteArray::number(on ? 1 : 0), PendingCmd::SetPTT);
}

void HamlibClient::getPTT() {
    sendCommand("t", PendingCmd::GetPTT);
}

void HamlibClient::setMode(const QString &mode, int passband) {
    sendCommand("M " + mode.toUtf8() + " " + QByteArray::number(passband),
                PendingCmd::SetMode);
}

void HamlibClient::getMode() {
    sendCommand("m", PendingCmd::GetMode);
}

void HamlibClient::setRFPower(double level) {
    sendCommand("L RFPOWER " + QByteArray::number(qBound(0.0, level, 1.0), 'f', 2),
                PendingCmd::Other);
}

void HamlibClient::onReadyRead() {
    m_buffer.append(m_socket.readAll());

    while (m_buffer.contains('\n')) {
        int idx = m_buffer.indexOf('\n');
        QByteArray line = m_buffer.left(idx).trimmed();
        m_buffer.remove(0, idx + 1);

        if (!line.isEmpty())
            processResponse(line);
    }
}

void HamlibClient::processResponse(const QByteArray &line) {
    // Every rigctld response sequence ends with "RPRT N"
    if (line.startsWith("RPRT")) {
        int code = line.mid(5).toInt();
        bool ok = (code == 0);

        if (!m_pending.isEmpty()) {
            PendingCmd cmd = m_pending.dequeue();

            if (ok) {
                switch (cmd) {
                case PendingCmd::GetFreq:
                    if (!m_responseLines.isEmpty())
                        emit frequencyChanged(m_responseLines.first().toLongLong());
                    break;

                case PendingCmd::GetPTT:
                    if (!m_responseLines.isEmpty())
                        emit pttChanged(m_responseLines.first().toInt() != 0);
                    break;

                case PendingCmd::GetMode:
                    if (m_responseLines.size() >= 2)
                        emit modeChanged(m_responseLines[0],
                                         m_responseLines[1].toInt());
                    break;

                default:
                    break;
                }
            }

            emit commandCompleted(ok, code);
        }

        m_responseLines.clear();
    } else {
        m_responseLines.append(QString::fromUtf8(line));
    }
}

void HamlibClient::onSocketError(QAbstractSocket::SocketError) {
    emit errorOccurred(m_socket.errorString());
}
