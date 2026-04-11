#pragma once
#include <QMainWindow>
#include <QTabWidget>
#include <QLineEdit>
#include <QSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QGroupBox>
#include "ProgressWidget.h"
#include "FileListView.h"
#include "../core/TransferManager.h"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    // Send tab
    void onBrowseFile();
    void onSendStart();
    void onSendCancel();

    // Recv tab
    void onServerToggle();

    // Shared handlers
    void onSendProgress(qint64 t, qint64 total);
    void onSendSpeed(double bps);
    void onSendFinished(bool ok, QString msg);
    void onSendStatus(QString msg);

    void onRecvProgress(qint64 t, qint64 total);
    void onRecvSpeed(double bps);
    void onRecvFinished(bool ok, QString msg);
    void onRecvStatus(QString msg);
    void onNewConnection(QString peer);

private:
    void buildSendTab(QWidget* tab);
    void buildRecvTab(QWidget* tab);
    void setSendBusy(bool busy);
    void setServerRunning(bool running);

    TransferManager* mgr_ = nullptr;
    QTabWidget*      tabs_;

    // Send tab widgets
    QLineEdit*    editFilePath_;
    QLineEdit*    editHost_;
    QSpinBox*     spinPort_;
    QPushButton*  btnSend_;
    QPushButton*  btnCancel_;
    ProgressWidget* sendProgress_;
    FileListView*   sendLog_;

    // Recv tab widgets
    QSpinBox*     spinServerPort_;
    QLineEdit*    editSaveDir_;
    QPushButton*  btnBrowseSaveDir_;
    QPushButton*  btnServerToggle_;
    QLabel*       labelServerStatus_;
    ProgressWidget* recvProgress_;
    FileListView*   recvLog_;

    bool serverRunning_ = false;
};
