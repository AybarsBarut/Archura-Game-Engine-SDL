#pragma once

#include <string>
#include <vector>
#include <string_view>

namespace Archura {

class Scene;

/**
 * @brief Kaydedilmiş bir oyun projesinin meta verisi
 *
 * Proje adı kullanıcı tarafından belirlenir.
 * Dosya adı: saves/<projectName>.scene
 */
struct ProjectSave {
    std::string name;       // Kullanıcının verdiği proje adı ("Benim FPS Oyunum")
    std::string filePath;   // "saves/BenimFPSOyunum.scene"
    std::string timestamp;  // "2026-02-27 11:30:00"
    int entityCount = 0;    // Sahne içindeki kullanıcı entity sayısı (bilgi amaçlı)
    bool valid = false;     // Dosya diskte mevcut mu?
};

/**
 * @brief GameSaveManager – Proje kayıt/yükleme sistemi
 *
 * Singleton. Oyun motorunda farklı projeler oluşturulup kaydedilebilir.
 * Kayıt odağı: kullanıcının tasarladığı sahne (entity'ler, mesh tipi,
 *              renkler, texture yolları, ışıklar) – oyuncu konumu değil!
 *
 * Dosyalar:  saves/<ProjectName>.scene  (JSON formatı)
 * İsim kuralı: alfanumerik + boşluk, max 64 karakter
 */
class GameSaveManager {
public:
    static GameSaveManager& Get() {
        static GameSaveManager instance;
        return instance;
    }

    // Diskteki kayıtlı projeleri tara (saves/*.scene)
    void RefreshProjects();

    // Projeyi kaydet (projectName boşsa "Unnamed" kullanılır)
    bool SaveProject(const std::string& projectName, Scene* scene);

    // Proje yükle (dosya adıyla)
    bool LoadProject(const std::string& filePath, Scene* scene);

    // Proje sil
    bool DeleteProject(const std::string& filePath);

    // Kayıtlı proje listesi
    const std::vector<ProjectSave>& GetProjects() const { return m_Projects; }

    // Dosya adı oluştur (isimden güvenli dosya adı türet)
    static std::string MakeSafeFileName(std::string_view name);

private:
    GameSaveManager();
    ~GameSaveManager() = default;
    GameSaveManager(const GameSaveManager&) = delete;
    GameSaveManager& operator=(const GameSaveManager&) = delete;

    std::string GetTimestamp() const;
    static std::string EscapeJson(std::string_view s);
    static std::string ExtractJsonField(std::string_view json, std::string_view key);

    std::vector<ProjectSave> m_Projects;
};

} // namespace Archura
