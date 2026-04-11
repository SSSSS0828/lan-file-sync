#include "RecvWorker.h"
#include "Protocol.h"
#include "MD5Helper.h"

#include <QDateTime>
#include <QTimer>
#include <QDir>
#include <QDebug>

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <algorithm>

RecvWorker::RecvWorker(QObject* parent)
    : QObject(parent)
{
    pool_ = new ThreadPool(
        std::max(2u, std::thread::hardware_concurrency()));
}

RecvWorker::~RecvWorker()
{
    delete pool_;
    delete resumeMgr_;
}

// ─────────────────────────────────────────────────────────────
void RecvWorker::startListening(quint16 port, const QString& saveDir)
{
    saveDir_ = saveDir;
    QDir().mkpath(saveDir);

    resumeMgr_ = new ResumeManager(saveDir.toStdString());

    server_ = new QTcpServer(this);
    connect(server_, &QTcpServer::newConnection,
            this, &RecvWorker::onNewConnection);

    if (!server_->listen(QHostAddress::Any, port)) {
        emit transferFinished(false,
            "Cannot listen on port " + QString::number(port) +
            ": " + server_->errorString());
        return;
    }
    emit statusMessage("Listening on port " + QString::number(port));

    auto* speedTimer = new QTimer(this);
    connect(speedTimer, &QTimer::timeout, this, &RecvWorker::onSpeedTimer);
    speedTimeLast_ = QDateTime::currentMSecsSinceEpoch();
    speedTimer->start(500);
}

void RecvWorker::stopListening()
{
    if (server_) server_->close();
    if (client_) client_->disconnectFromHost();
}

// ─────────────────────────────────────────────────────────────
void RecvWorker::onNewConnection()
{
    if (client_) {
        // Only one client at a time; reject extra connections
        QTcpSocket* extra = server_->nextPendingConnection();
        extra->disconnectFromHost();
        extra->deleteLater();
        return;
    }
    client_ = server_->nextPendingConnection();
    connect(client_, &QTcpSocket::readyRead,
            this, &RecvWorker::onReadyRead);
    connect(client_, &QTcpSocket::disconnected,
            this, &RecvWorker::onClientDisconnected);

    emit newConnection(client_->peerAddress().toString());
    emit statusMessage("Connection from " +
                        client_->peerAddress().toString());
}

// ─────────────────────────────────────────────────────────────
void RecvWorker::onReadyRead()
{
    recvBuf_ += client_->readAll();

    while (true) {
        if (recvBuf_.size() < static_cast<int>(sizeof(FrameHeader))) break;

        FrameHeader hdr;
        memcpy(&hdr, recvBuf_.constData(), sizeof(FrameHeader));
        if (hdr.magic != PROTO_MAGIC) { recvBuf_.clear(); return; }

        if (hdr.type == FT_FILE_INFO) {
            int needed = static_cast<int>(sizeof(FrameHeader) +
                                          sizeof(FileInfoPayload));
            if (recvBuf_.size() < needed) break;

            FileInfoPayload fip;
            memcpy(&fip,
                   recvBuf_.constData() + sizeof(FrameHeader),
                   sizeof(fip));
            recvBuf_.remove(0, needed);
            handleFileInfo(fip);
            continue;
        }

        if (hdr.type == FT_CHUNK_DATA) {
            int hdrNeeded = static_cast<int>(sizeof(FrameHeader) +
                                              sizeof(ChunkDataHeader));
            if (recvBuf_.size() < hdrNeeded) break;

            ChunkDataHeader cdh;
            memcpy(&cdh,
                   recvBuf_.constData() + sizeof(FrameHeader),
                   sizeof(cdh));

            int total = hdrNeeded + static_cast<int>(cdh.chunk_size);
            if (recvBuf_.size() < total) break;

            const char* data =
                recvBuf_.constData() + hdrNeeded;
            handleChunkData(cdh, data, cdh.chunk_size);
            recvBuf_.remove(0, total);
            continue;
        }

        // Unknown: flush
        recvBuf_.clear();
        break;
    }
}

// ─────────────────────────────────────────────────────────────
void RecvWorker::handleFileInfo(const FileInfoPayload& fip)
{
    if (headerParsed_) return;  // already set up
    headerParsed_ = true;

    fileSize_    = static_cast<qint64>(fip.file_size);
    totalChunks_ = fip.total_chunks;
    chunkSize_   = fip.chunk_size;
    filename_    = QString::fromLocal8Bit(fip.filename);
    expectedMD5_ = QString::fromLatin1(fip.file_md5, 32);

    // Sanitize filename
    filename_.replace("/", "_").replace("..", "_");
    filePath_ = saveDir_ + "/" + filename_;

    emit statusMessage(
        QString("Receiving: %1  (%2 chunks × %3 MB)")
            .arg(filename_)
            .arg(totalChunks_)
            .arg(chunkSize_ / 1024.0 / 1024.0, 0, 'f', 1));

    // Pre-allocate file
    int fd = ::open(filePath_.toLocal8Bit().constData(),
                    O_RDWR | O_CREAT, 0644);
    if (fd >= 0) {
        if (::ftruncate(fd, fileSize_) == 0) {
            // good
        }
        ::close(fd);
    }

    // Check for resume
    std::vector<bool> bitmap =
        resumeMgr_->load(filename_.toStdString(), totalChunks_);

    {
        std::lock_guard<std::mutex> lk(bitmapMutex_);
        ackBitmap_ = bitmap;
        for (bool b : ackBitmap_)
            if (b) ++recvCount_;
    }

    uint32_t done = recvCount_.load();
    if (done > 0) {
        // Send RESUME_REQ with current bitmap
        uint32_t bmBytes = (totalChunks_ + 7) / 8;
        QByteArray frame(
            static_cast<int>(sizeof(FrameHeader) +
                              sizeof(ResumeReqHeader) + bmBytes),
            0);

        FrameHeader fhdr;
        fhdr.magic    = PROTO_MAGIC;
        fhdr.version  = PROTO_VERSION;
        fhdr.type     = FT_RESUME_REQ;
        fhdr.flags    = 0;
        fhdr.reserved = 0;

        ResumeReqHeader rrh;
        rrh.total_chunks = totalChunks_;

        char* p = frame.data();
        memcpy(p, &fhdr, sizeof(fhdr));  p += sizeof(fhdr);
        memcpy(p, &rrh,  sizeof(rrh));   p += sizeof(rrh);

        uint8_t* bm = reinterpret_cast<uint8_t*>(p);
        for (uint32_t i = 0; i < totalChunks_; ++i)
            if (ackBitmap_[i])
                bm[i/8] |= (1 << (i%8));

        client_->write(frame);
        client_->flush();

        emit statusMessage(
            QString("Resume: %1/%2 chunks already done")
                .arg(done).arg(totalChunks_));
    }

    emit progressUpdated(
        static_cast<qint64>(done) * chunkSize_, fileSize_);
}

// ─────────────────────────────────────────────────────────────
void RecvWorker::handleChunkData(const ChunkDataHeader& cdh,
                                   const char* data, uint32_t dataLen)
{
    // Already acked? send ACK again (dedup)
    {
        std::lock_guard<std::mutex> lk(bitmapMutex_);
        if (ackBitmap_[cdh.chunk_index]) {
            sendAck(cdh.chunk_index, true);
            return;
        }
    }

    // Verify MD5
    std::string got = MD5Helper::compute(data, dataLen);
    std::string exp(cdh.chunk_md5, 32);
    if (got != exp) {
        emit statusMessage(
            QString("Chunk %1 MD5 mismatch!").arg(cdh.chunk_index));
        sendAck(cdh.chunk_index, false);  // NACK
        return;
    }

    // Write via mmap
    writeChunk(cdh.chunk_index,
               QByteArray(data, static_cast<int>(dataLen)));
}

// ─────────────────────────────────────────────────────────────
void RecvWorker::writeChunk(uint32_t idx, const QByteArray& data)
{
    // Offload write to pool thread
    pool_->enqueue([this, idx, data] {
        off_t offset = static_cast<off_t>(idx) * chunkSize_;
        size_t len   = static_cast<size_t>(data.size());

        int fd = ::open(filePath_.toLocal8Bit().constData(),
                        O_RDWR);
        if (fd < 0) {
            QMetaObject::invokeMethod(this, [this, idx] {
                sendAck(idx, false);
            }, Qt::QueuedConnection);
            return;
        }

        void* addr = mmap(nullptr, len, PROT_WRITE,
                          MAP_SHARED, fd, offset);
        ::close(fd);

        if (addr == MAP_FAILED) {
            QMetaObject::invokeMethod(this, [this, idx] {
                sendAck(idx, false);
            }, Qt::QueuedConnection);
            return;
        }

        memcpy(addr, data.constData(), len);
        msync(addr, len, MS_ASYNC);
        munmap(addr, len);

        // Back to main thread: update state
        QMetaObject::invokeMethod(this, [this, idx] {
            {
                std::lock_guard<std::mutex> lk(bitmapMutex_);
                ackBitmap_[idx] = true;
            }
            uint32_t cnt = ++recvCount_;
            sendAck(idx, true);

            qint64 transferred = (cnt < totalChunks_)
                ? static_cast<qint64>(cnt) * chunkSize_
                : fileSize_;
            speedBytes_ = transferred;
            emit progressUpdated(transferred, fileSize_);

            // Save resume state every 16 chunks
            if (cnt % 16 == 0)
                resumeMgr_->save(filename_.toStdString(), ackBitmap_);

            if (cnt == totalChunks_)
                finalizeTransfer();

        }, Qt::QueuedConnection);
    });
}

// ─────────────────────────────────────────────────────────────
void RecvWorker::sendAck(uint32_t chunkIndex, bool ok)
{
    ChunkAckPayload cap;
    cap.chunk_index = chunkIndex;
    std::string frame = makeFrame(
        ok ? FT_CHUNK_ACK : FT_CHUNK_NACK,
        &cap, sizeof(cap));
    if (client_ && client_->state() == QAbstractSocket::ConnectedState) {
        client_->write(frame.data(),
                        static_cast<qint64>(frame.size()));
    }
}

// ─────────────────────────────────────────────────────────────
void RecvWorker::finalizeTransfer()
{
    emit statusMessage("All chunks received. Verifying full file MD5...");

    pool_->enqueue([this] {
        std::string md5;
        try {
            md5 = MD5Helper::computeFile(filePath_.toStdString());
        } catch (...) {
            QMetaObject::invokeMethod(this, [this] {
                emit transferFinished(false, "File MD5 verification error");
            }, Qt::QueuedConnection);
            return;
        }

        bool ok = (md5 == expectedMD5_.toStdString());
        QMetaObject::invokeMethod(this, [this, ok, md5] {
            if (ok) {
                resumeMgr_->remove(filename_.toStdString());
                emit progressUpdated(fileSize_, fileSize_);
                emit transferFinished(true,
                    "✓ Transfer complete. MD5 verified: " +
                    QString::fromStdString(md5));
            } else {
                emit transferFinished(false,
                    "✗ MD5 mismatch! File may be corrupted.\n"
                    "  Expected: " + expectedMD5_ + "\n"
                    "  Got:      " + QString::fromStdString(md5));
            }
            // Reset for next transfer
            headerParsed_ = false;
            recvCount_    = 0;
            client_->disconnectFromHost();
            client_ = nullptr;
        }, Qt::QueuedConnection);
    });
}

// ─────────────────────────────────────────────────────────────
void RecvWorker::onClientDisconnected()
{
    // If transfer was incomplete, save resume state
    if (headerParsed_ && recvCount_.load() < totalChunks_) {
        std::lock_guard<std::mutex> lk(bitmapMutex_);
        resumeMgr_->save(filename_.toStdString(), ackBitmap_);
        emit statusMessage("Client disconnected. Progress saved for resume.");
    }
    if (client_) {
        client_->deleteLater();
        client_ = nullptr;
    }
    headerParsed_ = false;
    recvCount_    = 0;
}

void RecvWorker::onSpeedTimer()
{
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    double dt  = (now - speedTimeLast_) / 1000.0;
    if (dt > 0) {
        double spd = (speedBytes_ - speedBytesLast_) / dt;
        emit speedUpdated(spd > 0 ? spd : 0);
    }
    speedBytesLast_ = speedBytes_;
    speedTimeLast_  = now;
}
