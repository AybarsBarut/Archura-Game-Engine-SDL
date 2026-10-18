#include "Texture.h"
#include "RenderThread.h"
#include "../core/Logger.h"
#include "../core/ResourceManager.h"
#include <glad/glad.h>
#include <iostream>
#include <limits>
#include <utility>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace Archura {

Texture::~Texture() {
    Release();
}

Texture::Texture(Texture&& other) noexcept {
    *this = std::move(other);
}

Texture& Texture::operator=(Texture&& other) noexcept {
    if (this == &other) return *this;
    Release();
    m_TextureID = std::exchange(other.m_TextureID, 0);
    m_Width = std::exchange(other.m_Width, 0);
    m_Height = std::exchange(other.m_Height, 0);
    m_Channels = std::exchange(other.m_Channels, 0);
    m_Path = std::move(other.m_Path);
    m_CanonicalPath = std::move(other.m_CanonicalPath);
    return *this;
}

void Texture::Release() noexcept {
    if (m_TextureID && !RenderThread::IsCurrent()) {
        ARCH_LOG_ERROR("Texture destruction attempted outside the render thread; GPU deletion skipped");
        m_TextureID = 0;
        return;
    }
    if (m_TextureID) glDeleteTextures(1, &m_TextureID);
    m_TextureID = 0;
    m_Width = 0;
    m_Height = 0;
    m_Channels = 0;
}

bool Texture::LoadFromFile(const std::string& path, bool generateMipmaps,
                           bool srgb) {
    if (!ResourceManager::IsSafeAssetPath(path)) {
        ARCH_LOG_ERROR("Unsafe or empty texture path rejected: " + path);
        return false;
    }

    const std::filesystem::path resolved =
        ResourceManager::Get().ResolveAssetPath(path, {}, true);
    const std::string loadPath = resolved.empty() ? path : resolved.string();
    const std::string canonicalKey =
        ResourceManager::Get().CanonicalAssetKey(loadPath);

    int width = 0;
    int height = 0;
    int channels = 0;

    // stb_image ile resim yukle
    // The thread-local variant avoids races with terrain/asset worker loads.
    stbi_set_flip_vertically_on_load_thread(1);
    unsigned char* data = stbi_load(loadPath.c_str(), &width, &height, &channels, 0);
    
    if (!data) {
        ARCH_LOG_ERROR("Failed to load texture: " + path);
        ARCH_LOG_ERROR("STB Error: " + std::string(stbi_failure_reason()));
        return false;
    }

    if (width <= 0 || height <= 0 || channels < 1 || channels > 4) {
        stbi_image_free(data);
        ARCH_LOG_ERROR("Invalid texture dimensions or channel count: " + path);
        return false;
    }

    if (!RenderThread::IsCurrent()) {
        // Headless CPU validation path (mirroring Mesh::SetupMesh when
        // RenderThread is detached): image file and dimensions are verified,
        // while OpenGL object creation is skipped.
        stbi_image_free(data);
        m_TextureID = 0;
        m_Width = width;
        m_Height = height;
        m_Channels = channels;
        m_Path = path;
        m_CanonicalPath = canonicalKey;
        return true;
    }

    GLint maxTextureSize = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
    if (width > maxTextureSize || height > maxTextureSize) {
        stbi_image_free(data);
        ARCH_LOG_ERROR("Texture exceeds GL_MAX_TEXTURE_SIZE: " + path);
        return false;
    }

    // OpenGL dokusu olustur
    unsigned int newTexture = 0;
    glGenTextures(1, &newTexture);
    if (newTexture == 0) {
        stbi_image_free(data);
        ARCH_LOG_ERROR("OpenGL failed to allocate texture: " + path);
        return false;
    }
    glBindTexture(GL_TEXTURE_2D, newTexture);

    // Doku parametreleri
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, generateMipmaps ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Bicim belirle
    GLenum format = GL_RED;
    GLenum internalFormat = GL_R8;
    if (channels == 2) { format = GL_RG; internalFormat = GL_RG8; }
    else if (channels == 3) { format = GL_RGB; internalFormat = srgb ? GL_SRGB8 : GL_RGB8; }
    else if (channels == 4) { format = GL_RGBA; internalFormat = srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8; }

    // Doku verisini GPU'ya yukle
    GLint previousUnpackAlignment = 4;
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &previousUnpackAlignment);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format,
                 GL_UNSIGNED_BYTE, data);
    glPixelStorei(GL_UNPACK_ALIGNMENT, previousUnpackAlignment);
    
    if (generateMipmaps) {
        glGenerateMipmap(GL_TEXTURE_2D);
    }

    // Bellegi serbest birak
    stbi_image_free(data);

    glBindTexture(GL_TEXTURE_2D, 0);

    const unsigned int oldTexture = m_TextureID;
    m_TextureID = newTexture;
    m_Width = width;
    m_Height = height;
    m_Channels = channels;
    m_Path = path;
    m_CanonicalPath = canonicalKey;
    if (oldTexture) glDeleteTextures(1, &oldTexture);
    return true;
}

bool Texture::CreateSolid(unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    if (!RenderThread::IsCurrent()) {
        ARCH_LOG_ERROR("Texture::CreateSolid must run on the OpenGL render thread");
        return false;
    }
    unsigned char data[] = { r, g, b, a };

    unsigned int newTexture = 0;
    glGenTextures(1, &newTexture);
    if (newTexture == 0) return false;
    glBindTexture(GL_TEXTURE_2D, newTexture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, data);

    glBindTexture(GL_TEXTURE_2D, 0);
    const unsigned int oldTexture = m_TextureID;
    m_TextureID = newTexture;
    m_Width = 1;
    m_Height = 1;
    m_Channels = 4;
    m_Path.clear();
    m_CanonicalPath.clear();
    if (oldTexture) glDeleteTextures(1, &oldTexture);
    return true;
}

void Texture::Bind(unsigned int slot) const {
    if (!RenderThread::IsCurrent()) {
        ARCH_LOG_ERROR("Texture::Bind must run on the OpenGL render thread");
        return;
    }
#ifdef ARCHURA_DEBUG
    GLint maxUnits = 0;
    glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &maxUnits);
    if (slot >= static_cast<unsigned int>(maxUnits)) {
        ARCH_LOG_ERROR("Texture unit out of range: " + std::to_string(slot));
        return;
    }
#endif
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_2D, m_TextureID);
}

void Texture::Unbind() const {
    if (!RenderThread::IsCurrent()) return;
    glBindTexture(GL_TEXTURE_2D, 0);
}

// ==================== TextureManager ====================

TextureManager& TextureManager::Get() {
    static TextureManager instance;
    return instance;
}

Texture* TextureManager::Load(const std::string& name, const std::string& path, bool generateMipmaps) {
    return LoadShared(name, path, generateMipmaps).get();
}

std::shared_ptr<Texture> TextureManager::LoadShared(const std::string& name,
                                                    const std::string& path,
                                                    bool generateMipmaps) {
    const std::string canonicalKey = ResourceManager::Get().CanonicalAssetKey(path);
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

Texture* TextureManager::Get(const std::string& name) {
    return GetShared(name).get();
}

std::shared_ptr<Texture> TextureManager::GetShared(const std::string& name) {
    const std::string canonicalKey = ResourceManager::Get().CanonicalAssetKey(name);
    if (!canonicalKey.empty()) {
        auto canonIt = m_TexturesByCanonicalPath.find(canonicalKey);
        if (canonIt != m_TexturesByCanonicalPath.end()) {
            return canonIt->second;
        }
    }
    auto it = m_Textures.find(name);
    if (it != m_Textures.end()) {
        return it->second;
    }
    return nullptr;
}

void TextureManager::Clear() {
    m_Textures.clear();
    m_TexturesByCanonicalPath.clear();
}

} // namespace Archura
