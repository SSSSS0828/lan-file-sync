#pragma once
#include <string>
#include <cstdint>
#include <cstddef>

class MD5Helper {
public:
    // Compute MD5 of a memory buffer, return 32-char hex string
    static std::string compute(const void* data, size_t len);

    // Compute MD5 of a file on disk (reads in chunks, no full-file mmap)
    static std::string computeFile(const std::string& filepath);
};
