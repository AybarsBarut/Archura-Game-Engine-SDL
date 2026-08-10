#include "ResourceManager.h"
#include "../rendering/Shader.h"
#include "../rendering/Texture.h"
#include "../rendering/Mesh.h"
#include "../rendering/Camera.h"
#include <iostream>

namespace Archura {

ResourceManager& ResourceManager::Get() {
    static ResourceManager instance;
    return instance;
}

// ==================== Shader Yonetimi ====================

Shader* ResourceManager::LoadShader(const std::string& name, const std::string& vertPath, const std::string& fragPath) {
    // Zaten var mi?
    auto it = m_Shaders.find(name);
    if (it != m_Shaders.end()) {
        // std::cout << "Shader '" << name << "' already loaded, returning cached version." << std::endl;
        return it->second.get();
    }

    // Yeni shader yukle
    auto shader = std::make_shared<Shader>();
    if (shader->LoadFromFile(vertPath, fragPath)) {
        m_Shaders[name] = shader;
        // std::cout << "Loaded shader: " << name << std::endl;
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
    auto it = m_Textures.find(name);
    if (it != m_Textures.end()) {
        return it->second.get();
    }

    auto texture = std::make_shared<Texture>();
    if (texture->LoadFromFile(path, generateMipmaps)) {
        m_Textures[name] = texture;
        return texture.get();
    }

    return nullptr;
}

Texture* ResourceManager::GetTexture(const std::string& name) {
    auto it = m_Textures.find(name);
    return (it != m_Textures.end()) ? it->second.get() : nullptr;
}

std::shared_ptr<Texture> ResourceManager::GetTextureShared(const std::string& name) {
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
        // Eski kamerayi sil ve yenisiyle degistir
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
