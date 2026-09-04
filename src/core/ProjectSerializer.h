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
        // Helper to serialize an entity to a JSON-like string
        static std::string SerializeEntity(Entity* entity);
        static std::string EscapeJson(std::string_view value);
        static std::string ExtractJsonField(std::string_view line,
                                             std::string_view key);
    };

} // namespace Archura
