#include "ProjectSerializer.h"

#include "../ecs/Component.h"

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace Archura {

std::string ProjectSerializer::EscapeJson(std::string_view value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (const char c : value) {
        switch (c) {
            case '"': escaped += "\\\""; break;
            case '\\': escaped += "\\\\"; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default: escaped += c; break;
        }
    }
    return escaped;
}

std::string ProjectSerializer::ExtractJsonField(std::string_view line,
                                                std::string_view key) {
    const std::string quotedKey = "\"" + std::string(key) + "\"";
    const size_t keyPos = line.find(quotedKey);
    if (keyPos == std::string_view::npos) return {};
    size_t valuePos = line.find(':', keyPos + quotedKey.size());
    if (valuePos == std::string_view::npos) return {};
    ++valuePos;
    while (valuePos < line.size() &&
           (line[valuePos] == ' ' || line[valuePos] == '\t')) ++valuePos;
    if (valuePos >= line.size()) return {};

    if (line[valuePos] == '"') {
        ++valuePos;
        std::string value;
        while (valuePos < line.size()) {
            const char c = line[valuePos++];
            if (c == '"') break;
            if (c == '\\' && valuePos < line.size()) {
                const char escaped = line[valuePos++];
                switch (escaped) {
                    case 'n': value += '\n'; break;
                    case 'r': value += '\r'; break;
                    case 't': value += '\t'; break;
                    default: value += escaped; break;
                }
            } else {
                value += c;
            }
        }
        return value;
    }

    size_t end = valuePos;
    while (end < line.size() && line[end] != ',' && line[end] != '}' &&
           line[end] != ']') ++end;
    std::string value(line.substr(valuePos, end - valuePos));
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t'))
        value.pop_back();
    return value;
}

bool ProjectSerializer::SaveProject(const std::string& path,
                                    const ProjectConfig& config,
                                    Scene* scene) {
    if (!scene || path.empty()) return false;

    const std::filesystem::path projectPath(path);
    const auto parent = projectPath.parent_path();
    std::error_code ec;
    if (!parent.empty()) std::filesystem::create_directories(parent, ec);
    if (ec) return false;

    std::ofstream file(path, std::ios::out | std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to save project file: " << path << std::endl;
        return false;
    }

    file << "{\n";
    file << "  \"project\": {\n";
    file << "    \"name\": \"" << EscapeJson(config.name) << "\",\n";
    file << "    \"version\": \"" << EscapeJson(config.version) << "\",\n";
    file << "    \"start_scene\": \"" << EscapeJson(config.startScene) << "\"\n";
    file << "  },\n";
    file << "  \"entities\": [\n";

    const auto& entities = scene->GetEntities();
    for (size_t i = 0; i < entities.size(); ++i) {
        file << SerializeEntity(entities[i].get());
        if (i + 1 < entities.size()) file << ',';
        file << '\n';
    }

    file << "  ]\n}\n";
    file.flush();
    return file.good();
}

bool ProjectSerializer::LoadProject(const std::string& path,
                                    ProjectConfig& outConfig,
                                    Scene* scene) {
    if (!scene || path.empty()) return false;

    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file.is_open()) return false;
    file.seekg(0, std::ios::end);
    const std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    constexpr std::streamsize kMaxProjectSize = 32 * 1024 * 1024;
    if (size <= 0 || size > kMaxProjectSize) return false;
    std::string content(static_cast<size_t>(size), '\0');
    if (!file.read(content.data(), size)) return false;

    const size_t first = content.find_first_not_of(" \t\r\n");
    const size_t last = content.find_last_not_of(" \t\r\n");
    if (first == std::string::npos || content[first] != '{' ||
        last == std::string::npos || content[last] != '}' ||
        content.find("\"project\"") == std::string::npos ||
        content.find("\"entities\"") == std::string::npos) return false;

    struct LoadedEntity {
        std::string name;
        glm::vec3 position{0.0f};
        glm::vec3 rotation{0.0f};
        glm::vec3 scale{1.0f};
        bool hasTransform = false;
    };

    std::vector<LoadedEntity> loaded;
    LoadedEntity current;
    bool inEntities = false;
    bool entitiesClosed = false;
    bool entityOpen = false;
    bool transformOpen = false;
    bool parseError = false;

    auto parseArray = [](std::string_view line, std::string_view key,
                         glm::vec3& output) -> bool {
        const std::string quotedKey = "\"" + std::string(key) + "\"";
        const size_t keyPos = line.find(quotedKey);
        if (keyPos == std::string_view::npos) return true;
        const size_t open = line.find('[', keyPos);
        if (open == std::string_view::npos) return false;
        const char* cursor = line.data() + open + 1;
        const char* end = line.data() + line.size();
        char* next = nullptr;
        float values[3]{};
        for (float& value : values) {
            while (cursor < end && (*cursor == ' ' || *cursor == '\t' ||
                                    *cursor == ',')) ++cursor;
            value = std::strtof(cursor, &next);
            if (next == cursor || !std::isfinite(value)) return false;
            cursor = next;
        }
        output = glm::vec3(values[0], values[1], values[2]);
        return true;
    };

    std::string_view remaining(content);
    while (!remaining.empty()) {
        const size_t eol = remaining.find('\n');
        std::string_view line = eol == std::string_view::npos
                                    ? remaining
                                    : remaining.substr(0, eol);
        remaining = eol == std::string_view::npos
                        ? std::string_view{}
                        : remaining.substr(eol + 1);
        const size_t start = line.find_first_not_of(" \t\r");
        if (start == std::string_view::npos) continue;
        line = line.substr(start);
        const size_t end = line.find_last_not_of(" \t\r");
        if (end != std::string_view::npos) line = line.substr(0, end + 1);

        if (line.find("\"entities\"") != std::string_view::npos) {
            inEntities = true;
            continue;
        }
        if (!inEntities) {
            if (line.find("\"name\":") != std::string_view::npos)
                outConfig.name = ExtractJsonField(line, "name");
            if (line.find("\"version\":") != std::string_view::npos)
                outConfig.version = ExtractJsonField(line, "version");
            if (line.find("\"start_scene\":") != std::string_view::npos)
                outConfig.startScene = ExtractJsonField(line, "start_scene");
            continue;
        }

        if (line == "]") {
            entitiesClosed = true;
            continue;
        }
        if (entitiesClosed) continue;

        if (line == "{") {
            if (entityOpen || loaded.size() >= 100000) {
                parseError = true;
                break;
            }
            current = LoadedEntity{};
            entityOpen = true;
            continue;
        }
        if (line.find("\"transform\":") != std::string_view::npos) {
            transformOpen = true;
            current.hasTransform = true;
            continue;
        }
        if (line == "}" || line == "},") {
            if (transformOpen) {
                transformOpen = false;
                continue;
            }
            if (!entityOpen) {
                parseError = true;
                break;
            }
            if (current.name.empty()) {
                parseError = true;
                break;
            }
            loaded.push_back(current);
            entityOpen = false;
            continue;
        }
        if (!entityOpen) continue;
        if (line.find("\"name\":") != std::string_view::npos) {
            current.name = ExtractJsonField(line, "name");
            if (current.name.empty()) parseError = true;
        }
        if (!parseArray(line, "position", current.position) ||
            !parseArray(line, "rotation", current.rotation) ||
            !parseArray(line, "scale", current.scale)) parseError = true;
    }

    if (!entitiesClosed || entityOpen || transformOpen || parseError ||
        outConfig.name.empty())
        return false;

    std::vector<EntityID> toDelete;
    for (const auto& entity : scene->GetEntities())
        toDelete.push_back(entity->GetID());
    for (const EntityID id : toDelete) scene->DestroyEntity(id);

    std::vector<EntityHandle> created;
    try {
        for (const LoadedEntity& data : loaded) {
            Entity* entity = scene->CreateEntity(data.name);
            created.push_back(entity->GetHandle());
            if (data.hasTransform) {
                if (auto* transform = entity->GetComponent<Transform>()) {
                    transform->position = data.position;
                    transform->rotation = data.rotation;
                    transform->scale = data.scale;
                }
            }
        }
    } catch (...) {
        for (const EntityHandle handle : created) scene->DestroyEntity(handle);
        return false;
    }

    return true;
}

std::string ProjectSerializer::SerializeEntity(Entity* entity) {
    if (!entity) return "    {}";
    std::stringstream ss;
    ss << "    {\n";
    ss << "      \"id\": " << entity->GetID() << ",\n";
    ss << "      \"name\": \"" << EscapeJson(entity->GetName()) << "\",\n";

    auto* transform = entity->GetComponent<Transform>();
    if (transform) {
        ss << "      \"transform\": {\n";
        ss << "        \"position\": [" << transform->position.x << ", "
           << transform->position.y << ", " << transform->position.z << "],\n";
        ss << "        \"rotation\": [" << transform->rotation.x << ", "
           << transform->rotation.y << ", " << transform->rotation.z << "],\n";
        ss << "        \"scale\": [" << transform->scale.x << ", "
           << transform->scale.y << ", " << transform->scale.z << "]\n";
        ss << "      }\n";
    }
    ss << "    }";
    return ss.str();
}

} // namespace Archura
