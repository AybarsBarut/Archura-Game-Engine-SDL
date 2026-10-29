#include "RuntimeDiscovery.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

int main() {
    const auto scratch = fs::temp_directory_path() /
        ("archura-launcher-test-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    struct Cleanup {
        fs::path path;
        ~Cleanup() { std::error_code error; fs::remove_all(path, error); }
    } cleanup{scratch};
    auto touch = [](const fs::path& path) {
        fs::create_directories(path.parent_path());
        std::ofstream(path).put('x');
    };
    auto require = [](bool condition, const char* message) {
        if (!condition) throw std::runtime_error(message);
    };
    try {
        const auto root = scratch / "checkout";
        const auto launcher = root / "build/windows-x64/bin/Debug";
        fs::create_directories(launcher);
        touch(root / "CMakeLists.txt");
        touch(root / "CMakePresets.json");
        touch(root / "src/launcher/Launcher.cpp");
        require(Archura::Launcher::FindRepositoryRoot(launcher) == root,
                "Find root from deeply nested output");
        require(Archura::Launcher::FindRuntime(root, launcher).empty(),
                "Missing runtime stays missing");
        const auto legacy = root / "build/bin/Release/ArchuraEngine.exe";
        touch(legacy);
        require(Archura::Launcher::FindRuntime(root, launcher) == legacy,
                "Legacy layout fallback");
        const auto release = root / "build/windows-x64/bin/Release/ArchuraEngine.exe";
        const auto debug = launcher / "ArchuraEngine.exe";
        const auto validation = root / "build/loop-validation/bin/Debug/ArchuraEngine.exe";
        touch(release); touch(debug); touch(validation);
        const auto base = fs::file_time_type::clock::now();
        fs::last_write_time(release, base - std::chrono::hours(3));
        fs::last_write_time(debug, base - std::chrono::hours(2));
        fs::last_write_time(validation, base - std::chrono::hours(1));
        fs::last_write_time(legacy, base);
        require(Archura::Launcher::FindRuntime(root, launcher) == validation,
                "Newest preset beats stale sibling and newer legacy copy");
        fs::last_write_time(release, base);
        fs::last_write_time(debug, base);
        fs::last_write_time(validation, base);
        require(Archura::Launcher::FindRuntime(root, launcher) == release,
                "Equal timestamps have stable preference");
        require(Archura::Launcher::FindRuntime({}, launcher) == debug,
                "Packaged launcher uses sibling without checkout");
        require(Archura::Launcher::FindRepositoryRoot(scratch).empty(),
                "Unrelated directory is not a checkout");
        touch(scratch / "CMakeLists.txt");
        touch(scratch / "CMakePresets.json");
        require(Archura::Launcher::FindRepositoryRoot(scratch).empty(),
                "Unrelated CMake project is not the engine checkout");
        std::cout << "Runtime discovery: 8 cases passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
