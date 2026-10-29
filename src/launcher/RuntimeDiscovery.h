#pragma once

#include <filesystem>
#include <initializer_list>
#include <system_error>

namespace Archura::Launcher {
namespace fs = std::filesystem;

inline fs::path FindRepositoryRoot(fs::path directory) {
    for (;;) {
        std::error_code error;
        if (fs::is_regular_file(directory / "CMakePresets.json", error) &&
            fs::is_regular_file(directory / "CMakeLists.txt", error) &&
            fs::is_regular_file(directory / "src/launcher/Launcher.cpp", error))
            return directory;
        const auto parent = directory.parent_path();
        if (parent.empty() || parent == directory) return {};
        directory = parent;
    }
}

// Preset outputs form the preferred tier. Pick their newest executable; order
// below breaks timestamp ties so repeated launches always make the same choice.
inline fs::path FindRuntime(const fs::path& root, const fs::path& launcherDirectory) {
    const char* presetOutputs[] = {
        "build/windows-x64/bin/Release/ArchuraEngine.exe",
        "build/windows-x64/bin/Debug/ArchuraEngine.exe",
        "build/loop-validation/bin/Release/ArchuraEngine.exe",
        "build/loop-validation/bin/Debug/ArchuraEngine.exe"
    };
    fs::path newest;
    fs::file_time_type newestTime{};
    if (!root.empty()) {
        for (const auto* relative : presetOutputs) {
            const auto candidate = root / relative;
            std::error_code error;
            if (!fs::is_regular_file(candidate, error)) continue;
            const auto time = fs::last_write_time(candidate, error);
            if (!error && (newest.empty() || time > newestTime)) {
                newest = candidate;
                newestTime = time;
            }
        }
        if (!newest.empty()) return newest;
    }
    // Packaged launchers use their sibling runtime. Historical build layouts
    // remain supported only when no current preset output exists.
    std::error_code error;
    const auto sibling = launcherDirectory / "ArchuraEngine.exe";
    if (fs::is_regular_file(sibling, error)) return sibling;
    if (!root.empty()) {
        for (const auto* relative : {
                 "build/bin/Release/ArchuraEngine.exe",
                 "build/bin/Debug/ArchuraEngine.exe",
                 "bin/Release/ArchuraEngine.exe", "bin/Debug/ArchuraEngine.exe",
                 "ArchuraEngine.exe"}) {
            const auto candidate = root / relative;
            error.clear();
            if (fs::is_regular_file(candidate, error)) return candidate;
        }
    }
    return {};
}
} // namespace Archura::Launcher
