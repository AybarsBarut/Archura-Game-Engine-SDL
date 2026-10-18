#include "ResourceManager.h"
#include "../rendering/Shader.h"
#include "../rendering/Texture.h"
#include "../rendering/Mesh.h"
#include "../rendering/Camera.h"
#include <iostream>
#include <system_error>

namespace Archura {

ResourceManager& ResourceManager::Get() {
    static ResourceManager instance;
    return instance;
}

void ResourceManager::SetProjectRoot(const std::filesystem::path& rootPath) {
    if (rootPath.empty()) {
        m_ProjectRoot.clear();
        return;
    }
    std::error_code ec;
    const auto canonical = std::filesystem::weakly_canonical(rootPath, ec);
    m_ProjectRoot = ec ? rootPath.lexically_normal() : canonical;
}

bool ResourceManager::IsSafeAssetPath(std::string_view rawPath) noexcept {
    if (rawPath.empty() || rawPath.find('\0') != std::string_view::npos) {
        return false;
    }
    const std::filesystem::path path{std::string(rawPath)};
    if (path.has_root_name() && !path.has_root_directory()) {
        // Reject Windows drive-relative paths like "C:foo/bar.png".
        return false;
    }
    for (const auto& part : path) {
        if (part == "..") {
            return false;
        }
    }
    return true;
}

std::filesystem::path ResourceManager::ResolveAssetPath(
    std::string_view rawPath,
    const std::filesystem::path& documentRoot,
    bool requireExisting) const {
    if (!IsSafeAssetPath(rawPath)) {
        return {};
    }

    const std::filesystem::path inputPath{std::string(rawPath)};
    auto existsRegular = [](const std::filesystem::path& candidate) -> bool {
        if (candidate.empty()) return false;
        std::error_code ec;
        return (std::filesystem::is_regular_file(candidate, ec) ||
                std::filesystem::is_directory(candidate, ec)) && !ec;
    };

    if (inputPath.is_relative()) {
        if (!documentRoot.empty()) {
            const auto fromDoc = (documentRoot / inputPath).lexically_normal();
            if (existsRegular(fromDoc)) return fromDoc;
            const auto parentRoot = documentRoot.parent_path();
            if (!parentRoot.empty()) {
                const auto fromParent = (parentRoot / inputPath).lexically_normal();
                if (existsRegular(fromParent)) return fromParent;
            }
        }
        if (!m_ProjectRoot.empty()) {
            const auto fromProject = (m_ProjectRoot / inputPath).lexically_normal();
            if (existsRegular(fromProject)) return fromProject;
        }
        const auto fromCwd = inputPath.lexically_normal();
        if (existsRegular(fromCwd)) return fromCwd;

        if (!requireExisting) {
            if (!documentRoot.empty()) return (documentRoot / inputPath).lexically_normal();
            if (!m_ProjectRoot.empty()) return (m_ProjectRoot / inputPath).lexically_normal();
            return fromCwd;
        }
        return {};
    }

    // Absolute path: try direct match first, then check if an "assets/..." tail
    // can be relocated under documentRoot or m_ProjectRoot.
    const auto normalizedAbs = inputPath.lexically_normal();
    if (existsRegular(normalizedAbs)) {
        return normalizedAbs;
    }

    std::filesystem::path tail;
    bool collecting = false;
    for (const auto& part : normalizedAbs) {
        const std::string token = part.generic_string();
        if (!collecting && (token == "assets" || token == "models" || token == "objects" ||
                            token == "textures" || token == "audio")) {
            collecting = true;
        }
        if (collecting) {
            tail /= part;
        }
    }
    if (!tail.empty()) {
        if (!documentRoot.empty()) {
            const auto relocatedDoc = (documentRoot / tail).lexically_normal();
            if (existsRegular(relocatedDoc)) return relocatedDoc;
            const auto parentRoot = documentRoot.parent_path();
            if (!parentRoot.empty()) {
                const auto relocatedParent = (parentRoot / tail).lexically_normal();
                if (existsRegular(relocatedParent)) return relocatedParent;
            }
        }
        if (!m_ProjectRoot.empty()) {
            const auto relocatedProj = (m_ProjectRoot / tail).lexically_normal();
            if (existsRegular(relocatedProj)) return relocatedProj;
        }
    }

    return requireExisting ? std::filesystem::path{} : normalizedAbs;
}

std::string ResourceManager::CanonicalAssetKey(
    std::string_view rawPath,
    const std::filesystem::path& documentRoot) const {
    if (!IsSafeAssetPath(rawPath)) {
        return {};
    }
    std::filesystem::path resolved = ResolveAssetPath(rawPath, documentRoot, true);
    if (resolved.empty()) {
        resolved = ResolveAssetPath(rawPath, documentRoot, false);
    }
    if (resolved.empty()) {
        return {};
    }
    std::error_code ec;
    const auto canonical = std::filesystem::weakly_canonical(resolved, ec);
    return (ec ? resolved.lexically_normal() : canonical).generic_string();
}

// ==================== Shader Yonetimi ====================

Shader* ResourceManager::LoadShader(const std::string& name, const std::string& vertPath, const std::string& fragPath) {
    auto it = m_Shaders.find(name);
    if (it != m_Shaders.end()) {
        return it->second.get();
    }

    const auto resolvedVert = ResolveAssetPath(vertPath, {}, false);
    const auto resolvedFrag = ResolveAssetPath(fragPath, {}, false);
    auto shader = std::make_shared<Shader>();
    if (shader->LoadFromFile(resolvedVert.empty() ? vertPath : resolvedVert.string(),
                             resolvedFrag.empty() ? fragPath : resolvedFrag.string())) {
        m_Shaders[name] = shader;
        return shader.get();
    }

    std::cerr << "Failed to load shader: " << name << std::endl;
    return nullptr;
}

Shader* ResourceManager::GetShader(const std::string& name) {
    auto it = m_Shaders.find(name);
    return (it != m_Shaders.end()) ? it->second.get() : nullptr;
}

std::shared_ptr<Shader> ResourceManager::GetShaderShared(const std::string& name) {
    auto it = m_Shaders.find(name);
    return (it != m_Shaders.end()) ? it->second : nullptr;
}

// ==================== Doku Yonetimi ====================

Texture* ResourceManager::LoadTexture(const std::string& name, const std::string& path, bool generateMipmaps) {
    return LoadTextureShared(name, path, generateMipmaps).get();
}

std::shared_ptr<Texture> ResourceManager::LoadTextureShared(const std::string& name,
                                                            const std::string& path,
                                                            bool generateMipmaps) {
    const std::string canonicalKey = CanonicalAssetKey(path);
    if (!canonicalKey.empty()) {
        auto canonIt = m_TexturesByCanonicalPath.find(canonicalKey);
        if (canonIt != m_TexturesByCanonicalPath.end() && canonIt->second) {
            if (!name.empty()) {
                m_Textures[name] = canonIt->second;
            }
            return canonIt->second;
        }
    }

    auto it = m_Textures.find(name);
    if (it != m_Textures.end() && it->second) {
        if (path.empty() || it->second->GetCanonicalPath() == canonicalKey) {
            return it->second;
        }
    }

    auto texture = std::make_shared<Texture>();
    if (texture->LoadFromFile(path, generateMipmaps)) {
        if (!canonicalKey.empty()) {
            texture->SetCanonicalPath(canonicalKey);
            m_TexturesByCanonicalPath[canonicalKey] = texture;
            m_Textures[canonicalKey] = texture;
        }
        if (!name.empty()) {
            m_Textures[name] = texture;
        }
        return texture;
    }

    return nullptr;
}

Texture* ResourceManager::GetTexture(const std::string& name) {
    return GetTextureShared(name).get();
}

std::shared_ptr<Texture> ResourceManager::GetTextureShared(const std::string& name) {
    const std::string canonicalKey = CanonicalAssetKey(name);
    if (!canonicalKey.empty()) {
        auto canonIt = m_TexturesByCanonicalPath.find(canonicalKey);
        if (canonIt != m_TexturesByCanonicalPath.end()) {
            return canonIt->second;
        }
    }
    auto it = m_Textures.find(name);
    return (it != m_Textures.end()) ? it->second : nullptr;
}

// ==================== Model Yonetimi ====================

Mesh* ResourceManager::AddMesh(const std::string& name, Mesh* mesh) {
    return AddMesh(name, std::shared_ptr<Mesh>(mesh)).get();
}

std::shared_ptr<Mesh> ResourceManager::AddMesh(const std::string& name,
                                               std::shared_ptr<Mesh> mesh) {
    if (!mesh) return nullptr;

    auto it = m_Meshes.find(name);
    if (it != m_Meshes.end()) {
        // Add is intentionally stable: legacy GetMesh() observers remain valid
        // until Clear(). Explicit hot replacement must use versioned names or a
        // future generational handle API.
        return it->second;
    }

    m_Meshes.emplace(name, mesh);
    return mesh;
}

Mesh* ResourceManager::GetMesh(const std::string& name) {
    auto it = m_Meshes.find(name);
    return (it != m_Meshes.end()) ? it->second.get() : nullptr;
}

std::shared_ptr<Mesh> ResourceManager::GetMeshShared(const std::string& name) {
    auto it = m_Meshes.find(name);
    return (it != m_Meshes.end()) ? it->second : nullptr;
}

// ==================== Camera Yonetimi ====================

Camera* ResourceManager::AddCamera(const std::string& name, Camera* camera) {
    if (!camera) return nullptr;

    auto it = m_Cameras.find(name);
    if (it != m_Cameras.end()) {
        delete it->second;
    }

    m_Cameras[name] = camera;
    return camera;
}

Camera* ResourceManager::GetCamera(const std::string& name) {
    auto it = m_Cameras.find(name);
    return (it != m_Cameras.end()) ? it->second : nullptr;
}

// ==================== Temizlik ====================

void ResourceManager::Clear() {
    ClearShaders();
    ClearTextures();
    ClearMeshes();
    ClearCameras();
}

void ResourceManager::ClearShaders() {
    m_Shaders.clear();
}

void ResourceManager::ClearTextures() {
    m_Textures.clear();
    m_TexturesByCanonicalPath.clear();
}

void ResourceManager::ClearMeshes() {
    m_Meshes.clear();
}

void ResourceManager::ClearCameras() {
    for (auto& pair : m_Cameras) {
        delete pair.second;
    }
    m_Cameras.clear();
}

} // namespace Archura
