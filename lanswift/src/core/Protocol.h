#pragma once
#include <cstdint>
#include <cstring>
#include <string>

// Magic number: "LSWF"
static const uint32_t PROTO_MAGIC   = 0x4C535746;
static const uint8_t  PROTO_VERSION = 0x01;

// Frame types
enum FrameType : uint8_t {
    FT_FILE_INFO     = 0x01,
    FT_CHUNK_DATA    = 0x02,
    FT_CHUNK_ACK     = 0x03,
    FT_CHUNK_NACK    = 0x04,
    FT_RESUME_REQ    = 0x05,
    FT_TRANSFER_DONE = 0x06,
    FT_ERROR         = 0xFF,
};

#pragma pack(push, 1)

struct FrameHeader {
    uint32_t magic;
    uint8_t  version;
    uint8_t  type;
    uint8_t  flags;
    uint8_t  reserved;
};

struct FileInfoPayload {
    uint64_t file_size;
    uint32_t total_chunks;
    uint32_t chunk_size;
    char     file_md5[33];
    char     filename[256];
};

struct ChunkDataHeader {
    uint32_t chunk_index;
    uint32_t chunk_size;
    char     chunk_md5[33];
};

struct ChunkAckPayload {
    uint32_t chunk_index;
};

struct ResumeReqHeader {
    uint32_t total_chunks;
    // followed by ceil(total_chunks/8) bytes bitmap
};

struct TransferDonePayload {
    char file_md5[33];
};

#pragma pack(pop)

inline std::string makeFrame(uint8_t type, const void* payload, uint32_t payload_len)
{
    FrameHeader hdr;
    hdr.magic    = PROTO_MAGIC;
    hdr.version  = PROTO_VERSION;
    hdr.type     = type;
    hdr.flags    = 0;
    hdr.reserved = 0;

    std::string frame;
    frame.resize(sizeof(FrameHeader) + payload_len);
    memcpy(&frame[0], &hdr, sizeof(FrameHeader));
    if (payload && payload_len)
        memcpy(&frame[sizeof(FrameHeader)], payload, payload_len);
    return frame;
}
