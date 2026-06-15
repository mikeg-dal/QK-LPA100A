#include "core/TcpTransport.h"
#include "Style.h"
#include <QDebug>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

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

    m_connectTimer.setSingleShot(true);
    connect(&m_connectTimer, &QTimer::timeout,
            this, &TcpTransport::onConnectTimeout);
}

void TcpTransport::setHost(const QString &host) {
    m_host = host;
}

void TcpTransport::setPort(quint16 port) {
    m_port = port;
}

bool TcpTransport::open() {
    // abort() (not disconnectFromHost) so a lingering half-open socket is torn
    // down immediately with an RST rather than blocking in ClosingState. This
    // also frees the single-client Lantronix bridge's stale session promptly.
    if (m_socket.state() != QAbstractSocket::UnconnectedState) {
        m_socket.abort();
    }

    m_socket.connectToHost(m_host, m_port);
    m_connectTimer.start(Style::Protocol::TcpConnectTimeoutMs);
    // Non-blocking — connectionChanged(true) fires on success, errorOccurred on
    // failure or connect timeout. The bool return is always true for TCP.
    return true;
}

void TcpTransport::close() {
    m_connectTimer.stop();
    // Immediate teardown — disconnectFromHost() can hang in ClosingState waiting
    // on a FIN ack that a dead peer will never send.
    m_socket.abort();
}

bool TcpTransport::isOpen() const {
    return m_socket.state() == QAbstractSocket::ConnectedState;
}

qint64 TcpTransport::write(const QByteArray &data) {
    if (!isOpen()) return -1;
    const qint64 n = m_socket.write(data);
    if (n < 0)
        emit errorOccurred(m_socket.errorString());
    return n;
}

void TcpTransport::onReadyRead() {
    QByteArray data = m_socket.readAll();
    if (!data.isEmpty()) {
        emit dataReceived(data);
    }
}

void TcpTransport::onConnected() {
    m_connectTimer.stop();
    enableKeepAlive();
    emit connectionChanged(true);
}

void TcpTransport::onDisconnected() {
    m_connectTimer.stop();
    emit connectionChanged(false);
}

void TcpTransport::onError(QAbstractSocket::SocketError error) {
    Q_UNUSED(error);
    m_connectTimer.stop();
    emit errorOccurred(m_socket.errorString());
}

void TcpTransport::onConnectTimeout() {
    if (m_socket.state() == QAbstractSocket::ConnectedState)
        return;  // Raced with a successful connect — nothing to do.
    // The dial stalled (bridge unreachable, or still holding a stale single-client
    // session). Abort and surface the failure so the supervisor backs off and retries.
    m_socket.abort();
    emit errorOccurred("Connection timed out");
}

void TcpTransport::enableKeepAlive() {
    // Qt's cross-platform flag turns SO_KEEPALIVE on but leaves the (2-hour)
    // system defaults in place. Tune the macOS-native timers so a vanished peer
    // — cable pull, bridge power-off, Wi-Fi drop with no FIN — is detected in
    // roughly TcpKeepAliveIdleSec + TcpKeepAliveIntervalSec * TcpKeepAliveCount
    // seconds and the socket emits disconnected, instead of looking alive forever.
    m_socket.setSocketOption(QAbstractSocket::KeepAliveOption, 1);

    const int fd = static_cast<int>(m_socket.socketDescriptor());
    if (fd < 0) return;

    int on = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on));

    int idle  = Style::Protocol::TcpKeepAliveIdleSec;
    int intvl = Style::Protocol::TcpKeepAliveIntervalSec;
    int cnt   = Style::Protocol::TcpKeepAliveCount;
#ifdef TCP_KEEPALIVE   // macOS/BSD: seconds of idle before the first probe
    ::setsockopt(fd, IPPROTO_TCP, TCP_KEEPALIVE, &idle, sizeof(idle));
#endif
#ifdef TCP_KEEPINTVL
    ::setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &intvl, sizeof(intvl));
#endif
#ifdef TCP_KEEPCNT
    ::setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &cnt, sizeof(cnt));
#endif
}
