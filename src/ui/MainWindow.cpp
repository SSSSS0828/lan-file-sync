#include "MainWindow.h"

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QGroupBox>
#include <QDir>
#include <QSettings>

static const quint16 DEFAULT_PORT = 9527;

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , mgr_(new TransferManager(this))
{
    setWindowTitle("LanSwift Transfer");
    setMinimumSize(620, 520);

    tabs_ = new QTabWidget(this);
    setCentralWidget(tabs_);

    auto* sendTab = new QWidget;
    auto* recvTab = new QWidget;
    buildSendTab(sendTab);
    buildRecvTab(recvTab);
    tabs_->addTab(sendTab, "📤  Send");
    tabs_->addTab(recvTab, "📥  Receive");

    // Wire signals – Send
    auto* s = mgr_->sender();
    connect(s, &SendWorker::progressUpdated,
            this, &MainWindow::onSendProgress,
            Qt::QueuedConnection);
    connect(s, &SendWorker::speedUpdated,
            this, &MainWindow::onSendSpeed,
            Qt::QueuedConnection);
    connect(s, &SendWorker::transferFinished,
            this, &MainWindow::onSendFinished,
            Qt::QueuedConnection);
    connect(s, &SendWorker::statusMessage,
            this, &MainWindow::onSendStatus,
            Qt::QueuedConnection);

    // Wire signals – Recv
    auto* r = mgr_->receiver();
    connect(r, &RecvWorker::progressUpdated,
            this, &MainWindow::onRecvProgress,
            Qt::QueuedConnection);
    connect(r, &RecvWorker::speedUpdated,
            this, &MainWindow::onRecvSpeed,
            Qt::QueuedConnection);
    connect(r, &RecvWorker::transferFinished,
            this, &MainWindow::onRecvFinished,
            Qt::QueuedConnection);
    connect(r, &RecvWorker::statusMessage,
            this, &MainWindow::onRecvStatus,
            Qt::QueuedConnection);
    connect(r, &RecvWorker::newConnection,
            this, &MainWindow::onNewConnection,
            Qt::QueuedConnection);

    // Restore settings
    QSettings cfg("LanSwift", "Transfer");
    editHost_->setText(cfg.value("host", "127.0.0.1").toString());
    spinPort_->setValue(cfg.value("port", DEFAULT_PORT).toInt());
    spinServerPort_->setValue(
        cfg.value("server_port", DEFAULT_PORT).toInt());
    editSaveDir_->setText(
        cfg.value("save_dir",
                  QDir::homePath() + "/Downloads/LanSwift").toString());
}

// ─────────────────────────────────────────────────────────────
void MainWindow::buildSendTab(QWidget* tab)
{
    auto* outer = new QVBoxLayout(tab);

    // File selection
    auto* grpFile = new QGroupBox("File", tab);
    auto* hFile   = new QHBoxLayout(grpFile);
    editFilePath_ = new QLineEdit;
    editFilePath_->setPlaceholderText("Select a file to send...");
    auto* btnBrowse = new QPushButton("Browse...");
    hFile->addWidget(editFilePath_);
    hFile->addWidget(btnBrowse);
    connect(btnBrowse, &QPushButton::clicked,
            this, &MainWindow::onBrowseFile);

    // Remote host
    auto* grpHost = new QGroupBox("Destination", tab);
    auto* form    = new QFormLayout(grpHost);
    editHost_ = new QLineEdit("127.0.0.1");
    spinPort_ = new QSpinBox;
    spinPort_->setRange(1024, 65535);
    spinPort_->setValue(DEFAULT_PORT);
    form->addRow("Host:", editHost_);
    form->addRow("Port:", spinPort_);

    // Buttons
    auto* hBtn  = new QHBoxLayout;
    btnSend_    = new QPushButton("▶  Start Transfer");
    btnCancel_  = new QPushButton("■  Cancel");
    btnCancel_->setEnabled(false);
    hBtn->addStretch();
    hBtn->addWidget(btnSend_);
    hBtn->addWidget(btnCancel_);
    connect(btnSend_,   &QPushButton::clicked,
            this, &MainWindow::onSendStart);
    connect(btnCancel_, &QPushButton::clicked,
            this, &MainWindow::onSendCancel);

    // Progress
    sendProgress_ = new ProgressWidget(tab);
    sendLog_      = new FileListView(tab);

    outer->addWidget(grpFile);
    outer->addWidget(grpHost);
    outer->addLayout(hBtn);
    outer->addWidget(sendProgress_);
    outer->addWidget(sendLog_, 1);
}

// ─────────────────────────────────────────────────────────────
void MainWindow::buildRecvTab(QWidget* tab)
{
    auto* outer = new QVBoxLayout(tab);

    auto* grpSrv = new QGroupBox("Server Settings", tab);
    auto* form   = new QFormLayout(grpSrv);

    spinServerPort_ = new QSpinBox;
    spinServerPort_->setRange(1024, 65535);
    spinServerPort_->setValue(DEFAULT_PORT);

    auto* hSave    = new QHBoxLayout;
    editSaveDir_   = new QLineEdit;
    editSaveDir_->setPlaceholderText("Save directory...");
    btnBrowseSaveDir_ = new QPushButton("Browse...");
    hSave->addWidget(editSaveDir_);
    hSave->addWidget(btnBrowseSaveDir_);
    connect(btnBrowseSaveDir_, &QPushButton::clicked, this, [this] {
        QString d = QFileDialog::getExistingDirectory(
            this, "Select save directory", editSaveDir_->text());
        if (!d.isEmpty()) editSaveDir_->setText(d);
    });

    form->addRow("Port:", spinServerPort_);
    form->addRow("Save to:", hSave);

    auto* hBtn    = new QHBoxLayout;
    btnServerToggle_ = new QPushButton("▶  Start Server");
    labelServerStatus_ = new QLabel("● Stopped");
    labelServerStatus_->setStyleSheet("color: gray;");
    hBtn->addWidget(labelServerStatus_);
    hBtn->addStretch();
    hBtn->addWidget(btnServerToggle_);
    connect(btnServerToggle_, &QPushButton::clicked,
            this, &MainWindow::onServerToggle);

    recvProgress_ = new ProgressWidget(tab);
    recvLog_      = new FileListView(tab);

    outer->addWidget(grpSrv);
    outer->addLayout(hBtn);
    outer->addWidget(recvProgress_);
    outer->addWidget(recvLog_, 1);
}

// ─────────────────────────────────────────────────────────────
void MainWindow::onBrowseFile()
{
    QString f = QFileDialog::getOpenFileName(this, "Select file to send");
    if (!f.isEmpty()) editFilePath_->setText(f);
}

void MainWindow::onSendStart()
{
    QString fp   = editFilePath_->text().trimmed();
    QString host = editHost_->text().trimmed();
    if (fp.isEmpty()) {
        QMessageBox::warning(this, "No file", "Please select a file.");
        return;
    }
    if (host.isEmpty()) {
        QMessageBox::warning(this, "No host", "Please enter a destination host.");
        return;
    }
    sendProgress_->reset();
    sendLog_->clear();
    setSendBusy(true);

    // Persist settings
    QSettings cfg("LanSwift", "Transfer");
    cfg.setValue("host", host);
    cfg.setValue("port", spinPort_->value());

    mgr_->sendFile(fp, host, static_cast<quint16>(spinPort_->value()));
}

void MainWindow::onSendCancel()
{
    mgr_->cancelSend();
    setSendBusy(false);
}

void MainWindow::onServerToggle()
{
    if (serverRunning_) {
        mgr_->stopServer();
        setServerRunning(false);
    } else {
        QString dir = editSaveDir_->text().trimmed();
        if (dir.isEmpty()) {
            QMessageBox::warning(this, "No directory",
                                 "Please select a save directory.");
            return;
        }
        recvProgress_->reset();
        recvLog_->clear();

        QSettings cfg("LanSwift", "Transfer");
        cfg.setValue("server_port", spinServerPort_->value());
        cfg.setValue("save_dir", dir);

        mgr_->startServer(
            static_cast<quint16>(spinServerPort_->value()), dir);
        setServerRunning(true);
    }
}

// ─────────────────────────────────────────────────────────────
// Send slots
void MainWindow::onSendProgress(qint64 t, qint64 total)
    { sendProgress_->onProgress(t, total); }
void MainWindow::onSendSpeed(double bps)
    { sendProgress_->onSpeed(bps); }
void MainWindow::onSendStatus(QString msg)
    { sendLog_->addEntry(msg); }
void MainWindow::onSendFinished(bool ok, QString msg)
{
    setSendBusy(false);
    sendLog_->addEntry(msg, !ok);
    if (!ok) QMessageBox::critical(this, "Transfer failed", msg);
}

// Recv slots
void MainWindow::onRecvProgress(qint64 t, qint64 total)
    { recvProgress_->onProgress(t, total); }
void MainWindow::onRecvSpeed(double bps)
    { recvProgress_->onSpeed(bps); }
void MainWindow::onRecvStatus(QString msg)
    { recvLog_->addEntry(msg); }
void MainWindow::onNewConnection(QString peer)
    { recvLog_->addEntry("New connection from " + peer); }
void MainWindow::onRecvFinished(bool ok, QString msg)
{
    recvLog_->addEntry(msg, !ok);
    if (!ok) QMessageBox::critical(this, "Receive failed", msg);
}

// ─────────────────────────────────────────────────────────────
void MainWindow::setSendBusy(bool busy)
{
    btnSend_->setEnabled(!busy);
    btnCancel_->setEnabled(busy);
    editFilePath_->setEnabled(!busy);
    editHost_->setEnabled(!busy);
    spinPort_->setEnabled(!busy);
}

void MainWindow::setServerRunning(bool running)
{
    serverRunning_ = running;
    btnServerToggle_->setText(running ? "■  Stop Server" : "▶  Start Server");
    spinServerPort_->setEnabled(!running);
    editSaveDir_->setEnabled(!running);
    btnBrowseSaveDir_->setEnabled(!running);
    labelServerStatus_->setText(running ? "● Running" : "● Stopped");
    labelServerStatus_->setStyleSheet(
        running ? "color: green; font-weight: bold;" : "color: gray;");
}
