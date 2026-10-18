#pragma once
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#pragma push_macro("APIENTRY")
#undef APIENTRY
#include <windows.h>
#pragma pop_macro("APIENTRY")
#endif

namespace Archura::Serialization {
// Write and validate a sibling file before touching the destination. Retain the
// preceding successful save as .bak; failures leave the destination intact.
inline bool AtomicWrite(const std::string& path, const std::string& content) {
    if (path.empty() || path.find('\0') != std::string::npos) return false;
    const std::filesystem::path target(path);
    std::error_code ec;
    if (!target.parent_path().empty()) std::filesystem::create_directories(target.parent_path(), ec);
    if (ec) return false;
    static std::atomic<unsigned long long> serial{0};
    auto temporary = target;
    temporary += ".tmp." + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "." + std::to_string(serial++);
    struct Cleanup { std::filesystem::path path; ~Cleanup() { std::error_code ignored; std::filesystem::remove(path, ignored); } } cleanup{temporary};
    {
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        if (!stream || !stream.write(content.data(), static_cast<std::streamsize>(content.size()))) return false;
        stream.flush(); if (!stream) return false;
        stream.close(); if (stream.fail()) return false;
    }
    if (std::filesystem::exists(target, ec)) {
        auto backup = target; backup += ".bak";
        if (!std::filesystem::copy_file(target, backup, std::filesystem::copy_options::overwrite_existing, ec) || ec) return false;
    } else if (ec) return false;
#ifdef _WIN32
    return MoveFileExW(temporary.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    std::filesystem::rename(temporary, target, ec);
    return !ec;
#endif
}
} // namespace Archura::Serialization
