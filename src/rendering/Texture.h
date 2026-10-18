#pragma once

#include <string>
#include <unordered_map>
#include <memory>

namespace Archura {

/**
 * @brief Texture sınıfı - 2D texture yönetimi
 * 
 * stb_image ile texture loading, OpenGL texture oluşturma
 */
class Texture {
public:
    Texture() = default;
    ~Texture();

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;

    // Texture'ı dosyadan yükle
    bool LoadFromFile(const std::string& path, bool generateMipmaps = true,
                      bool srgb = false);
    
    // Solid color texture oluştur
    bool CreateSolid(unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255);

    void Bind(unsigned int slot = 0) const;
    void Unbind() const;

    unsigned int GetID() const { return m_TextureID; }
    int GetWidth() const { return m_Width; }
    int GetHeight() const { return m_Height; }
    int GetChannels() const { return m_Channels; }

    bool IsLoaded() const { return m_TextureID != 0; }
    const std::string& GetPath() const { return m_Path; }
    const std::string& GetCanonicalPath() const { return m_CanonicalPath; }
    void SetPath(const std::string& path) { m_Path = path; }
    void SetCanonicalPath(const std::string& canonicalPath) { m_CanonicalPath = canonicalPath; }

private:
    void Release() noexcept;

    unsigned int m_TextureID = 0;
    int m_Width = 0;
    int m_Height = 0;
    int m_Channels = 0;
    std::string m_Path;
    std::string m_CanonicalPath;
};

/**
 * @brief TextureManager - Texture caching ve resource management
 */
class TextureManager {
public:
    static TextureManager& Get();

    // Texture yükle ve cache'le
    Texture* Load(const std::string& name, const std::string& path, bool generateMipmaps = true);
    std::shared_ptr<Texture> LoadShared(const std::string& name, const std::string& path, bool generateMipmaps = true);
    
    // Cache'den texture al
    Texture* Get(const std::string& name);
    std::shared_ptr<Texture> GetShared(const std::string& name);
    
    // Tüm texture'ları temizle
    void Clear();

private:
    TextureManager() = default;
    ~TextureManager() { Clear(); }

    TextureManager(const TextureManager&) = delete;
    TextureManager& operator=(const TextureManager&) = delete;

private:
    std::unordered_map<std::string, std::shared_ptr<Texture>> m_Textures;
    std::unordered_map<std::string, std::shared_ptr<Texture>> m_TexturesByCanonicalPath;
};
} // namespace Archura
