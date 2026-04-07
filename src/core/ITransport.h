#pragma once

#include <QObject>
#include <QByteArray>
#include <QString>

class ITransport : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    virtual ~ITransport() = default;

    virtual bool open() = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;
    virtual qint64 write(const QByteArray &data) = 0;

signals:
    void dataReceived(const QByteArray &data);
    void connectionChanged(bool connected);
    void errorOccurred(const QString &error);
};
