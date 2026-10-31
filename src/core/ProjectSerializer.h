#pragma once

#include <string>
#include <filesystem>
#include <string_view>
#include <vector>
#include "../ecs/Entity.h"

namespace Archura {

    struct ProjectConfig {
        std::string name;
        std::string version;
        std::string startScene;
    };

    class ProjectSerializer {
    public:
        static bool SaveProject(const std::string& path, const ProjectConfig& config, Scene* scene);
        static bool LoadProject(const std::string& path, ProjectConfig& outConfig, Scene* scene);
        static bool SaveProject(const std::string& path, const ProjectConfig& config, Scene* scene,
                                const std::filesystem::path& contentRoot);
        static bool LoadProject(const std::string& path, ProjectConfig& outConfig, Scene* scene,
                                const std::filesystem::path& contentRoot);
        static bool SaveProjectPath(const std::filesystem::path& path, const ProjectConfig& config, Scene* scene,
                                    const std::filesystem::path& contentRoot = {});
        static bool LoadProjectPath(const std::filesystem::path& path, ProjectConfig& outConfig, Scene* scene,
                                    const std::filesystem::path& contentRoot = {});

    private:
        static std::string EscapeJson(std::string_view value);
    };

} // namespace Archura
