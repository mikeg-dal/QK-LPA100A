#pragma once

#include "ITransport.h"
#include <QSerialPort>

class SerialTransport : public ITransport {
    Q_OBJECT
public:
    explicit SerialTransport(QObject *parent = nullptr);

    void setPortName(const QString &portName);
    QString portName() const;
    void setBaudRate(qint32 baud);

    bool open() override;
    void close() override;
    bool isOpen() const override;
    qint64 write(const QByteArray &data) override;

    static QStringList availablePorts();

private slots:
    void onReadyRead();
    void onError(QSerialPort::SerialPortError error);

private:
    QSerialPort m_serial;
};
