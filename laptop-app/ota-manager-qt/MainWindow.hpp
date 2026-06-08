#pragma once
#include <QMainWindow>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QTextEdit>
#include <QLineEdit>
#include <QGroupBox>
#include <QComboBox>
#include "QnxStatusClient.hpp"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void onConnectClicked();
    void onSelectImageClicked();
    void onSendUpdateClicked();
    void onStateChanged(QString state);
    void onLaptopProgress(quint64 received, quint64 total, int percent);
    void onYoctoProgress(int percent, QString msg);
    void onVersionInfo(QString activeSlot, QString vA, QString vB);
    void onLogReceived(QString msg);
    void onConnectionStatus(bool connected);

private:
    void setupUi();
    void appendLog(const QString &msg);
    QString stateToDisplay(const QString &state);
    QString stateToColor(const QString &state);

    // Connection panel
    QLineEdit   *gatewayIpEdit_;
    QPushButton *connectBtn_;
    QLabel      *connectionStatusLabel_;

    // Vehicle status panel
    QLabel      *stateLabel_;
    QLabel      *activeSlotLabel_;
    QLabel      *versionALabel_;
    QLabel      *versionBLabel_;

    // Update panel
    QLineEdit   *imagePathEdit_;
    QPushButton *selectImageBtn_;
    QPushButton *sendUpdateBtn_;

    // Progress panel
    QProgressBar *laptopProgressBar_;
    QLabel       *laptopProgressLabel_;
    QProgressBar *yoctoProgressBar_;
    QLabel       *yoctoProgressLabel_;

    // Log panel
    QTextEdit   *logView_;

    // Backend
    QnxStatusClient *statusClient_;
    QString          selectedImage_;
    QString          currentGatewayHost_;
};
