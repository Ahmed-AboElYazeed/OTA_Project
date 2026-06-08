#include "MainWindow.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QProcess>
#include <QDateTime>
#include <QMessageBox>
#include <QFont>
#include <QDir>
#include <QScrollBar>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , statusClient_(new QnxStatusClient(this))
{
    setupUi();

    connect(statusClient_, &QnxStatusClient::stateChanged,
            this, &MainWindow::onStateChanged);
    connect(statusClient_, &QnxStatusClient::laptopProgressChanged,
            this, &MainWindow::onLaptopProgress);
    connect(statusClient_, &QnxStatusClient::yoctoProgressChanged,
            this, &MainWindow::onYoctoProgress);
    connect(statusClient_, &QnxStatusClient::versionInfoReceived,
            this, &MainWindow::onVersionInfo);
    connect(statusClient_, &QnxStatusClient::logReceived,
            this, &MainWindow::onLogReceived);
    connect(statusClient_, &QnxStatusClient::connectionStatusChanged,
            this, &MainWindow::onConnectionStatus);
}

void MainWindow::setupUi() {
    setWindowTitle("OTA Update Manager \u2014 Vehicle Gateway");
    setMinimumSize(900, 700);

    QWidget     *central = new QWidget(this);
    QVBoxLayout *root    = new QVBoxLayout(central);
    root->setSpacing(12);
    root->setContentsMargins(16, 16, 16, 16);
    setCentralWidget(central);

    // ── Gateway Connection ────────────────────────────────────────────────
    QGroupBox   *connBox    = new QGroupBox("Vehicle Gateway Connection");
    QHBoxLayout *connLayout = new QHBoxLayout(connBox);

    connLayout->addWidget(new QLabel("Gateway IP / Host:"));
    gatewayIpEdit_ = new QLineEdit("qnxpi.local");
    gatewayIpEdit_->setFixedWidth(200);
    connLayout->addWidget(gatewayIpEdit_);

    connectBtn_ = new QPushButton("Connect");
    connectBtn_->setFixedWidth(100);
    connLayout->addWidget(connectBtn_);

    connectionStatusLabel_ = new QLabel("\u25cf Disconnected");
    connectionStatusLabel_->setStyleSheet("color: #e74c3c; font-weight: bold;");
    connLayout->addWidget(connectionStatusLabel_);
    connLayout->addStretch();
    root->addWidget(connBox);

    // ── Vehicle Status ────────────────────────────────────────────────────
    QGroupBox  *statusBox    = new QGroupBox("Vehicle OTA Status");
    QGridLayout *statusGrid  = new QGridLayout(statusBox);

    statusGrid->addWidget(new QLabel("Update State:"), 0, 0);
    stateLabel_ = new QLabel("\u2014");
    stateLabel_->setStyleSheet("font-weight: bold; font-size: 14px;");
    statusGrid->addWidget(stateLabel_, 0, 1);

    statusGrid->addWidget(new QLabel("Active Slot:"), 1, 0);
    activeSlotLabel_ = new QLabel("\u2014");
    activeSlotLabel_->setStyleSheet("font-weight: bold;");
    statusGrid->addWidget(activeSlotLabel_, 1, 1);

    statusGrid->addWidget(new QLabel("Slot A Version:"), 2, 0);
    versionALabel_ = new QLabel("\u2014");
    statusGrid->addWidget(versionALabel_, 2, 1);

    statusGrid->addWidget(new QLabel("Slot B Version:"), 3, 0);
    versionBLabel_ = new QLabel("\u2014");
    statusGrid->addWidget(versionBLabel_, 3, 1);

    statusGrid->setColumnStretch(1, 1);
    root->addWidget(statusBox);

    // ── Update Deployment ─────────────────────────────────────────────────
    QGroupBox   *updateBox    = new QGroupBox("Deploy Update");
    QVBoxLayout *updateLayout = new QVBoxLayout(updateBox);

    QHBoxLayout *fileRow = new QHBoxLayout();
    fileRow->addWidget(new QLabel("Rootfs Image:"));
    imagePathEdit_ = new QLineEdit();
    imagePathEdit_->setPlaceholderText("Select a .ext4 rootfs image...");
    fileRow->addWidget(imagePathEdit_);
    selectImageBtn_ = new QPushButton("Browse...");
    selectImageBtn_->setFixedWidth(90);
    fileRow->addWidget(selectImageBtn_);
    updateLayout->addLayout(fileRow);

    QHBoxLayout *btnRow = new QHBoxLayout();
    sendUpdateBtn_ = new QPushButton("\u25b6  Send Update to Vehicle");
    sendUpdateBtn_->setFixedHeight(38);
    sendUpdateBtn_->setStyleSheet(
        "QPushButton { background:#2ecc71; color:white; font-weight:bold;"
        "border-radius:4px; font-size:13px; }"
        "QPushButton:disabled { background:#95a5a6; }"
        "QPushButton:hover { background:#27ae60; }");
    sendUpdateBtn_->setEnabled(false);
    btnRow->addStretch();
    btnRow->addWidget(sendUpdateBtn_);
    updateLayout->addLayout(btnRow);
    root->addWidget(updateBox);

    // ── Progress ──────────────────────────────────────────────────────────
    QGroupBox   *progBox    = new QGroupBox("Transfer Progress");
    QGridLayout *progGrid   = new QGridLayout(progBox);

    progGrid->addWidget(new QLabel("Laptop \u2192 QNX Gateway:"), 0, 0);
    laptopProgressBar_ = new QProgressBar();
    laptopProgressBar_->setRange(0, 100);
    laptopProgressBar_->setValue(0);
    laptopProgressBar_->setStyleSheet(
        "QProgressBar::chunk { background:#3498db; }");
    progGrid->addWidget(laptopProgressBar_, 0, 1);
    laptopProgressLabel_ = new QLabel("0%");
    laptopProgressLabel_->setFixedWidth(45);
    progGrid->addWidget(laptopProgressLabel_, 0, 2);

    progGrid->addWidget(new QLabel("QNX Gateway \u2192 ECU:"), 1, 0);
    yoctoProgressBar_ = new QProgressBar();
    yoctoProgressBar_->setRange(0, 100);
    yoctoProgressBar_->setValue(0);
    yoctoProgressBar_->setStyleSheet(
        "QProgressBar::chunk { background:#e67e22; }");
    progGrid->addWidget(yoctoProgressBar_, 1, 1);
    yoctoProgressLabel_ = new QLabel("0%");
    yoctoProgressLabel_->setFixedWidth(45);
    progGrid->addWidget(yoctoProgressLabel_, 1, 2);

    progGrid->setColumnStretch(1, 1);
    root->addWidget(progBox);

    // ── Gateway Logs ──────────────────────────────────────────────────────
    QGroupBox   *logBox    = new QGroupBox("Gateway Logs");
    QVBoxLayout *logLayout = new QVBoxLayout(logBox);
    logView_ = new QTextEdit();
    logView_->setReadOnly(true);
    logView_->setFont(QFont("Monospace", 9));
    logView_->setStyleSheet("background:#1e1e1e; color:#d4d4d4;");
    logView_->setMinimumHeight(160);
    logLayout->addWidget(logView_);
    root->addWidget(logBox);

    // ── Connections ───────────────────────────────────────────────────────
    connect(connectBtn_,     &QPushButton::clicked,
            this, &MainWindow::onConnectClicked);
    connect(selectImageBtn_, &QPushButton::clicked,
            this, &MainWindow::onSelectImageClicked);
    connect(sendUpdateBtn_,  &QPushButton::clicked,
            this, &MainWindow::onSendUpdateClicked);
}

void MainWindow::onConnectClicked() {
    currentGatewayHost_ = gatewayIpEdit_->text().trimmed();
    if (currentGatewayHost_.isEmpty()) return;
    appendLog("[APP] Connecting to " + currentGatewayHost_ + ":55001...");
    statusClient_->connectToGateway(currentGatewayHost_);
}

void MainWindow::onSelectImageClicked() {
    QString path = QFileDialog::getOpenFileName(
        this, "Select Rootfs Image", QDir::homePath(),
        "Rootfs Images (*.ext4 *.img);;All Files (*)");
    if (!path.isEmpty()) {
        selectedImage_ = path;
        imagePathEdit_->setText(path);
        sendUpdateBtn_->setEnabled(true);
        appendLog("[APP] Image selected: " + path);
    }
}

void MainWindow::onSendUpdateClicked() {
    if (selectedImage_.isEmpty()) return;

    sendUpdateBtn_->setEnabled(false);
    laptopProgressBar_->setValue(0);
    yoctoProgressBar_->setValue(0);
    laptopProgressLabel_->setText("0%");
    yoctoProgressLabel_->setText("0%");

    appendLog("[APP] Launching send_update.py...");

    // Run the Python sender as a background process
    QProcess *proc = new QProcess(this);
    QString scriptPath = QDir::homePath() +
        "/ITI_Files/linux/Embedded-Linux/cppProject/OTA_Project/laptop-app/python/send_update.py";

    connect(proc, &QProcess::readyReadStandardOutput, this, [this, proc]() {
        QString out = proc->readAllStandardOutput();
        for (const QString &line : out.split('\n'))
            if (!line.trimmed().isEmpty())
                appendLog("[SEND] " + line.trimmed());
    });
    connect(proc, &QProcess::readyReadStandardError, this, [this, proc]() {
        QString err = proc->readAllStandardError();
        for (const QString &line : err.split('\n'))
            if (!line.trimmed().isEmpty())
                appendLog("[SEND ERR] " + line.trimmed());
    });
    connect(proc,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int code, QProcess::ExitStatus) {
                appendLog(code == 0
                    ? "[APP] send_update.py completed successfully"
                    : "[APP] send_update.py exited with error");
                sendUpdateBtn_->setEnabled(true);
            });

    proc->start("python3", {scriptPath, selectedImage_, currentGatewayHost_});
}

void MainWindow::onStateChanged(QString state) {
    stateLabel_->setText(stateToDisplay(state));
    stateLabel_->setStyleSheet(
        "font-weight:bold; font-size:14px; color:" + stateToColor(state));
    appendLog("[GW] State \u2192 " + state);
}

void MainWindow::onLaptopProgress(quint64 received, quint64 total, int percent) {
    laptopProgressBar_->setValue(percent);
    laptopProgressLabel_->setText(QString::number(percent) + "%");
}

void MainWindow::onYoctoProgress(int percent, QString msg) {
    yoctoProgressBar_->setValue(percent);
    yoctoProgressLabel_->setText(QString::number(percent) + "%");
}

void MainWindow::onVersionInfo(QString activeSlot,
                               QString vA, QString vB) {
    activeSlotLabel_->setText(activeSlot.toUpper());
    activeSlotLabel_->setStyleSheet(
        "font-weight:bold; color:" +
        QString(activeSlot == "a" ? "#2ecc71" : "#3498db"));
    versionALabel_->setText(vA + (activeSlot=="a" ? "  \u25c0 active" : ""));
    versionBLabel_->setText(vB + (activeSlot=="b" ? "  \u25c0 active" : ""));
}

void MainWindow::onLogReceived(QString msg) {
    appendLog(msg);
}

void MainWindow::onConnectionStatus(bool connected) {
    if (connected) {
        connectionStatusLabel_->setText("\u25cf Connected");
        connectionStatusLabel_->setStyleSheet(
            "color:#2ecc71; font-weight:bold;");
        appendLog("[APP] Connected to gateway");
    } else {
        connectionStatusLabel_->setText("\u25cf Disconnected");
        connectionStatusLabel_->setStyleSheet(
            "color:#e74c3c; font-weight:bold;");
        sendUpdateBtn_->setEnabled(false);
        appendLog("[APP] Disconnected from gateway");
    }
}

void MainWindow::appendLog(const QString &msg) {
    QString ts = QDateTime::currentDateTime().toString("hh:mm:ss");
    logView_->append("[" + ts + "] " + msg);
    logView_->verticalScrollBar()->setValue(
        logView_->verticalScrollBar()->maximum());
}

QString MainWindow::stateToDisplay(const QString &s) {
    if (s == "idle")                  return "Idle \u2014 Ready";
    if (s == "receiving_from_laptop") return "Receiving from Laptop...";
    if (s == "verifying")             return "Verifying Image...";
    if (s == "forwarding_to_yocto")   return "Deploying to ECU...";
    if (s == "complete")              return "Update Complete \u2713";
    if (s == "failed")                return "Update Failed \u2717";
    return s;
}

QString MainWindow::stateToColor(const QString &s) {
    if (s == "idle")                  return "#95a5a6";
    if (s == "complete")              return "#2ecc71";
    if (s == "failed")                return "#e74c3c";
    return "#f39c12";
}
