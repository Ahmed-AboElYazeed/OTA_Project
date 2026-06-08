#pragma once
#include <QObject>
#include <QTcpSocket>
#include <QString>

// Connects to QNX port 55001 and emits parsed status signals
class QnxStatusClient : public QObject {
    Q_OBJECT
public:
    explicit QnxStatusClient(QObject *parent = nullptr);
    void connectToGateway(const QString &host);
    void disconnect();

signals:
    void stateChanged(QString state);
    void laptopProgressChanged(quint64 received, quint64 total, int percent);
    void yoctoProgressChanged(int percent, QString msg);
    void versionInfoReceived(QString activeSlot,
                             QString versionA, QString versionB);
    void logReceived(QString msg);
    void connectionStatusChanged(bool connected);

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();

private:
    QTcpSocket *socket_;
    QByteArray  buffer_;
    void parseMessage(const QJsonObject &obj);
};
