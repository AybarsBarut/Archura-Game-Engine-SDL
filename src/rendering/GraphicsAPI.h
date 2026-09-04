#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace Archura {

enum class GraphicsAPI {
    Auto,
    OpenGL,
    Vulkan
};

const char* ToString(GraphicsAPI api) noexcept;
std::optional<GraphicsAPI> ParseGraphicsAPI(std::string_view value);
bool IsGraphicsAPICompiled(GraphicsAPI api) noexcept;

struct GraphicsLaunchOptions {
    GraphicsAPI requestedAPI = GraphicsAPI::Auto;
    bool allowFallback = true;
    bool explicitlySelected = false;
};

// Resolution order: command line > ARCHURA_GRAPHICS_API > preference file > Auto.
// Recognized command-line forms:
//   --graphics=auto|opengl|vulkan
//   --graphics auto|opengl|vulkan
//   --no-graphics-fallback
bool ResolveGraphicsLaunchOptions(
    int argc,
    char** argv,
    const std::filesystem::path& preferencePath,
    GraphicsLaunchOptions& options,
    std::string& error);

bool LoadGraphicsPreference(const std::filesystem::path& path,
                            GraphicsAPI& api);
bool SaveGraphicsPreference(const std::filesystem::path& path,
                            GraphicsAPI api);

} // namespace Archura
