#include <cstdint>
#include "ResumeManager.h"
#include <fstream>
#include <stdexcept>
#include <cstring>

ResumeManager::ResumeManager(const std::string& saveDir)
    : saveDir_(saveDir)
{}

std::string ResumeManager::resumePath(const std::string& fileId) const
{
    return saveDir_ + "/" + fileId + ".lanswift_resume";
}

std::vector<bool> ResumeManager::load(const std::string& fileId,
                                       uint32_t totalChunks)
{
    std::vector<bool> bitmap(totalChunks, false);
    std::ifstream ifs(resumePath(fileId), std::ios::binary);
    if (!ifs) return bitmap;   // no resume file → start fresh

    uint32_t saved_total = 0;
    ifs.read(reinterpret_cast<char*>(&saved_total), 4);
    if (saved_total != totalChunks) return bitmap;  // mismatch → restart

    for (uint32_t i = 0; i < totalChunks; ++i) {
        uint8_t v = 0;
        ifs.read(reinterpret_cast<char*>(&v), 1);
        bitmap[i] = (v != 0);
    }
    return bitmap;
}

void ResumeManager::save(const std::string& fileId,
                          const std::vector<bool>& bitmap)
{
    std::ofstream ofs(resumePath(fileId), std::ios::binary | std::ios::trunc);
    if (!ofs) return;
    uint32_t total = static_cast<uint32_t>(bitmap.size());
    ofs.write(reinterpret_cast<const char*>(&total), 4);
    for (bool b : bitmap) {
        uint8_t v = b ? 1 : 0;
        ofs.write(reinterpret_cast<const char*>(&v), 1);
    }
}

void ResumeManager::remove(const std::string& fileId)
{
    ::remove(resumePath(fileId).c_str());
}
