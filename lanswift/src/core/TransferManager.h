#pragma once
#include <QObject>
#include <QString>
#include "SendWorker.h"
#include "RecvWorker.h"

// Thin facade used by the UI to drive send/receive operations.
class TransferManager : public QObject {
    Q_OBJECT
public:
    explicit TransferManager(QObject* parent = nullptr);

    SendWorker* sender()   { return sender_; }
    RecvWorker* receiver() { return receiver_; }

    void sendFile(const QString& filePath,
                  const QString& host,
                  quint16 port);

    void startServer(quint16 port, const QString& saveDir);
    void stopServer();
    void cancelSend();

private:
    SendWorker* sender_   = nullptr;
    RecvWorker* receiver_ = nullptr;
};
