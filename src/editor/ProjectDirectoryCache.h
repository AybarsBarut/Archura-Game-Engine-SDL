#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace Archura {

// Only the worker touches the filesystem. Readers retain immutable snapshots
// across UI iteration, even when navigation or refresh schedules another scan.
class ProjectDirectoryCache {
public:
    using Clock = std::chrono::steady_clock;
    struct Entry {
        std::filesystem::path path;
        std::string name;
        bool isDirectory = false;
    };
    struct Snapshot {
        std::filesystem::path directory;
        std::vector<Entry> entries;
        std::string error;
        std::uint64_t revision = 0;
    };

    explicit ProjectDirectoryCache(
        std::chrono::milliseconds pollInterval = std::chrono::seconds(2));
    ~ProjectDirectoryCache();
    ProjectDirectoryCache(const ProjectDirectoryCache&) = delete;
    ProjectDirectoryCache& operator=(const ProjectDirectoryCache&) = delete;

    void Update(const std::filesystem::path& directory,
                Clock::time_point now = Clock::now());
    void Invalidate();
    std::shared_ptr<const Snapshot> GetSnapshot() const;
    std::uint64_t GetScanCount() const;

private:
    void Worker();
    const std::chrono::milliseconds m_PollInterval;
    mutable std::mutex m_Mutex;
    std::condition_variable m_Wake;
    std::filesystem::path m_Directory;
    std::shared_ptr<const Snapshot> m_Snapshot;
    Clock::time_point m_NextPoll{};
    std::uint64_t m_RequestRevision = 0;
    std::uint64_t m_ScanCount = 0;
    bool m_Invalidated = true;
    bool m_Pending = false;
    bool m_Stopping = false;
    std::thread m_Worker;
};

} // namespace Archura
