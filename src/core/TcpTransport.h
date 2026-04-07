#pragma once

#include "ITransport.h"
#include <QTcpSocket>

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

private:
    QTcpSocket m_socket;
    QString m_host;
    quint16 m_port = 10001; // Lantronix default
};
