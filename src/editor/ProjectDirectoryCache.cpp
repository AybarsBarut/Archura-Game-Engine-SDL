#include "ProjectDirectoryCache.h"

#include <algorithm>
#include <exception>

namespace Archura {

ProjectDirectoryCache::ProjectDirectoryCache(std::chrono::milliseconds interval)
    : m_PollInterval(std::max(interval, std::chrono::milliseconds(1))),
      m_Worker(&ProjectDirectoryCache::Worker, this) {}

ProjectDirectoryCache::~ProjectDirectoryCache() {
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_Stopping = true;
    }
    m_Wake.notify_one();
    m_Worker.join();
}

void ProjectDirectoryCache::Update(const std::filesystem::path& directory,
                                   Clock::time_point now) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    if (directory != m_Directory) {
        m_Directory = directory;
        m_Invalidated = true;
        m_Snapshot.reset();
    }
    if (!m_Invalidated && now < m_NextPoll) return;
    m_Invalidated = false;
    m_NextPoll = now + m_PollInterval;
    ++m_RequestRevision;
    m_Pending = true;
    m_Wake.notify_one();
}

void ProjectDirectoryCache::Invalidate() {
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_Invalidated = true;
}

std::shared_ptr<const ProjectDirectoryCache::Snapshot>
ProjectDirectoryCache::GetSnapshot() const {
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_Snapshot;
}

std::uint64_t ProjectDirectoryCache::GetScanCount() const {
    std::lock_guard<std::mutex> lock(m_Mutex);
    return m_ScanCount;
}

void ProjectDirectoryCache::Worker() {
    for (;;) {
        std::filesystem::path directory;
        std::uint64_t revision;
        {
            std::unique_lock<std::mutex> lock(m_Mutex);
            m_Wake.wait(lock, [this] { return m_Stopping || m_Pending; });
            if (m_Stopping) return;
            directory = m_Directory;
            revision = m_RequestRevision;
            m_Pending = false;
        }

        auto result = std::make_shared<Snapshot>();
        result->directory = directory;
        result->revision = revision;
        try {
            std::error_code error;
            std::filesystem::directory_iterator current(directory, error), end;
            while (!error && current != end) {
                Entry entry;
                entry.path = current->path();
                entry.name = entry.path.filename().string();
                entry.isDirectory = current->is_directory(error);
                if (error) break;
                result->entries.push_back(std::move(entry));
                current.increment(error);
            }
            if (error) {
                result->error = directory.string() + ": " + error.message();
                result->entries.clear();
            } else {
                std::sort(result->entries.begin(), result->entries.end(),
                    [](const Entry& a, const Entry& b) {
                        if (a.isDirectory != b.isDirectory) return a.isDirectory;
                        return a.name < b.name;
                    });
            }
        } catch (const std::exception& error) {
            result->entries.clear();
            result->error = error.what();
        }

        {
            std::lock_guard<std::mutex> lock(m_Mutex);
            ++m_ScanCount;
            // A navigation/refresh request made during I/O supersedes this scan.
            if (revision == m_RequestRevision) m_Snapshot = std::move(result);
        }
    }
}

} // namespace Archura
