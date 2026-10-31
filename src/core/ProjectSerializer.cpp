#include "ProjectSerializer.h"
#include "SerializationJson.h"
#include "GameSaveManager.h"
#include "ResourceManager.h"
#include "AtomicFile.h"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <locale>
#include <sstream>

namespace Archura {
namespace {
constexpr size_t MaxProjectBytes = 32u * 1024u * 1024u;

// Resolve with the prospective project's root during staging. A failed load or
// any save restores the previous session; success commits without allocating.
class ResourceContext {
public:
    ResourceContext(const std::filesystem::path& root, bool explicitRoot)
        : m_ExplicitRoot(explicitRoot) {
        std::error_code error;
        if (explicitRoot && (!std::filesystem::is_directory(root, error) || error)) return;
        if (!root.empty()) {
            m_PreviousRoot = std::filesystem::weakly_canonical(root, error);
            if (error) m_PreviousRoot = root.lexically_normal();
        }
        ResourceManager::Get().SwapProjectRoot(m_PreviousRoot, m_ExplicitRoot);
        m_Valid = m_Restore = true;
    }
    ~ResourceContext() {
        if (m_Restore) ResourceManager::Get().SwapProjectRoot(m_PreviousRoot, m_ExplicitRoot);
    }
    bool Valid() const noexcept { return m_Valid; }
    void Commit() noexcept { m_Restore = false; }
    ResourceContext(const ResourceContext&) = delete;
    ResourceContext& operator=(const ResourceContext&) = delete;
private:
    std::filesystem::path m_PreviousRoot;
    bool m_ExplicitRoot = false;
    bool m_Valid = false, m_Restore = false;
};
}

std::string ProjectSerializer::EscapeJson(std::string_view value) { return Serialization::Escape(value); }

bool ProjectSerializer::SaveProject(const std::string& path, const ProjectConfig& config, Scene* scene) {
    return SaveProjectPath(std::filesystem::path(path), config, scene);
}

bool ProjectSerializer::SaveProject(const std::string& path, const ProjectConfig& config, Scene* scene,
                                    const std::filesystem::path& contentRoot) {
    return SaveProjectPath(std::filesystem::path(path), config, scene, contentRoot);
}

bool ProjectSerializer::SaveProjectPath(const std::filesystem::path& path, const ProjectConfig& config, Scene* scene,
                                        const std::filesystem::path& contentRoot) {
    if (!scene || path.empty() || config.name.empty()) return false;
    const auto& resources = ResourceManager::Get();
    ResourceContext context(contentRoot.empty() ? resources.GetProjectRoot() : contentRoot,
                            contentRoot.empty() ? resources.HasExplicitContentRoot() : true);
    if (!context.Valid()) return false;
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
    // The complete document, including its project metadata and JSON wrapper,
    // must fit the same bound enforced when reopening it.
    if (content.size() > MaxProjectBytes) return false;
    const std::filesystem::path docRoot = path.parent_path();
    return Serialization::JsonParser(content).Parse(validation) &&
        GameSaveManager::LoadEntities(validation, scene, false, true, docRoot) &&
        Serialization::AtomicWritePath(path, content);
}

bool ProjectSerializer::LoadProject(const std::string& path, ProjectConfig& outConfig, Scene* scene) {
    return LoadProjectPath(std::filesystem::path(path), outConfig, scene);
}

bool ProjectSerializer::LoadProject(const std::string& path, ProjectConfig& outConfig, Scene* scene,
                                    const std::filesystem::path& contentRoot) {
    return LoadProjectPath(std::filesystem::path(path), outConfig, scene, contentRoot);
}

bool ProjectSerializer::LoadProjectPath(const std::filesystem::path& path, ProjectConfig& outConfig, Scene* scene,
                                        const std::filesystem::path& contentRoot) {
    if (!scene || path.empty()) return false;
    if (path.native().find(std::filesystem::path::value_type{}) != std::filesystem::path::string_type::npos) return false;
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) return false;
    const auto size = file.tellg();
    if (size <= 0 || size > static_cast<std::streamoff>(MaxProjectBytes)) return false;
    std::string content(static_cast<size_t>(size), '\0');
    file.seekg(0); if (!file.read(content.data(), size)) return false;
    Serialization::Json root;
    if (!Serialization::JsonParser(content).Parse(root)) { std::cerr << "[ProjectSerializer] Invalid JSON: " << path.u8string() << '\n'; return false; }
    int version = 1;
    if (!Serialization::IntField(root, "format_version", version) || (version != 1 && version != 2)) return false;
    const auto* project = root.Find("project");
    const auto* entities = root.Find("entities");
    const auto budget = GameSaveManager::GetReconstructionBudget();
    if (!project || project->kind != Serialization::Json::Kind::Object || !entities || entities->kind != Serialization::Json::Kind::Array || entities->array.size() > budget.maxEntities) return false;
    ProjectConfig config;
    if (!Serialization::StringField(*project, "name", config.name, true) || config.name.empty() ||
        !Serialization::StringField(*project, "version", config.version) ||
        !Serialization::StringField(*project, "start_scene", config.startScene)) return false;
    const std::filesystem::path docRoot = path.parent_path();
    ResourceContext context(contentRoot.empty() ? docRoot : contentRoot, !contentRoot.empty());
    if (!context.Valid()) return false;
    if (!GameSaveManager::LoadEntities(root, scene, false, false, docRoot)) { std::cerr << "[ProjectSerializer] Scene validation or reconstruction failed: " << path.u8string() << "; previous scene retained\n"; return false; }
    context.Commit();
    // Swapping strings cannot fail after committing the scene.
    outConfig.name.swap(config.name);
    outConfig.version.swap(config.version);
    outConfig.startScene.swap(config.startScene);
    return true;
}

} // namespace Archura
