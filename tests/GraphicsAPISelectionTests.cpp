#include "rendering/GraphicsAPI.h"

#include <cassert>
#include <filesystem>
#include <string>
#include <vector>

namespace {

Archura::GraphicsLaunchOptions Resolve(
    const std::vector<std::string>& arguments,
    const std::filesystem::path& preferencePath) {
    std::vector<char*> argv;
    argv.reserve(arguments.size());
    for (const std::string& argument : arguments) {
        argv.push_back(const_cast<char*>(argument.c_str()));
    }

    Archura::GraphicsLaunchOptions options;
    std::string error;
    const bool resolved = Archura::ResolveGraphicsLaunchOptions(
        static_cast<int>(argv.size()), argv.data(), preferencePath, options,
        error);
    assert(resolved);
    assert(error.empty());
    return options;
}

} // namespace

int main() {
    using Archura::GraphicsAPI;

    assert(Archura::ParseGraphicsAPI("OpenGL") == GraphicsAPI::OpenGL);
    assert(Archura::ParseGraphicsAPI(" vk ") == GraphicsAPI::Vulkan);
    assert(Archura::ParseGraphicsAPI("AUTO") == GraphicsAPI::Auto);
    assert(!Archura::ParseGraphicsAPI("directx"));

    const auto preferencePath =
        std::filesystem::temp_directory_path() /
        "archura_graphics_api_selection_test.cfg";
    std::error_code errorCode;
    std::filesystem::remove(preferencePath, errorCode);

    assert(Archura::SaveGraphicsPreference(preferencePath,
                                           GraphicsAPI::Vulkan));
    auto options = Resolve({"ArchuraEngine"}, preferencePath);
    assert(options.requestedAPI == GraphicsAPI::Vulkan);
    assert(!options.explicitlySelected);

    options = Resolve({"ArchuraEngine", "--graphics=opengl"},
                      preferencePath);
    assert(options.requestedAPI == GraphicsAPI::OpenGL);
    assert(options.explicitlySelected);

    options = Resolve({"ArchuraEngine", "--graphics", "auto",
                       "--no-graphics-fallback"},
                      preferencePath);
    assert(options.requestedAPI == GraphicsAPI::Auto);
    assert(!options.allowFallback);

    std::filesystem::remove(preferencePath, errorCode);
    return 0;
}
