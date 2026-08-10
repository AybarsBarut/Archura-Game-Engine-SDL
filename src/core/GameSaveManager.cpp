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

    if (size <= 0) return false;

    std::string content;
    content.resize(static_cast<size_t>(size));
    if (!file.read(content.data(), size)) return false;
    file.close();

    {
        const auto& entities = scene->GetEntities();
        std::vector<uint32_t> toDelete;
        toDelete.reserve(entities.size());
        for (const auto& ePtr : entities) {
            if (!IsSystemEntity(ePtr->GetName()))
                toDelete.push_back(ePtr->GetID());
        }
        for (auto id : toDelete)
            scene->DestroyEntity(id);
    }

    std::string_view remaining(content);
    bool inEntities = false;

    std::string  curName;
    std::string  curMeshType = "cube";
    std::string  curTexPath;
    float px=0,py=0,pz=0, rx=0,ry=0,rz=0, sx=1,sy=1,sz=1;
    float cr=1,cg=1,cb=1;
    float colX=1,colY=1,colZ=1;
    float colCenterX=0,colCenterY=0,colCenterZ=0;
    int colShape=0;
    bool hasColliderShape=false, colTrigger=false;
    bool  hasLight = false;
    int   lightType = 0;
    float lr=1,lg=1,lb=1, lInt=1, lRange=20;

    auto applyEntity = [&]() {
        if (curName.empty()) return;
        Entity* e = scene->CreateEntity(curName);
        auto* t = e->GetComponent<Transform>();
        if (t) { t->position={px,py,pz}; t->rotation={rx,ry,rz}; t->scale={sx,sy,sz}; }

        auto* mr = e->AddComponent<MeshRenderer>();
        if (curMeshType == "sphere")
            mr->SetMeshAsset(Mesh::CreateSphereShared());
        else if (curMeshType == "capsule")
            mr->SetMeshAsset(Mesh::CreateCapsuleShared());
        else if (curMeshType == "ramp")
            mr->SetMeshAsset(Mesh::CreateRampShared());
        else if (curMeshType == "stairs")
            mr->SetMeshAsset(Mesh::CreateStairsShared());
        else
            mr->SetMeshAsset(Mesh::CreateCubeShared());

        mr->color = {cr,cg,cb};

        if (!curTexPath.empty()) {
            std::string stem = std::filesystem::path(curTexPath).stem().string();
            auto tex = TextureManager::Get().LoadShared(stem, curTexPath);
            if (tex) mr->SetTextureAsset(std::move(tex));
        }

        auto* col = e->AddComponent<BoxCollider>();
        col->size = {colX, colY, colZ};
        col->center = {colCenterX, colCenterY, colCenterZ};
        col->shape = hasColliderShape
                         ? static_cast<BoxCollider::Shape>(colShape == 1 ? 1 : 0)
                         : (curMeshType == "ramp" ? BoxCollider::Shape::Ramp
                                                   : BoxCollider::Shape::Box);
        col->isTrigger = colTrigger;

        if (hasLight) {
            auto* lc = e->AddComponent<LightComponent>();
            lc->type      = static_cast<LightComponent::Type>(lightType);
            lc->color     = {lr,lg,lb};
            lc->intensity = lInt;
            lc->range     = lRange;
        }
    };

    auto resetCurrent = [&]() {
        curName.clear(); curMeshType = "cube"; curTexPath.clear();
        px=0; py=0; pz=0; rx=0; ry=0; rz=0; sx=1; sy=1; sz=1;
        cr=1; cg=1; cb=1; colX=1; colY=1; colZ=1;
        colCenterX=0; colCenterY=0; colCenterZ=0;
        colShape=0; hasColliderShape=false; colTrigger=false;
        hasLight=false; lightType=0; lr=1; lg=1; lb=1; lInt=1; lRange=20;
    };

    auto parseArr3 = [](std::string_view line, std::string_view key, float& a, float& b, float& c) {
        size_t kp = line.find(key);
        if (kp == std::string_view::npos || kp == 0 || line[kp-1] != '"') return;
        size_t p = line.find('[', kp);
        if (p == std::string_view::npos) return; ++p;
        
        const char* str = line.data() + p;
        const char* endStr = line.data() + line.length();
        char* nextPtr;
        
        float va = std::strtof(str, &nextPtr);
        if (str == nextPtr) return;
        str = nextPtr;
        while(str < endStr && (*str == ' ' || *str == '\t' || *str == ',')) ++str;
        
        float vb = std::strtof(str, &nextPtr);
        if (str == nextPtr) return;
        str = nextPtr;
        while(str < endStr && (*str == ' ' || *str == '\t' || *str == ',')) ++str;
        
        float vc = std::strtof(str, nullptr);
        
        a = va; b = vb; c = vc;
    };

    while (!remaining.empty()) {
        size_t eol = remaining.find('\n');
        std::string_view line;
        if (eol == std::string_view::npos) {
            line = remaining;
            remaining = {};
        } else {
            line = remaining.substr(0, eol);
            remaining = remaining.substr(eol + 1);
        }

        size_t startPos = line.find_first_not_of(" \t\r");
        if (startPos == std::string_view::npos) continue;
        line = line.substr(startPos);
        
        size_t endPos = line.find_last_not_of(" \t\r");
        if (endPos != std::string_view::npos) {
            line = line.substr(0, endPos + 1);
        }

        if (line.find("\"entities\"") != std::string_view::npos) { inEntities = true; continue; }
        if (!inEntities) continue;

        if (line == "{") { resetCurrent(); }
        else if (line == "}" || line == "},") { applyEntity(); curName.clear(); }
        else {
            if (line.find("\"name\":")      != std::string_view::npos) curName     = ExtractJsonField(line, "name");
            if (line.find("\"mesh_type\":") != std::string_view::npos) curMeshType = ExtractJsonField(line, "mesh_type");
            if (line.find("\"texture\":")   != std::string_view::npos) curTexPath  = ExtractJsonField(line, "texture");
            if (line.find("\"light_type\":") != std::string_view::npos) {
                hasLight = true;
                std::string v = ExtractJsonField(line, "light_type");
                if (!v.empty()) try { lightType = std::stoi(v); } catch (...) {}
            }
            if (line.find("\"light_intensity\":") != std::string_view::npos) {
                std::string v = ExtractJsonField(line, "light_intensity");
                if (!v.empty()) try { lInt = std::stof(v); } catch (...) {}
            }
            if (line.find("\"light_range\":") != std::string_view::npos) {
                std::string v = ExtractJsonField(line, "light_range");
                if (!v.empty()) try { lRange = std::stof(v); } catch (...) {}
            }
            if (line.find("\"collider_shape\":") != std::string_view::npos) {
                std::string v = ExtractJsonField(line, "collider_shape");
                if (!v.empty()) try {
                    colShape = std::stoi(v);
                    hasColliderShape = true;
                } catch (...) {}
            }
            if (line.find("\"collider_trigger\":") != std::string_view::npos)
                colTrigger = line.find("true") != std::string_view::npos;

            parseArr3(line, "position",  px, py, pz);
            parseArr3(line, "rotation",  rx, ry, rz);
            parseArr3(line, "scale",     sx, sy, sz);
            parseArr3(line, "color",     cr, cg, cb);
            if (line.find("\"collider\":") != std::string_view::npos)
                parseArr3(line, "collider", colX, colY, colZ);
            if (line.find("\"collider_center\":") != std::string_view::npos)
                parseArr3(line, "collider_center", colCenterX, colCenterY, colCenterZ);
            parseArr3(line, "light_color", lr, lg, lb);
        }
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
        std::cout << "[SaveManager] Proje silindi: " << filePath << std::endl;
    }
    RefreshProjects();
    return true;
}

} // namespace Archura
