#pragma once

#include <string>
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

    private:
        static std::string EscapeJson(std::string_view value);
    };

} // namespace Archura
