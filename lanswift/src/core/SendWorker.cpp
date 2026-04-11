#include "SendWorker.h"
#include "Protocol.h"
#include "MD5Helper.h"

#include <QFileInfo>
#include <QDateTime>
#include <QTimer>
#include <QDebug>
#include <QHostAddress>

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <stdexcept>
#include <algorithm>

static const uint32_t CHUNK_BYTES =
    static_cast<uint32_t>(DEFAULT_CHUNK_SIZE_MB) * 1024 * 1024;

// ─────────────────────────────────────────────────────────────
SendWorker::SendWorker(QObject* parent)
    : QObject(parent)
    , windowSize_(DEFAULT_WINDOW_SIZE)
{
    sem_init(&windowSem_, 0, static_cast<unsigned>(windowSize_));
    unsigned n = std::max(2u, std::thread::hardware_concurrency());
    pool_ = new ThreadPool(n);
}

SendWorker::~SendWorker()
{
    cancelled_ = true;
    for (int i = 0; i < windowSize_ * 2; ++i) sem_post(&windowSem_);
    delete pool_;
    sem_destroy(&windowSem_);
}

// ─────────────────────────────────────────────────────────────
void SendWorker::cancelTransfer()
{
    cancelled_ = true;
    for (int i = 0; i < windowSize_ * 2; ++i) sem_post(&windowSem_);
    if (socket_ && socket_->state() != QAbstractSocket::UnconnectedState)
        socket_->disconnectFromHost();
}

// ─────────────────────────────────────────────────────────────
void SendWorker::startTransfer(const QString& filePath,
                                const QString& host,
                                quint16 port)
{
    filePath_   = filePath;
    host_       = host;
    port_       = port;
    cancelled_  = false;
    ackedCount_ = 0;

    QFileInfo fi(filePath);
    if (!fi.exists()) {
        emit transferFinished(false, "File not found: " + filePath);
        return;
    }
    fileSize_    = fi.size();
    chunkSize_   = CHUNK_BYTES;
    totalChunks_ = static_cast<uint32_t>(
        (fileSize_ + chunkSize_ - 1) / chunkSize_);
    ackBitmap_.assign(totalChunks_, false);

    // Speed timer
    auto* speedTimer = new QTimer(this);
    connect(speedTimer, &QTimer::timeout, this, &SendWorker::onSpeedTimer);
    speedTimeLast_ = QDateTime::currentMSecsSinceEpoch();
    speedTimer->start(500);

    // Compute file MD5 in background, then connect
    emit statusMessage("Computing file MD5...");
    pool_->enqueue([this] {
        std::string md5;
        try {
            md5 = MD5Helper::computeFile(filePath_.toStdString());
        } catch (std::exception& e) {
            QMetaObject::invokeMethod(this, [this, e] {
                emit transferFinished(false,
                    QString("MD5 error: ") + e.what());
            }, Qt::QueuedConnection);
            return;
        }
        fileMD5_ = QString::fromStdString(md5);
        QMetaObject::invokeMethod(this, [this] {
            // Create socket on main thread
            socket_ = new QTcpSocket(this);
            connect(socket_, &QTcpSocket::connected,
                    this, &SendWorker::onConnected);
            connect(socket_, &QTcpSocket::readyRead,
                    this, &SendWorker::onReadyRead);
            connect(socket_,
                &QAbstractSocket::errorOccurred,
                this, &SendWorker::onSocketError);
            emit statusMessage("Connecting to " + host_ + ":" +
                               QString::number(port_));
            socket_->connectToHost(host_, port_);
        }, Qt::QueuedConnection);
    });
}

// ─────────────────────────────────────────────────────────────
bool SendWorker::sendAll(const QByteArray& data)
{
    qint64 sent = 0;
    while (sent < data.size()) {
        qint64 n = socket_->write(data.constData() + sent,
                                  data.size() - sent);
        if (n <= 0) return false;
        sent += n;
        if (!socket_->waitForBytesWritten(5000)) return false;
    }
    return true;
}

// ─────────────────────────────────────────────────────────────
void SendWorker::onConnected()
{
    emit statusMessage("Connected. Sending file info...");

    FileInfoPayload fip;
    memset(&fip, 0, sizeof(fip));
    fip.file_size    = static_cast<uint64_t>(fileSize_);
    fip.total_chunks = totalChunks_;
    fip.chunk_size   = chunkSize_;
    strncpy(fip.file_md5,
            fileMD5_.toLocal8Bit().constData(), 32);
    strncpy(fip.filename,
            QFileInfo(filePath_).fileName().toLocal8Bit().constData(), 255);

    std::string frame = makeFrame(FT_FILE_INFO, &fip, sizeof(fip));
    QByteArray ba(frame.data(), static_cast<int>(frame.size()));
    if (!sendAll(ba)) {
        emit transferFinished(false, "Failed to send file info");
        return;
    }
    // Server will respond with RESUME_REQ or we just start sending
    // We initiate a fresh dispatch (server ignores duplicates via ACK)
    dispatchChunks();
}

// ─────────────────────────────────────────────────────────────
void SendWorker::dispatchChunks()
{
    for (uint32_t i = 0; i < totalChunks_; ++i) {
        {
            std::lock_guard<std::mutex> lk(bitmapMutex_);
            if (ackBitmap_[i]) continue;
        }
        if (cancelled_) return;
        sem_wait(&windowSem_);           // blocks when window is full
        if (cancelled_) { sem_post(&windowSem_); return; }
        pool_->enqueue([this, i] { sendChunkTask(i); });
    }
}

// ─────────────────────────────────────────────────────────────
// Runs on a pool thread
void SendWorker::sendChunkTask(uint32_t idx)
{
    if (cancelled_) { sem_post(&windowSem_); return; }

    const off_t  offset    = static_cast<off_t>(idx) * chunkSize_;
    const size_t thisChunk = (idx == totalChunks_ - 1)
        ? static_cast<size_t>(fileSize_ - offset)
        : static_cast<size_t>(chunkSize_);

    // mmap read
    int fd = ::open(filePath_.toLocal8Bit().constData(), O_RDONLY);
    if (fd < 0) { sem_post(&windowSem_); return; }

    long   pageSize     = sysconf(_SC_PAGESIZE);
    off_t  alignOffset  = (offset / pageSize) * pageSize;
    size_t extra        = static_cast<size_t>(offset - alignOffset);
    size_t mapLen       = thisChunk + extra;

    void* addr = mmap(nullptr, mapLen, PROT_READ,
                      MAP_PRIVATE | MAP_POPULATE, fd, alignOffset);
    ::close(fd);
    if (addr == MAP_FAILED) { sem_post(&windowSem_); return; }

    madvise(addr, mapLen, MADV_SEQUENTIAL);
    const char* src = static_cast<const char*>(addr) + extra;

    // MD5
    std::string md5 = MD5Helper::compute(src, thisChunk);

    // Build frame
    size_t frameSize = sizeof(FrameHeader) +
                       sizeof(ChunkDataHeader) + thisChunk;
    QByteArray frame(static_cast<int>(frameSize), Qt::Uninitialized);

    FrameHeader fhdr;
    fhdr.magic    = PROTO_MAGIC;
    fhdr.version  = PROTO_VERSION;
    fhdr.type     = FT_CHUNK_DATA;
    fhdr.flags    = 0;
    fhdr.reserved = 0;

    ChunkDataHeader cdh;
    memset(&cdh, 0, sizeof(cdh));
    cdh.chunk_index = idx;
    cdh.chunk_size  = static_cast<uint32_t>(thisChunk);
    strncpy(cdh.chunk_md5, md5.c_str(), 32);

    char* p = frame.data();
    memcpy(p, &fhdr, sizeof(fhdr));                p += sizeof(fhdr);
    memcpy(p, &cdh,  sizeof(cdh));                 p += sizeof(cdh);
    memcpy(p, src,   thisChunk);

    munmap(addr, mapLen);

    // Send on main thread (QTcpSocket is not thread-safe)
    QMetaObject::invokeMethod(this, [this, frame, idx] {
        if (cancelled_) { sem_post(&windowSem_); return; }
        if (!sendAll(frame)) {
            sem_post(&windowSem_);
            emit transferFinished(false, "Write error on chunk " +
                                  QString::number(idx));
        }
        // sem_post happens in onReadyRead when ACK arrives
    }, Qt::QueuedConnection);
}

// ─────────────────────────────────────────────────────────────
void SendWorker::onReadyRead()
{
    recvBuf_ += socket_->readAll();

    while (true) {
        if (recvBuf_.size() < static_cast<int>(sizeof(FrameHeader))) break;

        FrameHeader hdr;
        memcpy(&hdr, recvBuf_.constData(), sizeof(FrameHeader));
        if (hdr.magic != PROTO_MAGIC) { recvBuf_.clear(); return; }

        if (hdr.type == FT_CHUNK_ACK || hdr.type == FT_CHUNK_NACK) {
            int needed = static_cast<int>(sizeof(FrameHeader) +
                                          sizeof(ChunkAckPayload));
            if (recvBuf_.size() < needed) break;

            ChunkAckPayload cap;
            memcpy(&cap,
                   recvBuf_.constData() + sizeof(FrameHeader),
                   sizeof(cap));
            recvBuf_.remove(0, needed);

            if (hdr.type == FT_CHUNK_ACK) {
                {
                    std::lock_guard<std::mutex> lk(bitmapMutex_);
                    ackBitmap_[cap.chunk_index] = true;
                }
                uint32_t cnt = ++ackedCount_;
                sem_post(&windowSem_);  // release window slot

                qint64 transferred = (cnt < totalChunks_)
                    ? static_cast<qint64>(cnt) * chunkSize_
                    : fileSize_;
                speedBytes_ = transferred;
                emit progressUpdated(transferred, fileSize_);

                if (cnt == totalChunks_) {
                    emit transferFinished(true,
                        "Transfer completed successfully!");
                    socket_->disconnectFromHost();
                    return;
                }
            } else {
                // NACK → retransmit (window slot stays "in use")
                uint32_t idx = cap.chunk_index;
                emit statusMessage(
                    QString("Retransmitting chunk %1...").arg(idx));
                pool_->enqueue([this, idx] { sendChunkTask(idx); });
            }
            continue;
        }

        if (hdr.type == FT_RESUME_REQ) {
            int hdrNeeded = static_cast<int>(sizeof(FrameHeader) +
                                              sizeof(ResumeReqHeader));
            if (recvBuf_.size() < hdrNeeded) break;
            ResumeReqHeader rrh;
            memcpy(&rrh,
                   recvBuf_.constData() + sizeof(FrameHeader),
                   sizeof(rrh));
            int bitmapBytes = static_cast<int>((rrh.total_chunks + 7) / 8);
            int total       = hdrNeeded + bitmapBytes;
            if (recvBuf_.size() < total) break;

            const uint8_t* bm = reinterpret_cast<const uint8_t*>(
                recvBuf_.constData() + hdrNeeded);
            {
                std::lock_guard<std::mutex> lk(bitmapMutex_);
                for (uint32_t i = 0; i < rrh.total_chunks; ++i) {
                    bool done = (bm[i/8] >> (i%8)) & 1;
                    if (done && !ackBitmap_[i]) {
                        ackBitmap_[i] = true;
                        ++ackedCount_;
                    }
                }
            }
            recvBuf_.remove(0, total);
            emit statusMessage(
                QString("Resuming: %1/%2 chunks already done")
                    .arg(ackedCount_.load()).arg(totalChunks_));
            continue;
        }

        // Unknown: flush
        recvBuf_.clear();
        break;
    }
}

// ─────────────────────────────────────────────────────────────
void SendWorker::onSocketError(QAbstractSocket::SocketError)
{
    if (!cancelled_) {
        emit transferFinished(false,
            "Socket error: " + socket_->errorString());
    }
}

void SendWorker::onSpeedTimer()
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
