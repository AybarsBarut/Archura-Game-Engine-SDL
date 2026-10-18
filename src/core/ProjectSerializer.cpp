#include "ProjectSerializer.h"
#include "SerializationJson.h"
#include "GameSaveManager.h"
#include "AtomicFile.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <locale>
#include <sstream>

namespace Archura {
std::string ProjectSerializer::EscapeJson(std::string_view value) { return Serialization::Escape(value); }

bool ProjectSerializer::SaveProject(const std::string& path, const ProjectConfig& config, Scene* scene) {
    if (!scene || path.empty() || config.name.empty()) return false;
    std::ostringstream file;
    file.imbue(std::locale::classic());
    file << std::setprecision(std::numeric_limits<float>::max_digits10);
    file << "{\n  \"format_version\": 2,\n  \"project\": {\n";
    file << "    \"name\": \"" << EscapeJson(config.name) << "\",\n";
    file << "    \"version\": \"" << EscapeJson(config.version) << "\",\n";
    file << "    \"start_scene\": \"" << EscapeJson(config.startScene) << "\"\n  },\n  \"entities\": [\n";
    if (!GameSaveManager::WriteEntities(file, scene, false)) return false;
    file << "  ]\n}\n";
    // Reject NaN/infinity and invalid UTF-8 before replacing a previous save.
    Serialization::Json validation;
    const std::string content = file.str();
    return Serialization::JsonParser(content).Parse(validation) &&
        GameSaveManager::LoadEntities(validation, scene, false, true) && Serialization::AtomicWrite(path, content);
}

bool ProjectSerializer::LoadProject(const std::string& path, ProjectConfig& outConfig, Scene* scene) {
    if (!scene || path.empty()) return false;
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) return false;
    const auto size = file.tellg();
    if (size <= 0 || size > 32 * 1024 * 1024) return false;
    std::string content(static_cast<size_t>(size), '\0');
    file.seekg(0); if (!file.read(content.data(), size)) return false;
    Serialization::Json root;
    if (!Serialization::JsonParser(content).Parse(root)) { std::cerr << "[ProjectSerializer] Invalid JSON: " << path << '\n'; return false; }
    int version = 1;
    if (!Serialization::IntField(root, "format_version", version) || (version != 1 && version != 2)) return false;
    const auto* project = root.Find("project");
    const auto* entities = root.Find("entities");
    if (!project || project->kind != Serialization::Json::Kind::Object || !entities || entities->kind != Serialization::Json::Kind::Array || entities->array.size() > 100000) return false;
    ProjectConfig config;
    if (!Serialization::StringField(*project, "name", config.name, true) || config.name.empty() ||
        !Serialization::StringField(*project, "version", config.version) ||
        !Serialization::StringField(*project, "start_scene", config.startScene)) return false;
    if (!GameSaveManager::LoadEntities(root, scene, false)) { std::cerr << "[ProjectSerializer] Scene validation or reconstruction failed: " << path << "; previous scene retained\n"; return false; }
    // Swapping strings cannot fail after committing the scene.
    outConfig.name.swap(config.name);
    outConfig.version.swap(config.version);
    outConfig.startScene.swap(config.startScene);
    return true;
}

} // namespace Archura
