#pragma once

#include <string>
#include <vector>
#include <filesystem>

namespace Archura {

class Scene;

/**
 * @brief GameBuilderPanel – Oyun içi asset ekleme paneli
 *
 * Editor'a entegre çalışır. Üç sekme:
 *   - Objects  : Sahneye primitive veya .obj model spawn eder
 *   - Textures : assets/textures/ altındaki dosyaları seçili entity'ye atar
 *   - Audio    : assets/audio/ altındaki ses dosyalarını listeler
 */
class GameBuilderPanel {
public:
    GameBuilderPanel()  = default;
    ~GameBuilderPanel() = default;

    void Draw(Scene* scene, class Entity* selectedEntity);

    bool IsOpen()  const { return m_Open; }
    void SetOpen(bool v) { m_Open = v; }
    void ToggleOpen()    { m_Open = !m_Open; }

private:
    void DrawObjectsTab(Scene* scene);
    void DrawTexturesTab(Scene* scene, class Entity* selectedEntity);
    void DrawAudioTab();

    void RefreshTextureList();
    void RefreshAudioList();
    void RefreshModelList();

    bool m_Open = false;

    // Cached asset lists
    std::vector<std::string> m_TextureFiles;
    std::vector<std::string> m_AudioFiles;
    std::vector<std::string> m_ModelFiles;

    bool m_TexturesDirty = true;
    bool m_AudioDirty    = true;
    bool m_ModelsDirty   = true;

    int m_SelectedTextureIdx = -1;
    int m_SelectedAudioIdx   = -1;

    // Tab state
    int m_ActiveTab = 0; // 0=Objects, 1=Textures, 2=Audio
};

} // namespace Archura
