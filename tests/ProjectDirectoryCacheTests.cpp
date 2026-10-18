#include "editor/ProjectDirectoryCache.h"

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

using Cache = Archura::ProjectDirectoryCache;
namespace {
void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
std::shared_ptr<const Cache::Snapshot> Wait(Cache& cache,
                                          std::uint64_t after = 0) {
    const auto timeout = Cache::Clock::now() + std::chrono::seconds(5);
    while (Cache::Clock::now() < timeout) {
        auto snapshot = cache.GetSnapshot();
        if (snapshot && snapshot->revision > after) return snapshot;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    throw std::runtime_error("directory scan timed out");
}
}

int main() {
    const auto root = std::filesystem::temp_directory_path() /
        ("archura_directory_cache_" + std::to_string(
            Cache::Clock::now().time_since_epoch().count()));
    try {
        std::filesystem::create_directories(root / "z-folder");
        std::ofstream(root / "b.txt") << "b";
        std::ofstream(root / "a.txt") << "a";
        {
            Cache cache(std::chrono::seconds(2));
            const auto now = Cache::Clock::now();
            cache.Update(root, now);
            auto initial = Wait(cache);
            Require(initial->error.empty(), "initial scan must succeed");
            Require(initial->entries.size() == 3, "all entries discovered");
            Require(initial->entries[0].isDirectory &&
                    initial->entries[0].name == "z-folder" &&
                    initial->entries[1].name == "a.txt" &&
                    initial->entries[2].name == "b.txt",
                    "directories first, then alphabetical names");
            const auto scans = cache.GetScanCount();
            for (int i = 0; i < 100; ++i) cache.Update(root, now);
            Require(cache.GetScanCount() == scans,
                    "idle frames must not schedule filesystem work");

            std::filesystem::remove(root / "a.txt");
            std::ofstream(root / "c.txt") << "c";
            cache.Invalidate();
            cache.Update(root, now);
            auto refreshed = Wait(cache, initial->revision);
            Require(refreshed->entries.size() == 3 &&
                    refreshed->entries[1].name == "b.txt" &&
                    refreshed->entries[2].name == "c.txt",
                    "explicit invalidation discovers create/delete");
            Require(initial->entries[1].name == "a.txt",
                    "retained snapshots remain immutable");

            std::ofstream(root / "d.txt") << "d";
            cache.Update(root, now + std::chrono::seconds(3));
            auto polled = Wait(cache, refreshed->revision);
            Require(polled->entries.size() == 4,
                    "periodic asynchronous polling finds external changes");

            cache.Update(root / "z-folder", now);
            auto child = Wait(cache, polled->revision);
            Require(child->directory == root / "z-folder" &&
                    child->entries.empty() && child->error.empty(),
                    "navigation replaces snapshot");
            cache.Update(root / "missing", now);
            auto missing = Wait(cache, child->revision);
            Require(!missing->error.empty() && missing->entries.empty(),
                    "missing directory reports error without stale entries");

            // Race navigation against in-flight scans. Only the newest request
            // can publish, and destruction joins a queued/running worker.
            cache.Update(root, now);
            cache.Update(root / "z-folder", now);
            auto latest = Wait(cache, missing->revision);
            Require(latest->directory == root / "z-folder",
                    "superseded scan cannot publish stale navigation");
            cache.Invalidate();
            cache.Update(root, now);
        }
        { Cache unused; } // Shutdown also wakes a worker that never scanned.
        std::filesystem::remove_all(root);
        std::cout << "Project directory cache contracts passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        std::error_code cleanup;
        std::filesystem::remove_all(root, cleanup);
        return 1;
    }
}
