#include "GameSaveManager.h"
#include "../ecs/Entity.h"
#include "../ecs/Component.h"
#include "../rendering/Mesh.h"
#include "../rendering/Texture.h"

#include <fstream>
#include <sstream>
#include <filesystem>
#include <iostream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <algorithm>
#include <system_error>
#include <cmath>
#include <cstdlib>

namespace Archura {

static constexpr const char* s_SystemEntities[] = {
    "Player", "Skybox", "Sun", "AmbientLight",
    "Wall_North", "Wall_South", "Wall_East", "Wall_West", "Floor"
};

static bool IsSystemEntity(std::string_view name) {
    for (const char* sysName : s_SystemEntities) {
        if (name == sysName) return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Ctor
// ---------------------------------------------------------------------------
GameSaveManager::GameSaveManager() {
    std::error_code ec;
    std::filesystem::create_directories("saves", ec);
    RefreshProjects();
}

// ---------------------------------------------------------------------------
// Yardımcılar
// ---------------------------------------------------------------------------
std::string GameSaveManager::GetTimestamp() const {
    auto now  = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm_info{};
#ifdef _WIN32
    localtime_s(&tm_info, &time);
#else
    localtime_r(&time, &tm_info);
#endif
    std::ostringstream ss;
    ss << std::put_time(&tm_info, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

std::string GameSaveManager::EscapeJson(std::string_view s) {
    std::string out;
    out.reserve(s.size() + s.size() / 10);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            default:   out += c;
        }
    }
    return out;
}

std::string GameSaveManager::ExtractJsonField(std::string_view json, std::string_view key) {
    size_t pos = 0;
    while (true) {
        pos = json.find(key, pos);
        if (pos == std::string_view::npos) return "";

        if (pos > 0 && json[pos-1] == '"' && (pos + key.size() < json.size() && json[pos + key.size()] == '"')) {
            break;
        }
        pos += key.size();
    }
    
    pos += key.size() + 1;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == ':')) ++pos;
    if (pos >= json.size()) return "";

    if (json[pos] == '"') {
        ++pos;
        std::string val;
        while (pos < json.size() && json[pos] != '"') {
            if (json[pos] == '\\' && pos + 1 < json.size()) {
                ++pos;
                switch (json[pos]) {
                    case '"':  val += '"';  break;
                    case '\\': val += '\\'; break;
                    case 'n':  val += '\n'; break;
                    default:   val += json[pos];
                }
            } else {
                val += json[pos];
            }
            ++pos;
        }
        return val;
    } else {
        size_t end = pos;
        while (end < json.size() && json[end] != ',' && json[end] != '\n' &&
               json[end] != '\r' && json[end] != '}' && json[end] != ']') {
            ++end;
        }
        std::string_view val = json.substr(pos, end - pos);
        while (!val.empty() && (val.front() == ' ' || val.front() == '\t')) val.remove_prefix(1);
        while (!val.empty() && (val.back()  == ' ' || val.back()  == '\t')) val.remove_suffix(1);
        return std::string(val);
    }
}

std::string GameSaveManager::MakeSafeFileName(std::string_view name) {
    std::string result;
    result.reserve(name.size());
    for (char c : name) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-' || c == ' ') {
            result += (c == ' ') ? '_' : c;
        }
    }
    if (result.empty()) result = "Unnamed";
    if (result.size() > 64) result.resize(64);
    return result;
}

// ---------------------------------------------------------------------------
// RefreshProjects – saves/*.scene dosyalarını tara
// ---------------------------------------------------------------------------
void GameSaveManager::RefreshProjects() {
    m_Projects.clear();
    std::error_code ec;
    if (!std::filesystem::exists("saves", ec)) return;

    for (const auto& entry : std::filesystem::directory_iterator("saves", ec)) {
        if (!entry.is_regular_file(ec)) continue;
        if (entry.path().extension() != ".scene") continue;

        std::ifstream f(entry.path(), std::ios::in | std::ios::binary);
        if (!f.is_open()) continue;

        f.seekg(0, std::ios::end);
        std::streamsize size = f.tellg();
        f.seekg(0, std::ios::beg);

        if (size <= 0) continue;

        std::string content;
        content.resize(static_cast<size_t>(size));
        if (!f.read(content.data(), size)) continue;

        ProjectSave proj;
        proj.filePath   = entry.path().string();
        std::string_view contentView(content);
        proj.name       = ExtractJsonField(contentView, "project_name");
        proj.timestamp  = ExtractJsonField(contentView, "timestamp");
        proj.valid      = true;

        size_t cnt = 0, p = 0;
        std::string_view nameKey = "\"name\":";
        while ((p = contentView.find(nameKey, p)) != std::string_view::npos) { 
            ++cnt; 
            p += nameKey.length(); 
        }
        proj.entityCount = static_cast<int>(cnt);

        if (proj.name.empty()) proj.name = entry.path().stem().string();
        m_Projects.push_back(proj);
    }

    std::sort(m_Projects.begin(), m_Projects.end(),
        [](const ProjectSave& a, const ProjectSave& b) {
            return a.timestamp > b.timestamp;
        });
}

// ---------------------------------------------------------------------------
// SaveProject – sahneyi proje dosyasına yaz
// ---------------------------------------------------------------------------
bool GameSaveManager::SaveProject(const std::string& projectName, Scene* scene) {
    if (!scene) return false;

    std::error_code ec;
    std::filesystem::create_directories("saves", ec);

    const std::string& actualName = projectName.empty() ? "Unnamed" : projectName;
    std::string safeName = MakeSafeFileName(actualName);
    std::string path     = "saves/" + safeName + ".scene";
    std::string ts       = GetTimestamp();

    std::ofstream file(path, std::ios::out | std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[SaveManager] Dosya olusturulamadi: " << path << std::endl;
        return false;
    }

    file << "{\n";
    file << "  \"project_name\": \"" << EscapeJson(actualName) << "\",\n";
    file << "  \"timestamp\": \"" << EscapeJson(ts) << "\",\n";
    file << "  \"engine_version\": \"1.0\",\n";
    file << "  \"entities\": [\n";

    const auto& entities = scene->GetEntities();
    bool first = true;

    for (const auto& ePtr : entities) {
        Entity* e = ePtr.get();
        if (IsSystemEntity(e->GetName())) continue;

        if (!first) file << ",\n";
        first = false;

        auto* t  = e->GetComponent<Transform>();
        auto* mr = e->GetComponent<MeshRenderer>();
        auto* bc = e->GetComponent<BoxCollider>();
        auto* lc = e->GetComponent<LightComponent>();

        file << "    {\n";
        file << "      \"name\": \""  << EscapeJson(e->GetName()) << "\",\n";

        std::string meshType = bc && bc->shape == BoxCollider::Shape::Ramp
                                   ? "ramp"
                                   : "cube";
        std::string modelPath;
        if (mr) {
            meshType  = "cube";
        }
        file << "      \"mesh_type\": \"" << meshType << "\",\n";
        file << "      \"has_mesh_renderer\": " << (mr ? "true" : "false") << ",\n";

        if (!modelPath.empty())
            file << "      \"model_path\": \"" << EscapeJson(modelPath) << "\",\n";

        if (t) {
            file << "      \"position\": [" << t->position.x << "," << t->position.y << "," << t->position.z << "],\n";
            file << "      \"rotation\": [" << t->rotation.x << "," << t->rotation.y << "," << t->rotation.z << "],\n";
            file << "      \"scale\":    [" << t->scale.x    << "," << t->scale.y    << "," << t->scale.z    << "],\n";
        }

        if (mr) {
            file << "      \"color\": [" << mr->color.r << "," << mr->color.g << "," << mr->color.b << "],\n";
            std::string texPath = mr->texture ? mr->texture->GetPath() : "";
            file << "      \"texture\": \"" << EscapeJson(texPath) << "\",\n";
        }

        if (bc) {
            file << "      \"collider\": [" << bc->size.x << "," << bc->size.y << "," << bc->size.z << "],\n";
            file << "      \"collider_center\": [" << bc->center.x << "," << bc->center.y << "," << bc->center.z << "],\n";
            file << "      \"collider_shape\": " << static_cast<int>(bc->shape) << ",\n";
            file << "      \"collider_trigger\": " << (bc->isTrigger ? "true" : "false") << ",\n";
        }

        if (lc) {
            file << "      \"light_type\": " << static_cast<int>(lc->type) << ",\n";
            file << "      \"light_color\": [" << lc->color.r << "," << lc->color.g << "," << lc->color.b << "],\n";
            file << "      \"light_intensity\": " << lc->intensity << ",\n";
            file << "      \"light_range\": " << lc->range << ",\n";
        }

        file << "      \"__end\": true\n";
        file << "    }";
    }

    file << "\n  ]\n}\n";
    file.close();

    std::cout << "[SaveManager] Proje kaydedildi: " << path << std::endl;
    RefreshProjects();
    return true;
}

// ---------------------------------------------------------------------------
// LoadProject – sahneye proje entity'lerini yukle
// ---------------------------------------------------------------------------
bool GameSaveManager::LoadProject(const std::string& filePath, Scene* scene) {
    if (!scene) return false;
    std::error_code ec;
    if (!std::filesystem::exists(filePath, ec)) {
        std::cerr << "[SaveManager] Dosya bulunamadi: " << filePath << std::endl;
        return false;
    }

    std::ifstream file(filePath, std::ios::in | std::ios::binary);
    if (!file.is_open()) return false;

    file.seekg(0, std::ios::end);
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    constexpr std::streamsize kMaxProjectSize = 32 * 1024 * 1024;
    if (size <= 0 || size > kMaxProjectSize) {
        std::cerr << "[SaveManager] Gecersiz proje boyutu\n";
        return false;
    }

    std::string content;
    content.resize(static_cast<size_t>(size));
    if (!file.read(content.data(), size)) return false;
    file.close();

    const size_t first = content.find_first_not_of(" \t\r\n");
    const size_t last = content.find_last_not_of(" \t\r\n");
    if (first == std::string::npos || content[first] != '{' ||
        last == std::string::npos || content[last] != '}' ||
        content.find("\"entities\"") == std::string::npos) {
        std::cerr << "[SaveManager] Gecersiz proje formati\n";
        return false;
    }

    struct LoadedEntity {
        std::string name;
        std::string meshType = "cube";
        std::string texturePath;
        glm::vec3 position{0.0f};
        glm::vec3 rotation{0.0f};
        glm::vec3 scale{1.0f};
        glm::vec3 color{1.0f};
        glm::vec3 colliderSize{1.0f};
        glm::vec3 colliderCenter{0.0f};
        glm::vec3 lightColor{1.0f};
        int colliderShape = 0;
        int lightType = 0;
        float lightIntensity = 1.0f;
        float lightRange = 20.0f;
        bool hasMeshRenderer = true; // backwards-compatible with old saves
        bool hasCollider = false;
        bool colliderTrigger = false;
        bool hasLight = false;
        bool hasColliderShape = false;
    };

    std::vector<LoadedEntity> loaded;
    loaded.reserve(64);
    LoadedEntity current;
    bool inEntities = false;
    bool entitiesClosed = false;
    bool currentOpen = false;
    bool parseError = false;

    auto parseArr3 = [&](std::string_view line, std::string_view key,
                         glm::vec3& value) -> bool {
        const std::string quotedKey = "\"" + std::string(key) + "\"";
        const size_t kp = line.find(quotedKey);
        if (kp == std::string_view::npos) return true;
        const size_t open = line.find('[', kp);
        if (open == std::string_view::npos) return false;
        const char* begin = line.data() + open + 1;
        const char* end = line.data() + line.size();
        char* next = nullptr;
        float values[3]{};
        const char* cursor = begin;
        for (float& component : values) {
            while (cursor < end && (*cursor == ' ' || *cursor == '\t' || *cursor == ',')) ++cursor;
            component = std::strtof(cursor, &next);
            if (next == cursor || !std::isfinite(component)) return false;
            cursor = next;
        }
        value = glm::vec3(values[0], values[1], values[2]);
        return true;
    };

    auto parseFloat = [&](std::string_view line, std::string_view key,
                          float& target) -> bool {
        const std::string value = ExtractJsonField(line, key);
        if (value.empty()) return false;
        try {
            size_t consumed = 0;
            const float parsed = std::stof(value, &consumed);
            if (consumed != value.size() || !std::isfinite(parsed)) return false;
            target = parsed;
            return true;
        } catch (...) {
            return false;
        }
    };

    auto parseInt = [&](std::string_view line, std::string_view key,
                        int& target) -> bool {
        const std::string value = ExtractJsonField(line, key);
        if (value.empty()) return false;
        try {
            size_t consumed = 0;
            const int parsed = std::stoi(value, &consumed);
            if (consumed != value.size()) return false;
            target = parsed;
            return true;
        } catch (...) {
            return false;
        }
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
        if (!inEntities) continue;

        if (line == "]") {
            entitiesClosed = true;
            continue;
        }
        if (entitiesClosed) continue;

        if (line == "{") {
            if (currentOpen || loaded.size() >= 100000) {
                parseError = true;
                break;
            }
            current = LoadedEntity{};
            currentOpen = true;
            continue;
        }
        if (line == "}" || line == "},") {
            if (!currentOpen || current.name.empty()) {
                parseError = true;
                break;
            }
            loaded.push_back(current);
            currentOpen = false;
            continue;
        }
        if (!currentOpen) continue;

        if (line.find("\"name\":") != std::string_view::npos) {
            current.name = ExtractJsonField(line, "name");
            if (current.name.empty()) parseError = true;
        }
        if (line.find("\"mesh_type\":") != std::string_view::npos) {
            current.meshType = ExtractJsonField(line, "mesh_type");
            if (current.meshType != "cube" && current.meshType != "sphere" &&
                current.meshType != "capsule" && current.meshType != "ramp" &&
                current.meshType != "stairs") parseError = true;
        }
        if (line.find("\"has_mesh_renderer\":") != std::string_view::npos) {
            const std::string value = ExtractJsonField(line, "has_mesh_renderer");
            if (value == "true") current.hasMeshRenderer = true;
            else if (value == "false") current.hasMeshRenderer = false;
            else parseError = true;
        }
        if (line.find("\"texture\":") != std::string_view::npos)
            current.texturePath = ExtractJsonField(line, "texture");
        if (line.find("\"light_type\":") != std::string_view::npos) {
            current.hasLight = parseInt(line, "light_type", current.lightType);
            if (!current.hasLight) parseError = true;
        }
        if (line.find("\"light_intensity\":") != std::string_view::npos &&
            !parseFloat(line, "light_intensity", current.lightIntensity)) parseError = true;
        if (line.find("\"light_range\":") != std::string_view::npos &&
            !parseFloat(line, "light_range", current.lightRange)) parseError = true;
        if (line.find("\"collider_shape\":") != std::string_view::npos) {
            current.hasColliderShape = parseInt(line, "collider_shape", current.colliderShape);
            if (!current.hasColliderShape) parseError = true;
        }
        if (line.find("\"collider_trigger\":") != std::string_view::npos) {
            const std::string value = ExtractJsonField(line, "collider_trigger");
            if (value == "true") current.colliderTrigger = true;
            else if (value == "false") current.colliderTrigger = false;
            else parseError = true;
        }
        if (line.find("\"collider\":") != std::string_view::npos) {
            current.hasCollider = parseArr3(line, "collider", current.colliderSize);
            if (!current.hasCollider) parseError = true;
        }
        if (!parseArr3(line, "position", current.position) ||
            !parseArr3(line, "rotation", current.rotation) ||
            !parseArr3(line, "scale", current.scale) ||
            !parseArr3(line, "color", current.color) ||
            !parseArr3(line, "collider_center", current.colliderCenter) ||
            !parseArr3(line, "light_color", current.lightColor)) parseError = true;
    }

    if (!entitiesClosed || currentOpen || parseError || loaded.size() > 100000) {
        std::cerr << "[SaveManager] Proje parse edilemedi; sahne degistirilmedi\n";
        return false;
    }

    // Only mutate the scene after the complete file has been read and
    // validated. A malformed save can no longer erase the current scene.
    std::vector<EntityID> toDelete;
    for (const auto& ePtr : scene->GetEntities()) {
        if (!IsSystemEntity(ePtr->GetName())) toDelete.push_back(ePtr->GetID());
    }
    for (EntityID id : toDelete) scene->DestroyEntity(id);

    std::vector<EntityHandle> created;
    created.reserve(loaded.size());
    try {
        for (const LoadedEntity& data : loaded) {
            Entity* e = scene->CreateEntity(data.name);
            created.push_back(e->GetHandle());
            if (auto* t = e->GetComponent<Transform>()) {
                t->position = data.position;
                t->rotation = data.rotation;
                t->scale = data.scale;
            }

            if (data.hasMeshRenderer) {
                auto* mr = e->AddComponent<MeshRenderer>();
                if (data.meshType == "sphere") mr->SetMeshAsset(Mesh::CreateSphereShared());
                else if (data.meshType == "capsule") mr->SetMeshAsset(Mesh::CreateCapsuleShared());
                else if (data.meshType == "ramp") mr->SetMeshAsset(Mesh::CreateRampShared());
                else if (data.meshType == "stairs") mr->SetMeshAsset(Mesh::CreateStairsShared());
                else mr->SetMeshAsset(Mesh::CreateCubeShared());
                mr->color = data.color;
                if (!data.texturePath.empty()) {
                    const std::string stem = std::filesystem::path(data.texturePath).stem().string();
                    auto tex = TextureManager::Get().LoadShared(stem, data.texturePath);
                    if (tex) mr->SetTextureAsset(std::move(tex));
                }
            }

            if (data.hasCollider) {
                auto* col = e->AddComponent<BoxCollider>();
                col->size = data.colliderSize;
                col->center = data.colliderCenter;
                col->shape = data.hasColliderShape
                                 ? static_cast<BoxCollider::Shape>(data.colliderShape == 1 ? 1 : 0)
                                 : (data.meshType == "ramp" ? BoxCollider::Shape::Ramp
                                                               : BoxCollider::Shape::Box);
                col->isTrigger = data.colliderTrigger;
            }
            if (data.hasLight) {
                auto* lc = e->AddComponent<LightComponent>();
                lc->type = static_cast<LightComponent::Type>(data.lightType);
                lc->color = data.lightColor;
                lc->intensity = data.lightIntensity;
                lc->range = data.lightRange;
            }
        }
    } catch (...) {
        for (EntityHandle handle : created) scene->DestroyEntity(handle);
        std::cerr << "[SaveManager] Entity olusturma basarisiz\n";
        return false;
    }

    std::cout << "[SaveManager] Proje yuklendi: " << filePath << std::endl;
    return true;
}

// ---------------------------------------------------------------------------
// DeleteProject
// ---------------------------------------------------------------------------
bool GameSaveManager::DeleteProject(const std::string& filePath) {
    std::error_code ec;
    if (std::filesystem::exists(filePath, ec)) {
        std::filesystem::remove(filePath, ec);
        if (ec) return false;
        std::cout << "[SaveManager] Proje silindi: " << filePath << std::endl;
    } else if (ec) {
        return false;
    }
    RefreshProjects();
    return !std::filesystem::exists(filePath, ec) && !ec;
}

} // namespace Archura
