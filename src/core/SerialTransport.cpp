#include "core/SerialTransport.h"
#include "Style.h"
#include <QSerialPortInfo>
#include <QDebug>

SerialTransport::SerialTransport(QObject *parent)
    : ITransport(parent)
{
    m_serial.setBaudRate(Style::Protocol::DefaultBaud);
    m_serial.setDataBits(QSerialPort::Data8);
    m_serial.setParity(QSerialPort::NoParity);
    m_serial.setStopBits(QSerialPort::OneStop);
    m_serial.setFlowControl(QSerialPort::NoFlowControl);

    connect(&m_serial, &QSerialPort::readyRead,
            this, &SerialTransport::onReadyRead);
    connect(&m_serial, &QSerialPort::errorOccurred,
            this, &SerialTransport::onError);
}

void SerialTransport::setPortName(const QString &portName) {
    m_serial.setPortName(portName);
}

QString SerialTransport::portName() const {
    return m_serial.portName();
}

void SerialTransport::setBaudRate(qint32 baud) {
    m_serial.setBaudRate(baud);
}

bool SerialTransport::open() {
    if (m_serial.isOpen()) {
        m_serial.close();
    }

    if (!m_serial.open(QIODevice::ReadWrite)) {
        emit errorOccurred(m_serial.errorString());
        return false;
    }

    m_serial.clear();
    emit connectionChanged(true);
    return true;
}

void SerialTransport::close() {
    if (m_serial.isOpen()) {
        m_serial.close();
        emit connectionChanged(false);
    }
}

bool SerialTransport::isOpen() const {
    return m_serial.isOpen();
}

qint64 SerialTransport::write(const QByteArray &data) {
    if (!m_serial.isOpen()) return -1;
    return m_serial.write(data);
}

QStringList SerialTransport::availablePorts() {
    QStringList ports;
    for (const auto &info : QSerialPortInfo::availablePorts()) {
        ports << info.portName();
    }
    return ports;
}

void SerialTransport::onReadyRead() {
    QByteArray data = m_serial.readAll();
    if (!data.isEmpty()) {
        emit dataReceived(data);
    }
}

void SerialTransport::onError(QSerialPort::SerialPortError error) {
    if (error == QSerialPort::NoError) return;
    if (error == QSerialPort::ResourceError) {
        // Device disconnected
        emit connectionChanged(false);
    }
    emit errorOccurred(m_serial.errorString());
}
