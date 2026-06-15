#pragma once

#include "ITransport.h"
#include "Style.h"
#include <QTcpSocket>
#include <QTimer>

class TcpTransport : public ITransport {
    Q_OBJECT
public:
    explicit TcpTransport(QObject *parent = nullptr);

    void setHost(const QString &host);
    void setPort(quint16 port);
    QString host() const { return m_host; }
    quint16 port() const { return m_port; }

    bool open() override;
    void close() override;
    bool isOpen() const override;
    qint64 write(const QByteArray &data) override;

private slots:
    void onReadyRead();
    void onConnected();
    void onDisconnected();
    void onError(QAbstractSocket::SocketError error);
    void onConnectTimeout();

private:
    void enableKeepAlive();   // OS-level dead-peer detection so a half-open socket is dropped in seconds

    QTcpSocket m_socket;
    QTimer m_connectTimer;    // Bounds a connect attempt — open() is non-blocking and would otherwise never fail
    QString m_host;
    quint16 m_port = Style::Protocol::DefaultTcpPort;
};
