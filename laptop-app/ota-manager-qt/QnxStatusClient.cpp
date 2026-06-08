#include "QnxStatusClient.hpp"
#include <QJsonDocument>
#include <QJsonObject>

QnxStatusClient::QnxStatusClient(QObject *parent)
    : QObject(parent)
    , socket_(new QTcpSocket(this))
{
    connect(socket_, &QTcpSocket::connected,
            this, &QnxStatusClient::onConnected);
    connect(socket_, &QTcpSocket::disconnected,
            this, &QnxStatusClient::onDisconnected);
    connect(socket_, &QTcpSocket::readyRead,
            this, &QnxStatusClient::onReadyRead);
}

void QnxStatusClient::connectToGateway(const QString &host) {
    socket_->connectToHost(host, 55001);
}

void QnxStatusClient::disconnect() {
    socket_->disconnectFromHost();
}

void QnxStatusClient::onConnected() {
    emit connectionStatusChanged(true);
}

void QnxStatusClient::onDisconnected() {
    emit connectionStatusChanged(false);
}

void QnxStatusClient::onReadyRead() {
    buffer_ += socket_->readAll();
    // Messages are newline-delimited JSON
    while (true) {
        int idx = buffer_.indexOf('\n');
        if (idx < 0) break;
        QByteArray line = buffer_.left(idx).trimmed();
        buffer_.remove(0, idx + 1);
        if (line.isEmpty()) continue;
        QJsonDocument doc = QJsonDocument::fromJson(line);
        if (!doc.isNull() && doc.isObject())
            parseMessage(doc.object());
    }
}

void QnxStatusClient::parseMessage(const QJsonObject &obj) {
    QString type = obj["type"].toString();

    if (type == "state") {
        emit stateChanged(obj["state"].toString());
        // Also emit version if present in state message
        if (obj.contains("activeSlot"))
            emit versionInfoReceived(
                obj["activeSlot"].toString(),
                obj["versionA"].toString(),
                obj["versionB"].toString());
    }
    else if (type == "laptop_progress") {
        emit laptopProgressChanged(
            (quint64)obj["received"].toDouble(),
            (quint64)obj["total"].toDouble(),
            obj["percent"].toInt());
    }
    else if (type == "yocto_progress") {
        emit yoctoProgressChanged(
            obj["percent"].toInt(),
            obj["msg"].toString());
    }
    else if (type == "version") {
        emit versionInfoReceived(
            obj["activeSlot"].toString(),
            obj["versionA"].toString(),
            obj["versionB"].toString());
    }
    else if (type == "log") {
        emit logReceived(obj["msg"].toString());
    }
}
