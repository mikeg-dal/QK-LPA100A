#include "core/HamlibClient.h"
#include <QProcess>
#include <QRegularExpression>

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

QList<HamlibClient::RigInfo> HamlibClient::availableRigs() {
    QList<RigInfo> rigs;

    QProcess proc;
    proc.start("rigctld", {"--list"});
    if (!proc.waitForFinished(5000))
        return rigs;

    // Output format (fixed-width columns):
    // Rig #  Mfg                    Model                   Version         Status      Macro
    //  2047  Elecraft               K4                      20250515.32     Stable      RIG_MODEL_K4
    // Columns: 0-5=id, 6-29=mfg, 30-53=model, 54-69=version, 70-81=status

    const QString output = QString::fromUtf8(proc.readAllStandardOutput());
    const QStringList lines = output.split('\n');

    for (const QString &line : lines) {
        // Skip header, init messages, empty lines
        if (line.length() < 70) continue;
        if (line.trimmed().startsWith("Rig #")) continue;
        if (line.trimmed().startsWith("init")) continue;

        bool ok;
        int id = line.left(6).trimmed().toInt(&ok);
        if (!ok || id < 1) continue;

        QString mfg   = line.mid(6, 23).trimmed();
        QString model  = line.mid(30, 24).trimmed();
        QString status = line.mid(70, 12).trimmed();

        if (mfg.isEmpty() && model.isEmpty()) continue;

        rigs.append({id, mfg, model, status});
    }

    return rigs;
}
