#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

namespace Archura {

class Shader;
class Texture;
class Mesh;

/**
 * @brief ResourceManager - Unified asset management
 * 
 * Tüm game asset'lerini merkezi olarak yönetir
 */
class ResourceManager {
public:
    static ResourceManager& Get();

    // Project root configuration and canonical asset path resolution
    void SetProjectRoot(const std::filesystem::path& rootPath);
    const std::filesystem::path& GetProjectRoot() const noexcept { return m_ProjectRoot; }
    void ClearProjectRoot() noexcept { m_ProjectRoot.clear(); }

    static bool IsSafeAssetPath(std::string_view rawPath) noexcept;

    std::filesystem::path ResolveAssetPath(
        std::string_view rawPath,
        const std::filesystem::path& documentRoot = {},
        bool requireExisting = true) const;

    std::string CanonicalAssetKey(
        std::string_view rawPath,
        const std::filesystem::path& documentRoot = {}) const;

    // Shader management
    Shader* LoadShader(const std::string& name, const std::string& vertPath, const std::string& fragPath);
    Shader* GetShader(const std::string& name);
    std::shared_ptr<Shader> GetShaderShared(const std::string& name);

    // Texture management
    Texture* LoadTexture(const std::string& name, const std::string& path, bool generateMipmaps = true);
    std::shared_ptr<Texture> LoadTextureShared(const std::string& name, const std::string& path, bool generateMipmaps = true);
    Texture* GetTexture(const std::string& name);
    std::shared_ptr<Texture> GetTextureShared(const std::string& name);

    // Mesh management (procedural veya loaded)
    Mesh* AddMesh(const std::string& name, Mesh* mesh);
    std::shared_ptr<Mesh> AddMesh(const std::string& name, std::shared_ptr<Mesh> mesh);
    Mesh* GetMesh(const std::string& name);
    std::shared_ptr<Mesh> GetMeshShared(const std::string& name);

    // Camera management (for script-created cameras)
    class Camera* AddCamera(const std::string& name, Camera* camera);
    Camera* GetCamera(const std::string& name);

    // Resource lifecycle
    void Clear();
    void ClearShaders();
    void ClearTextures();
    void ClearMeshes();
    void ClearCameras();

    // Stats
    size_t GetShaderCount() const { return m_Shaders.size(); }
    size_t GetTextureCount() const { return m_TexturesByCanonicalPath.empty() ? m_Textures.size() : m_TexturesByCanonicalPath.size(); }
    size_t GetMeshCount() const { return m_Meshes.size(); }
    size_t GetCameraCount() const { return m_Cameras.size(); }

private:
    ResourceManager() = default;
    ~ResourceManager() { Clear(); }

    ResourceManager(const ResourceManager&) = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;

private:
    std::filesystem::path m_ProjectRoot;
    std::unordered_map<std::string, std::shared_ptr<Shader>> m_Shaders;
    std::unordered_map<std::string, std::shared_ptr<Texture>> m_Textures;
    std::unordered_map<std::string, std::shared_ptr<Texture>> m_TexturesByCanonicalPath;
    std::unordered_map<std::string, std::shared_ptr<Mesh>> m_Meshes;
    std::unordered_map<std::string, Camera*> m_Cameras;
};

} // namespace Archura
