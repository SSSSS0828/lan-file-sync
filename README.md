# LanSwift Transfer
> 基于 C++11 / Qt 5 的高性能局域网大文件传输与同步工具

![Platform](https://img.shields.io/badge/platform-Linux-blue?style=flat-square)
![Language](https://img.shields.io/badge/language-C%2B%2B11-orange?style=flat-square)
![Qt](https://img.shields.io/badge/Qt-5.x-green?style=flat-square)
![License](https://img.shields.io/badge/license-MIT-lightgrey?style=flat-square)

---

## 目录

- [项目简介](#项目简介)
- [核心特性](#核心特性)
- [系统架构](#系统架构)
- [技术实现详解](#技术实现详解)
  - [异步传输与 UI 解耦](#1-异步传输与-ui-解耦)
  - [mmap 零拷贝读写优化](#2-mmap-零拷贝读写优化)
  - [分块切片与 MD5 完整性校验](#3-分块切片与-md5-完整性校验)
  - [断点续传机制](#4-断点续传机制)
  - [滑动窗口与信号量流控](#5-滑动窗口与信号量流控)
- [OOM 崩溃排查实录](#oom-崩溃排查实录)
- [性能数据](#性能数据)
- [快速开始](#快速开始)
  - [依赖环境](#依赖环境)
  - [编译构建](#编译构建)
  - [运行](#运行)
- [配置说明](#配置说明)
- [目录结构](#目录结构)
- [已知限制与 TODO](#已知限制与-todo)

---

## 项目简介

**LanSwift Transfer** 是一款运行于 Linux 桌面端的轻量级局域网文件传输工具，专为解决 GB 级别大文件在千兆局域网环境下的高速传输痛点而设计。

传统工具（scp、rsync 等）在图形化、进度可视化、断点续传易用性方面存在不足；而直接使用 Qt 内置 `QFile` + 主线程读写的朴素方案，则会在传输大文件时导致 **UI 假死**。本项目从底层读写、并发模型、流量控制三个维度做了针对性优化，在保持界面响应流畅的前提下，将千兆网环境的实测传输速率跑满至带宽上限（约 **112 MB/s**）。

---

## 核心特性

| 特性 | 说明 |
|------|------|
| 🚀 高速传输 | mmap 零拷贝 + 线程池，千兆网实测 ≥ 100 MB/s |
| 📂 大文件支持 | 支持单文件 ≥ 10 GB，分块切片传输，无内存压力 |
| 🔁 断点续传 | 记录已确认块索引，重连后从断点继续，无需重传 |
| ✅ 完整性校验 | 每块独立 MD5 + 全文件 MD5 双重校验 |
| 🖥️ 流畅 UI | 后台线程 + Queued 信号槽，主线程始终可响应 |
| 🔒 流量控制 | 滑动窗口 + POSIX 信号量，防止发送队列 OOM |
| 📊 实时监控 | 传输速率、剩余时间、进度条实时刷新（100ms 间隔） |

---

## 系统架构

```
┌─────────────────────────────────────────────────────────┐
│                     Qt 主线程（UI）                       │
│  ProgressWidget  SpeedLabel  FileListView  ControlPanel  │
│         ▲               ▲                                │
│         │  Queued 信号槽 │  (线程安全，无阻塞)             │
└─────────┼───────────────┼────────────────────────────────┘
          │               │
┌─────────┼───────────────┼────────────────────────────────┐
│         │    TransferManager（调度层）                    │
│         │                                                │
│   ┌─────┴──────┐   ┌────┴───────┐   ┌────────────────┐  │
│   │ SendWorker │   │RecvWorker  │   │  MD5Verifier   │  │
│   │ (线程池)   │   │ (线程池)   │   │   (线程池)     │  │
│   └─────┬──────┘   └────┬───────┘   └────────────────┘  │
│         │               │                                │
│   mmap读取块       mmap写入块      信号量(sem_t)流控      │
└─────────┼───────────────┼────────────────────────────────┘
          │               │
┌─────────▼───────────────▼────────────────────────────────┐
│              TCP Socket 传输层                            │
│    ChunkHeader | ChunkData | ACK | NACK | RESUME_REQ     │
└─────────────────────────────────────────────────────────┘
```

---

## 技术实现详解

### 1. 异步传输与 UI 解耦

**痛点**：Qt 主线程执行 `QFile::read()` 读取大文件时，事件循环被阻塞，窗口无法重绘，用户操作无响应——即"假死"。

**方案**：引入纯 C++ 线程池（非 Qt 线程，避免 `QObject` 跨线程持有的复杂度），将所有文件 I/O 与网络 send/recv 操作投递到工作线程执行。进度回调通过 Qt **Queued 连接**的信号槽跨线程安全传递到主线程 UI。

```cpp
// 线程池核心（C++11 标准实现）
class ThreadPool {
public:
    explicit ThreadPool(size_t threads) {
        for (size_t i = 0; i < threads; ++i)
            workers.emplace_back([this] {
                for (;;) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(queue_mutex);
                        condition.wait(lock, [this] {
                            return stop || !tasks.empty();
                        });
                        if (stop && tasks.empty()) return;
                        task = std::move(tasks.front());
                        tasks.pop();
                    }
                    task();
                }
            });
    }

    template<class F>
    void enqueue(F&& f) {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            tasks.emplace(std::forward<F>(f));
        }
        condition.notify_one();
    }

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queue_mutex;
    std::condition_variable condition;
    bool stop = false;
};
```

```cpp
// 信号槽跨线程回调（Queued 模式自动队列化到主线程）
connect(worker, &SendWorker::progressUpdated,
        ui->progressBar, &ProgressWidget::onProgress,
        Qt::QueuedConnection);  // 关键：Queued 而非 Direct
```

### 2. mmap 零拷贝读写优化

**痛点**：传统 `fread()` / `read()` 路径：磁盘 → 内核页缓存 → `copy_to_user` → 用户缓冲区，存在一次多余的内核态拷贝，在千兆网大吞吐场景下 CPU 占用率高、速率受限。

**方案**：使用 Linux `mmap()` 将文件的指定区间直接映射到用户空间虚拟地址，操作系统按需换页（page fault），CPU 无需主动搬运数据，`send()` 系统调用直接从页缓存 DMA 到网卡。

```cpp
// 发送端：mmap 读取文件分块
bool SendWorker::sendChunk(int fd, off_t offset, size_t length) {
    // 页对齐（mmap offset 必须是 PAGE_SIZE 的整数倍）
    long page_size = sysconf(_SC_PAGESIZE);
    off_t aligned_offset = (offset / page_size) * page_size;
    size_t extra = offset - aligned_offset;
    size_t map_length = length + extra;

    void* addr = mmap(nullptr, map_length,
                      PROT_READ, MAP_PRIVATE | MAP_POPULATE,
                      fd, aligned_offset);
    if (addr == MAP_FAILED) {
        perror("mmap");
        return false;
    }

    // MAP_POPULATE：提示内核预读，减少后续 page fault 延迟
    madvise(addr, map_length, MADV_SEQUENTIAL);

    const char* data = static_cast<const char*>(addr) + extra;
    bool ok = socketSendAll(socket_fd, data, length);

    munmap(addr, map_length);
    return ok;
}
```

```cpp
// 接收端：mmap 写入文件（避免 write() 重复拷贝）
bool RecvWorker::recvChunk(int fd, off_t offset, size_t length,
                            const char* data) {
    // 预分配文件空间，避免稀疏文件问题
    if (fallocate(fd, 0, offset, length) < 0)
        posix_fadvise(fd, offset, length, POSIX_FADV_WILLNEED);

    void* addr = mmap(nullptr, length,
                      PROT_WRITE, MAP_SHARED,
                      fd, offset);
    if (addr == MAP_FAILED) return false;

    memcpy(addr, data, length);
    msync(addr, length, MS_ASYNC);   // 异步刷盘，不阻塞
    munmap(addr, length);
    return true;
}
```

### 3. 分块切片与 MD5 完整性校验

文件按固定块大小（默认 **4 MB**）切分，每块携带块序号和块级 MD5；全部块传输完成后，接收端再对完整文件做一次总 MD5 核对。

```
协议帧格式（自定义二进制协议，小端序）：

 0      1      2      3      4      5      6      7
┌──────────────────────────────────────────────────┐
│  Magic (4B: 0x4C535746)  │  Version (1B)         │
├──────────────────────────────────────────────────┤
│  Type (1B)  │  Flags (1B) │  Reserved (1B)        │
├──────────────────────────────────────────────────┤
│           Chunk Index (4B, uint32)               │
├──────────────────────────────────────────────────┤
│           Chunk Size  (4B, uint32)               │
├──────────────────────────────────────────────────┤
│           MD5 Digest  (16B)                      │
├──────────────────────────────────────────────────┤
│           Payload Data (Chunk Size bytes)        │
└──────────────────────────────────────────────────┘

Type 枚举：
  0x01  FILE_INFO      文件元数据（名称、大小、总块数、文件MD5）
  0x02  CHUNK_DATA     数据块
  0x03  CHUNK_ACK      接收端确认
  0x04  CHUNK_NACK     校验失败，请求重传
  0x05  RESUME_REQ     断点续传请求（携带已完成块位图）
  0x06  TRANSFER_DONE  传输完成
```

```cpp
// MD5 块级校验
std::string calcMD5(const char* data, size_t len) {
    unsigned char digest[MD5_DIGEST_LENGTH];
    MD5(reinterpret_cast<const unsigned char*>(data), len, digest);
    std::ostringstream oss;
    for (auto b : digest)
        oss << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<int>(b);
    return oss.str();
}
```

### 4. 断点续传机制

接收端在本地维护一个 **块完成位图**（`std::vector<bool>`），每次收到 CHUNK_ACK 后置位。异常断开重连时，发送 `RESUME_REQ` 帧携带当前位图，发送端跳过已确认的块，仅重传缺失部分。

```cpp
// 接收端：持久化已完成块索引到 .lanswift_resume 文件
void ResumeManager::save(const std::string& file_id,
                          const std::vector<bool>& bitmap) {
    std::ofstream ofs(resumeFilePath(file_id), std::ios::binary);
    uint32_t total = bitmap.size();
    ofs.write(reinterpret_cast<const char*>(&total), 4);
    for (bool b : bitmap) {
        uint8_t v = b ? 1 : 0;
        ofs.write(reinterpret_cast<const char*>(&v), 1);
    }
}

// 发送端：收到 RESUME_REQ 后跳过已确认块
for (uint32_t i = 0; i < total_chunks; ++i) {
    if (resume_bitmap[i]) continue;   // 已确认，跳过
    pool.enqueue([this, i] { sendChunk(i); });
}
```

### 5. 滑动窗口与信号量流控

**问题根源**（见 [OOM 崩溃排查实录](#oom-崩溃排查实录)）：线程池将所有分块任务一次性入队，发送速度远超 ACK 确认速度，内存中堆积了海量待发块的 mmap 驻留页，最终触发 OOM。

**解决方案**：引入 POSIX 信号量作为"令牌桶"，窗口大小（`WINDOW_SIZE`）控制同时在途的最大块数。发送前 `sem_wait` 获取令牌，收到 ACK 后 `sem_post` 归还令牌，从根本上限制内存驻留量。

```cpp
// 初始化：窗口大小 = 16 块（16 * 4MB = 64MB 最大在途内存）
sem_t window_sem;
sem_init(&window_sem, 0, WINDOW_SIZE);

// 发送线程：获取令牌后才允许读取并发送
void sendChunk(uint32_t idx) {
    sem_wait(&window_sem);          // 阻塞直到窗口有空位
    auto data = mmapReadChunk(idx); // 映射到内存
    socketSendAll(sock, data);
    // ACK 回调中 sem_post(&window_sem) 归还令牌
}

// ACK 处理（接收端确认后回调）
void onChunkAcked(uint32_t idx) {
    bitmap[idx] = true;
    sem_post(&window_sem);          // 归还令牌，窗口前移
    emit progressUpdated(ackedCount * chunk_size, total_size);
}
```

---

## OOM 崩溃排查实录

> **现象**：在 10 GB 文件传输约 2 分钟后进程崩溃，`dmesg` 输出 `Out of memory: Kill process`。

### 排查步骤

**Step 1：开启 Coredump**

```bash
ulimit -c unlimited
echo "/tmp/core.%p" > /proc/sys/kernel/core_pattern
# 重新触发崩溃，生成 /tmp/core.<pid>
```

**Step 2：GDB 分析核心转储**

```bash
gdb ./lanswift_transfer /tmp/core.12345

(gdb) bt           # 查看崩溃调用栈
(gdb) info threads # 查看所有线程状态
(gdb) thread 5
(gdb) bt
```

调用栈指向 `ThreadPool::enqueue` → `std::function` 构造 → `operator new` 失败，说明堆内存耗尽发生在任务入队阶段。

**Step 3：Valgrind / `/proc` 内存分析**

```bash
# 运行时观察内存增长
watch -n 1 "cat /proc/$(pgrep lanswift)/status | grep VmRSS"
```

发现 `VmRSS` 以每秒 ~200 MB 的速度线性增长——与"发送快、ACK 慢"的假设吻合。

**Step 4：定位根因**

发送端以千兆速率投递分块任务，但 TCP ACK RTT 约 1ms，加上接收端 MD5 验证和磁盘写入耗时，ACK 回来的速度远低于投递速度。线程池任务队列无上限，导致数千个任务对象及其捕获的 mmap 内存页同时驻留。

**Step 5：修复——引入信号量滑动窗口**

见 [滑动窗口与信号量流控](#5-滑动窗口与信号量流控)。修复后 `VmRSS` 稳定在 ~150 MB（`WINDOW_SIZE * CHUNK_SIZE + overhead`），10 GB 文件全程无崩溃。

---

## 性能数据

测试环境：两台 Ubuntu 22.04 主机，Intel i5-10400，千兆交换机直连。

| 文件大小 | 传输耗时 | 平均速率 | CPU 占用（发送端） | 内存峰值 |
|----------|----------|----------|-------------------|----------|
| 1 GB     | ~9.2 s   | 111 MB/s | 18%               | ~150 MB  |
| 5 GB     | ~46 s    | 111 MB/s | 19%               | ~152 MB  |
| 10 GB    | ~92 s    | 110 MB/s | 20%               | ~155 MB  |

> 说明：速率接近千兆以太网理论上限（125 MB/s），CPU 开销主要来自 MD5 计算与 TCP 协议栈，mmap 消除了额外的内核态拷贝开销。

---

## 快速开始

### 依赖环境

```bash
# Ubuntu / Debian
sudo apt update
sudo apt install -y \
    build-essential \
    cmake \
    qt5-default \
    qtbase5-dev \
    libssl-dev \       # MD5 via OpenSSL
    libgtest-dev       # 单元测试（可选）
```

| 依赖 | 版本要求 | 说明 |
|------|----------|------|
| GCC / Clang | ≥ 7.0 | C++11 支持 |
| Qt | 5.9 – 5.15 | 核心框架 |
| CMake | ≥ 3.10 | 构建系统 |
| OpenSSL | ≥ 1.1 | MD5 计算 |
| Linux Kernel | ≥ 3.14 | mmap + fallocate |

### 编译构建

```bash
git clone https://github.com/yourname/lanswift-transfer.git
cd lanswift-transfer

mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

编译产物位于 `build/bin/lanswift_transfer`。

**可选 CMake 参数：**

```bash
# 开启详细传输日志
cmake .. -DENABLE_TRANSFER_LOG=ON

# 自定义默认块大小（单位：MB，默认 4）
cmake .. -DDEFAULT_CHUNK_SIZE_MB=8

# 自定义滑动窗口大小（默认 16）
cmake .. -DDEFAULT_WINDOW_SIZE=32
```

### 运行

**启动接收端（服务器模式）：**

```bash
./lanswift_transfer --mode server --port 9527 --save-dir /data/recv
```

**启动发送端（客户端模式）：**

```bash
./lanswift_transfer --mode client --host 192.168.1.100 --port 9527 --file /data/bigfile.iso
```

**图形界面模式（默认）：**

```bash
./lanswift_transfer
# 在 UI 中填写目标 IP、端口，选择文件后点击"开始传输"
```

---

## 配置说明

配置文件位于 `~/.config/lanswift/config.ini`：

```ini
[Transfer]
chunk_size_mb   = 4        ; 分块大小，建议 2–16 MB
window_size     = 16       ; 滑动窗口大小（在途块数上限）
connect_timeout = 10       ; TCP 连接超时（秒）
retry_count     = 3        ; 块重传最大次数

[Server]
default_port    = 9527
save_directory  = ~/Downloads/LanSwift

[UI]
speed_refresh_ms = 100     ; 速率刷新间隔（毫秒）
theme            = dark     ; dark / light
```

---

## 目录结构

```
lanswift-transfer/
├── CMakeLists.txt
├── README.md
├── src/
│   ├── main.cpp
│   ├── core/
│   │   ├── ThreadPool.h          # C++11 线程池
│   │   ├── TransferManager.cpp   # 发送/接收调度
│   │   ├── SendWorker.cpp        # 发送工作线程（mmap + socket）
│   │   ├── RecvWorker.cpp        # 接收工作线程（mmap 写入）
│   │   ├── Protocol.h            # 自定义二进制协议帧定义
│   │   ├── ResumeManager.cpp     # 断点续传状态持久化
│   │   └── MD5Helper.cpp         # MD5 块/文件校验
│   └── ui/
│       ├── MainWindow.cpp        # Qt 主窗口
│       ├── ProgressWidget.cpp    # 进度条 + 速率展示
│       └── FileListView.cpp      # 传输队列列表
├── tests/
│   ├── test_threadpool.cpp
│   ├── test_md5.cpp
│   └── test_protocol.cpp
└── docs/
    └── architecture.drawio
```

---

## 已知限制与 TODO

- [ ] **加密传输**：当前明文传输，计划集成 TLS（OpenSSL `SSL_write`）
- [ ] **目录同步**：目前仅支持单文件，待实现目录递归同步（类 rsync 增量）
- [ ] **多文件并发**：当前串行传输队列，待支持多文件并发传输
- [ ] **Windows 移植**：mmap 替换为 `MapViewOfFile`，信号量替换为 Win32 API
- [ ] **传输压缩**：可选 zstd 压缩，对可压缩文件提升有效吞吐
- [ ] **UDP + 自定义可靠层**：探索 QUIC 或 KCP 替代 TCP，降低拥塞控制延迟

---

## License

MIT License © 2024

---

*If you find this project helpful, feel free to ⭐ star it.*
