#pragma once
#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QString>
#include <QByteArray>
#include <vector>
#include <mutex>
#include <atomic>
#include "Protocol.h"
#include "ResumeManager.h"
#include "ThreadPool.h"

class RecvWorker : public QObject {
    Q_OBJECT
public:
    explicit RecvWorker(QObject* parent = nullptr);
    ~RecvWorker();

    void startListening(quint16 port, const QString& saveDir);
    void stopListening();

signals:
    void progressUpdated(qint64 transferred, qint64 total);
    void speedUpdated(double bytesPerSec);
    void transferFinished(bool success, QString msg);
    void statusMessage(QString msg);
    void newConnection(QString peerAddr);

private slots:
    void onNewConnection();
    void onReadyRead();
    void onClientDisconnected();
    void onSpeedTimer();

private:
    void handleFileInfo(const FileInfoPayload& fip);
    void handleChunkData(const ChunkDataHeader& cdh,
                         const char* data, uint32_t dataLen);
    void sendAck(uint32_t chunkIndex, bool ok);
    void writeChunk(uint32_t idx, const QByteArray& data);
    void finalizeTransfer();

    QTcpServer*  server_     = nullptr;
    QTcpSocket*  client_     = nullptr;
    ThreadPool*  pool_       = nullptr;
    QString      saveDir_;

    QString   filename_;
    QString   filePath_;
    qint64    fileSize_     = 0;
    uint32_t  totalChunks_  = 0;
    uint32_t  chunkSize_    = 0;
    QString   expectedMD5_;

    std::vector<bool>  ackBitmap_;
    std::mutex         bitmapMutex_;
    std::atomic<uint32_t> recvCount_ {0};

    ResumeManager* resumeMgr_ = nullptr;
    QByteArray     recvBuf_;
    bool           headerParsed_ = false;   // FILE_INFO received

    // speed
    qint64 speedBytes_     = 0;
    qint64 speedBytesLast_ = 0;
    qint64 speedTimeLast_  = 0;
};
