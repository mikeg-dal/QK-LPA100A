#include "core/TcpTransport.h"
#include <QDebug>

TcpTransport::TcpTransport(QObject *parent)
    : ITransport(parent)
{
    connect(&m_socket, &QTcpSocket::readyRead,
            this, &TcpTransport::onReadyRead);
    connect(&m_socket, &QTcpSocket::connected,
            this, &TcpTransport::onConnected);
    connect(&m_socket, &QTcpSocket::disconnected,
            this, &TcpTransport::onDisconnected);
    connect(&m_socket, &QAbstractSocket::errorOccurred,
            this, &TcpTransport::onError);
}

void TcpTransport::setHost(const QString &host) {
    m_host = host;
}

void TcpTransport::setPort(quint16 port) {
    m_port = port;
}

bool TcpTransport::open() {
    if (m_socket.state() != QAbstractSocket::UnconnectedState) {
        m_socket.abort();
    }

    m_socket.connectToHost(m_host, m_port);
    // Non-blocking — connectionChanged signal fires on success
    return true;
}

void TcpTransport::close() {
    m_socket.disconnectFromHost();
}

bool TcpTransport::isOpen() const {
    return m_socket.state() == QAbstractSocket::ConnectedState;
}

qint64 TcpTransport::write(const QByteArray &data) {
    if (!isOpen()) return -1;
    return m_socket.write(data);
}

void TcpTransport::onReadyRead() {
    QByteArray data = m_socket.readAll();
    if (!data.isEmpty()) {
        emit dataReceived(data);
    }
}

void TcpTransport::onConnected() {
    emit connectionChanged(true);
}

void TcpTransport::onDisconnected() {
    emit connectionChanged(false);
}

void TcpTransport::onError(QAbstractSocket::SocketError error) {
    Q_UNUSED(error);
    emit errorOccurred(m_socket.errorString());
}
