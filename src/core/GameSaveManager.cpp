#include "GameSaveManager.h"
#include "SerializationJson.h"
#include "SceneData.h"
#include "AtomicFile.h"
#include <limits>
#include <locale>
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
    return Serialization::Escape(s);
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

    for (std::filesystem::directory_iterator it("saves", ec), end; !ec && it != end; it.increment(ec)) {
        const auto& entry = *it;
        if (!entry.is_regular_file(ec)) continue;
        if (entry.path().extension() != ".scene") continue;

        std::ifstream f(entry.path(), std::ios::in | std::ios::binary);
        if (!f.is_open()) continue;

        f.seekg(0, std::ios::end);
        std::streamsize size = f.tellg();
        f.seekg(0, std::ios::beg);

        if (size <= 0 || size > 32 * 1024 * 1024) continue;

        std::string content;
        content.resize(static_cast<size_t>(size));
        if (!f.read(content.data(), size)) continue;

        Serialization::Json root;
        if (!Serialization::JsonParser(content).Parse(root)) continue;
        const auto* entities = root.Find("entities");
        if (!entities || entities->kind != Serialization::Json::Kind::Array || entities->array.size() > 100000) continue;
        ProjectSave proj;
        proj.filePath = entry.path().string();
        if (!Serialization::StringField(root, "project_name", proj.name) ||
            !Serialization::StringField(root, "timestamp", proj.timestamp)) continue;
        proj.entityCount = static_cast<int>(entities->array.size());
        proj.valid = true;
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

    std::ostringstream file;
    file.imbue(std::locale::classic());
    file << std::setprecision(std::numeric_limits<float>::max_digits10);

    file << "{\n  \"format_version\": 1,\n";
    file << "  \"project_name\": \"" << EscapeJson(actualName) << "\",\n";
    file << "  \"timestamp\": \"" << EscapeJson(ts) << "\",\n";
    file << "  \"engine_version\": \"1.0\",\n";
    file << "  \"entities\": [\n";

    if (!WriteEntities(file, scene, true)) return false;

    file << "\n  ]\n}\n";
    Serialization::Json validation;
    const auto content = file.str();
    if (!Serialization::JsonParser(content).Parse(validation) || !LoadEntities(validation, scene, true, true) || !Serialization::AtomicWrite(path, content)) return false;

    std::cout << "[SaveManager] Proje kaydedildi: " << path << std::endl;
    RefreshProjects();
    return true;
}

bool GameSaveManager::WriteEntities(std::ostream& file, Scene* scene, bool omitSystem) {
    const auto& entities = scene->GetEntities();
    bool first = true;

    for (const auto& ePtr : entities) {
        Entity* e = ePtr.get();
        if (omitSystem && IsSystemEntity(e->GetName())) {
            if (e->GetParent() && !IsSystemEntity(e->GetParent()->GetName())) {
                std::cerr << "[SaveManager] Saved parent has an excluded system child; save cancelled\n";
                return false;
            }
            continue;
        }
        if (!Serialization::HasOnlySupportedComponents(e)) {
            std::cerr << "[SaveManager] Unsupported component on entity: " << e->GetName() << "; save cancelled\n";
            return false;
        }
        if (omitSystem && e->GetParent() && IsSystemEntity(e->GetParent()->GetName())) {
            std::cerr << "[SaveManager] Parent is excluded system entity; save cancelled: " << e->GetName() << '\n';
            return false;
        }

        if (!first) file << ",\n";
        first = false;

        auto* t  = e->GetComponent<Transform>();
        auto* mr = e->GetComponent<MeshRenderer>();
        auto* bc = e->GetComponent<BoxCollider>();
        auto* lc = e->GetComponent<LightComponent>();
        if (mr && (mr->GetShader() || (mr->GetTexture() && mr->GetTexture()->GetPath().empty()))) {
            std::cerr << "[SaveManager] Assigned shader or generated texture lacks a serialization source; save cancelled: " << e->GetName() << '\n';
            return false;
        }

        file << "    {\n";
        file << "      \"name\": \""  << EscapeJson(e->GetName()) << "\",\n";

        std::string meshType = bc && bc->shape == BoxCollider::Shape::Ramp
                                   ? "ramp"
                                   : "cube";
        std::string modelPath;
        const MeshSource* source = nullptr;
        if (mr && mr->GetMesh()) {
            source = &mr->GetMesh()->GetSource();
            switch (source->type) {
                case MeshSourceType::Cube: meshType = "cube"; break;
                case MeshSourceType::Plane: meshType = "plane"; break;
                case MeshSourceType::Sphere: meshType = "sphere"; break;
                case MeshSourceType::Capsule: meshType = "capsule"; break;
                case MeshSourceType::Stairs: meshType = "stairs"; break;
                case MeshSourceType::Ramp: meshType = "ramp"; break;
                case MeshSourceType::OBJ: meshType = "obj"; modelPath = source->path; break;
                case MeshSourceType::FBX: meshType = "fbx"; modelPath = source->path; break;
                default:
                    std::cerr << "[SaveManager] Custom mesh has no reconstructible source; save cancelled\n";
                    return false;
            }
        }
        file << "      \"mesh_type\": \"" << meshType << "\",\n";
        file << "      \"has_mesh_renderer\": " << (mr ? "true" : "false") << ",\n";

        if (source) {
            file << "      \"mesh_source\": {\"size\":" << source->size
                 << ",\"width\":" << source->width << ",\"height\":" << source->height
                 << ",\"depth\":" << source->depth << ",\"uv_scale\":" << source->uvScale
                 << ",\"radius\":" << source->radius << ",\"segments\":" << source->segments
                 << ",\"steps\":" << source->steps << "},\n";
        }
        file << "      \"has_mesh\": " << (mr && mr->GetMesh() ? "true" : "false") << ",\n";
        if (!modelPath.empty())
            file << "      \"model_path\": \"" << EscapeJson(modelPath) << "\",\n";

        if (t) {
            file << "      \"position\": [" << t->position.x << "," << t->position.y << "," << t->position.z << "],\n";
            file << "      \"rotation\": [" << t->rotation.x << "," << t->rotation.y << "," << t->rotation.z << "],\n";
            file << "      \"scale\":    [" << t->scale.x    << "," << t->scale.y    << "," << t->scale.z    << "],\n";
        }

        if (mr) {
            file << "      \"color\": [" << mr->color.r << "," << mr->color.g << "," << mr->color.b << "],\n";
            std::string texPath = mr->GetTexture() ? mr->GetTexture()->GetPath() : "";
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

        file << "      \"__end\": true";
        Serialization::WriteComponents(file, e);
        file << '\n';
        file << "    }";
    }

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

    Serialization::Json root;
    if (!Serialization::JsonParser(content).Parse(root)) {
        std::cerr << "[SaveManager] Invalid JSON in " << filePath << '\n'; return false;
    }
    int version = 1;
    if (!Serialization::IntField(root, "format_version", version) || version != 1) return false;
    const auto* entities = root.Find("entities");
    if (!entities || entities->kind != Serialization::Json::Kind::Array || entities->array.size() > 100000) return false;

    if (!LoadEntities(root, scene, true)) {
        std::cerr << "[SaveManager] Scene validation or reconstruction failed: " << filePath << "; previous scene retained\n"; return false;
    }

    std::cout << "[SaveManager] Proje yuklendi: " << filePath << std::endl;
    return true;
}

bool GameSaveManager::LoadEntities(const Serialization::Json& root, Scene* scene, bool keepSystem, bool validateOnly) {
    const auto* entities = root.Find("entities");
    if (!scene || !entities || entities->kind != Serialization::Json::Kind::Array || entities->array.size() > 100000) return false;
    std::error_code ec;
    struct LoadedEntity {
        Serialization::EntityData extra;
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
        MeshSource source;
        bool hasMesh = true;
        bool hasMeshRenderer = true; // backwards-compatible with old saves
        bool hasCollider = false;
        bool colliderTrigger = false;
        bool hasLight = false;
        bool hasColliderShape = false;
    };

    std::vector<LoadedEntity> loaded;
    loaded.reserve(64);
    for (const auto& data : entities->array) {
        LoadedEntity current;
        current.hasMeshRenderer = !root.Find("project");
        if (!Serialization::StringField(data, "name", current.name, true) ||
            !Serialization::StringField(data, "mesh_type", current.meshType) ||
            !Serialization::StringField(data, "texture", current.texturePath) ||
            !Serialization::BoolField(data, "has_mesh", current.hasMesh) ||
            !Serialization::BoolField(data, "has_mesh_renderer", current.hasMeshRenderer) ||
            !Serialization::BoolField(data, "collider_trigger", current.colliderTrigger) ||
            !Serialization::IntField(data, "light_type", current.lightType) ||
            !Serialization::IntField(data, "collider_shape", current.colliderShape) ||
            !Serialization::FloatField(data, "light_intensity", current.lightIntensity) ||
            !Serialization::FloatField(data, "light_range", current.lightRange) ||
            !Serialization::Vec3Field(data, "position", current.position) ||
            !Serialization::Vec3Field(data, "rotation", current.rotation) ||
            !Serialization::Vec3Field(data, "scale", current.scale) ||
            !Serialization::Vec3Field(data, "color", current.color) ||
            !Serialization::Vec3Field(data, "collider", current.colliderSize) ||
            !Serialization::Vec3Field(data, "collider_center", current.colliderCenter) ||
            !Serialization::Vec3Field(data, "light_color", current.lightColor)) return false;
        if (const auto* transform = data.Find("transform")) {
            if (transform->kind != Serialization::Json::Kind::Object ||
                !Serialization::Vec3Field(*transform, "position", current.position) ||
                !Serialization::Vec3Field(*transform, "rotation", current.rotation) ||
                !Serialization::Vec3Field(*transform, "scale", current.scale)) return false;
        }
        if (current.meshType == "cube") current.source.type = MeshSourceType::Cube;
        else if (current.meshType == "plane") current.source.type = MeshSourceType::Plane;
        else if (current.meshType == "sphere") current.source.type = MeshSourceType::Sphere;
        else if (current.meshType == "capsule") { current.source.type = MeshSourceType::Capsule; current.source.radius = 0.5f; current.source.height = 2.0f; }
        else if (current.meshType == "ramp") { current.source.type = MeshSourceType::Ramp; current.source.width = 1; current.source.height = 1; }
        else if (current.meshType == "stairs") { current.source.type = MeshSourceType::Stairs; current.source.width = 1; current.source.height = 1; }
        else if (current.meshType == "obj") current.source.type = MeshSourceType::OBJ;
        else if (current.meshType == "fbx") current.source.type = MeshSourceType::FBX;
        else return false;
        if (!Serialization::StringField(data, "model_path", current.source.path)) return false;
        if (const auto* source = data.Find("mesh_source")) {
            if (source->kind != Serialization::Json::Kind::Object ||
                !Serialization::FloatField(*source, "size", current.source.size) ||
                !Serialization::FloatField(*source, "width", current.source.width) ||
                !Serialization::FloatField(*source, "height", current.source.height) ||
                !Serialization::FloatField(*source, "depth", current.source.depth) ||
                !Serialization::FloatField(*source, "uv_scale", current.source.uvScale) ||
                !Serialization::FloatField(*source, "radius", current.source.radius) ||
                !Serialization::IntField(*source, "segments", current.source.segments) ||
                !Serialization::IntField(*source, "steps", current.source.steps)) return false;
        }
        if (current.source.type == MeshSourceType::OBJ || current.source.type == MeshSourceType::FBX) {
            if (current.source.path.empty() || current.source.path.find('\0') != std::string::npos ||
                !std::filesystem::is_regular_file(current.source.path, ec) || ec) return false;
        }
        if (!current.texturePath.empty() && (current.texturePath.find('\0') != std::string::npos ||
            !std::filesystem::is_regular_file(current.texturePath, ec) || ec)) return false;
        if (current.hasMeshRenderer && current.hasMesh) {
            if (current.source.size <= 0 || current.source.width <= 0 || current.source.height <= 0 ||
                current.source.depth <= 0 || current.source.radius <= 0 || current.source.segments < 3 ||
                current.source.segments > 512 || current.source.steps < 1 || current.source.steps > 4096) return false;
        }
        current.hasCollider = data.Find("collider") != nullptr;
        current.hasColliderShape = data.Find("collider_shape") != nullptr;
        current.hasLight = data.Find("light_type") != nullptr;
        if (current.colliderShape < 0 || current.colliderShape > 1 || current.lightType < 0 || current.lightType > 2 ||
            current.lightIntensity < 0 || current.lightRange < 0) return false;
        if (!Serialization::ReadComponents(data, current.extra)) return false;
        loaded.push_back(std::move(current));
    }

    if (!Serialization::ValidateHierarchy(loaded)) return false;
    if (validateOnly) return true;

    // Only mutate the scene after the complete file has been read and
    // validated. A malformed save can no longer erase the current scene.
    std::vector<EntityID> toDelete;
    for (const auto& ePtr : scene->GetEntities()) {
        if (!keepSystem || !IsSystemEntity(ePtr->GetName())) toDelete.push_back(ePtr->GetID());
    }

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
                if (data.hasMesh) {
                    auto mesh = Mesh::CreateFromSource(data.source);
                    if (!mesh) throw std::runtime_error("Mesh reconstruction failed");
                    mr->SetMeshAsset(std::move(mesh));
                }
                mr->color = data.color;
                if (!data.texturePath.empty()) {
                    const std::string assetKey = std::filesystem::weakly_canonical(data.texturePath).generic_string();
                    auto tex = TextureManager::Get().LoadShared(assetKey, data.texturePath);
                    if (!tex) throw std::runtime_error("Texture reconstruction failed");
                    mr->SetTextureAsset(std::move(tex));
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
            Serialization::ApplyComponents(e, data.extra);
        }
        if (!Serialization::LinkHierarchy(scene, loaded, created)) throw std::runtime_error("Invalid hierarchy");
    } catch (...) {
        for (EntityHandle handle : created) scene->DestroyEntity(handle);
        std::cerr << "[SaveManager] Component/resource/hierarchy reconstruction failed; previous scene retained\n";
        return false;
    }

    for (EntityID id : toDelete) scene->DestroyEntity(id);

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
