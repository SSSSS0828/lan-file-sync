#pragma once
#include <QObject>
#include <QTcpSocket>
#include <QString>
#include <QByteArray>
#include <atomic>
#include <mutex>
#include <vector>
#include <semaphore.h>
#include "ThreadPool.h"

class SendWorker : public QObject {
    Q_OBJECT
public:
    explicit SendWorker(QObject* parent = nullptr);
    ~SendWorker();

    void startTransfer(const QString& filePath,
                       const QString& host,
                       quint16 port);
    void cancelTransfer();

signals:
    void progressUpdated(qint64 transferred, qint64 total);
    void speedUpdated(double bytesPerSec);
    void transferFinished(bool success, QString msg);
    void statusMessage(QString msg);

private slots:
    void onConnected();
    void onReadyRead();
    void onSocketError(QAbstractSocket::SocketError err);
    void onSpeedTimer();

private:
    // Must be called from main thread
    bool sendAll(const QByteArray& data);

    // Dispatch all un-acked chunks into the thread pool
    void dispatchChunks();

    // Called by pool thread: builds frame, then queues send on main thread
    void sendChunkTask(uint32_t idx);

    QString     filePath_;
    QString     host_;
    quint16     port_         = 9527;
    qint64      fileSize_     = 0;
    uint32_t    totalChunks_  = 0;
    uint32_t    chunkSize_    = 0;
    QString     fileMD5_;

    QTcpSocket*           socket_    = nullptr;
    ThreadPool*           pool_      = nullptr;
    sem_t                 windowSem_;
    int                   windowSize_;

    std::atomic<bool>     cancelled_  {false};
    std::atomic<uint32_t> ackedCount_ {0};
    std::vector<bool>     ackBitmap_;
    std::mutex            bitmapMutex_;

    QByteArray  recvBuf_;

    // speed
    qint64 speedBytes_     = 0;
    qint64 speedBytesLast_ = 0;
    qint64 speedTimeLast_  = 0;
};
