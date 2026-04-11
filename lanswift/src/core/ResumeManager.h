#pragma once
#include <string>
#include <vector>
#include <cstdint>

// Persists a per-file bitmap of acknowledged chunks so that transfers can
// be resumed after a disconnect. Resume files are stored as:
//   <save_dir>/<filename>.lanswift_resume
class ResumeManager {
public:
    explicit ResumeManager(const std::string& saveDir);

    std::string resumePath(const std::string& fileId) const;

    // Load bitmap from disk. Returns all-false if no resume file exists.
    std::vector<bool> load(const std::string& fileId, uint32_t totalChunks);

    // Persist current bitmap to disk.
    void save(const std::string& fileId, const std::vector<bool>& bitmap);

    // Remove the resume file (call after successful full transfer).
    void remove(const std::string& fileId);

private:
    std::string saveDir_;
};
