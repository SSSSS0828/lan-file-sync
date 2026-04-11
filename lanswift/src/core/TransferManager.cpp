#include "TransferManager.h"

TransferManager::TransferManager(QObject* parent)
    : QObject(parent)
    , sender_(new SendWorker(this))
    , receiver_(new RecvWorker(this))
{}

void TransferManager::sendFile(const QString& filePath,
                                const QString& host,
                                quint16 port)
{
    sender_->startTransfer(filePath, host, port);
}

void TransferManager::startServer(quint16 port, const QString& saveDir)
{
    receiver_->startListening(port, saveDir);
}

void TransferManager::stopServer()
{
    receiver_->stopListening();
}

void TransferManager::cancelSend()
{
    sender_->cancelTransfer();
}
