#include "GraphicsAPI.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>

namespace Archura {
namespace {

std::string Normalize(std::string_view value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) {
        return {};
    }

    const auto last = value.find_last_not_of(" \t\r\n");
    std::string normalized(value.substr(first, last - first + 1));
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });
    return normalized;
}

std::optional<GraphicsAPI> ParsePreferenceLine(std::string_view line) {
    const auto comment = line.find_first_of("#;");
    if (comment != std::string_view::npos) {
        line = line.substr(0, comment);
    }

    const auto separator = line.find('=');
    if (separator != std::string_view::npos) {
        if (Normalize(line.substr(0, separator)) != "api") {
            return std::nullopt;
        }
        line = line.substr(separator + 1);
    }
    return ParseGraphicsAPI(line);
}

} // namespace

const char* ToString(GraphicsAPI api) noexcept {
    switch (api) {
        case GraphicsAPI::Auto:
            return "auto";
        case GraphicsAPI::OpenGL:
            return "opengl";
        case GraphicsAPI::Vulkan:
            return "vulkan";
    }
    return "auto";
}

std::optional<GraphicsAPI> ParseGraphicsAPI(std::string_view value) {
    const std::string normalized = Normalize(value);
    if (normalized == "auto") {
        return GraphicsAPI::Auto;
    }
    if (normalized == "opengl" || normalized == "gl") {
        return GraphicsAPI::OpenGL;
    }
    if (normalized == "vulkan" || normalized == "vk") {
        return GraphicsAPI::Vulkan;
    }
    return std::nullopt;
}

bool IsGraphicsAPICompiled(GraphicsAPI api) noexcept {
    switch (api) {
        case GraphicsAPI::Auto:
        case GraphicsAPI::OpenGL:
            return true;
        case GraphicsAPI::Vulkan:
#ifdef ARCHURA_HAS_VULKAN_BACKEND
            return true;
#else
            return false;
#endif
    }
    return false;
}

bool LoadGraphicsPreference(const std::filesystem::path& path,
                            GraphicsAPI& api) {
    std::ifstream input(path);
    if (!input.is_open()) {
        return false;
    }

    std::string line;
    while (std::getline(input, line)) {
        if (const auto parsed = ParsePreferenceLine(line)) {
            api = *parsed;
            return true;
        }
    }
    return false;
}

bool SaveGraphicsPreference(const std::filesystem::path& path,
                            GraphicsAPI api) {
    std::error_code errorCode;
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path(), errorCode);
        if (errorCode) {
            return false;
        }
    }

    std::ofstream output(path, std::ios::trunc);
    if (!output.is_open()) {
        return false;
    }
    output << "# Archura graphics API preference\n";
    output << "api=" << ToString(api) << '\n';
    return output.good();
}

bool ResolveGraphicsLaunchOptions(
    int argc,
    char** argv,
    const std::filesystem::path& preferencePath,
    GraphicsLaunchOptions& options,
    std::string& error) {
    options = {};
    error.clear();

    GraphicsAPI filePreference = GraphicsAPI::Auto;
    if (LoadGraphicsPreference(preferencePath, filePreference)) {
        options.requestedAPI = filePreference;
    }

    if (const char* environmentValue = std::getenv("ARCHURA_GRAPHICS_API")) {
        const auto parsed = ParseGraphicsAPI(environmentValue);
        if (!parsed) {
            error = "ARCHURA_GRAPHICS_API must be auto, opengl, or vulkan";
            return false;
        }
        options.requestedAPI = *parsed;
        options.explicitlySelected = true;
    }

    for (int index = 1; index < argc; ++index) {
        const std::string_view argument(argv[index] ? argv[index] : "");
        std::string_view value;

        if (argument == "--no-graphics-fallback") {
            options.allowFallback = false;
            continue;
        }

        if (argument.rfind("--graphics=", 0) == 0) {
            value = argument.substr(std::string_view("--graphics=").size());
        } else if (argument == "--graphics") {
            if (index + 1 >= argc || !argv[index + 1]) {
                error = "--graphics requires auto, opengl, or vulkan";
                return false;
            }
            value = argv[++index];
        } else {
            continue;
        }

        const auto parsed = ParseGraphicsAPI(value);
        if (!parsed) {
            error = "Unknown graphics API '" + std::string(value) +
                    "' (expected auto, opengl, or vulkan)";
            return false;
        }
        options.requestedAPI = *parsed;
        options.explicitlySelected = true;
    }

    return true;
}

} // namespace Archura
