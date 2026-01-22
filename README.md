# Archura Game Engine

![License](https://img.shields.io/badge/license-MIT-blue.svg) ![Platform](https://img.shields.io/badge/platform-Windows-lightgrey.svg) ![Status](https://img.shields.io/badge/status-Active%20Development-green.svg) ![C++](https://img.shields.io/badge/C++-17-blue.svg)

**Archura**, modern C++17 standartları ile geliştirilmiş, performans odaklı ve modüler bir FPS oyun motorudur. Düşük seviyeli sistem erişimi, veri odaklı tasarım (Data-Oriented Design) ve çok çekirdekli işlemci mimarilerinden tam yararlanmayı hedefleyen bir yapı üzerine kurulmuştur.

## Temel Özellikler

### Çekirdek Sistemler
*   **Veri Odaklı ECS (Entity Component System)**: Cache-friendly (önbellek dostu) ve yüksek performanslı entity yönetimi.
*   **Job System (Multithreading)**: JobSystem mimarisi ile fizik, animasyon ve render hazırlık aşamalarında tam paralel işlem gücü.
*   **Özel Bellek Yönetimi**: Yığın parçalanmasını (fragmentation) önleyen Stack ve Pool tahsisçileri (allocators).

### Grafik ve Render
*   **OpenGL 3.3+**: Modern render pipeline.
*   **Aydınlatma**: Dinamik ışıklandırma ve gölge haritalama (Shadow Mapping).
*   **Varlık Yönetimi**: `ufbx` entegrasyonu ile OBJ ve FBX formatında model desteği.

### Oyun Sistemleri
*   **Fizik**: AABB (Axis-Aligned Bounding Box) tabanlı hızlı çarpışma tespiti.
*   **Ağ (Networking)**: TCP tabanlı entegre Host/Join multiplayer mimarisi.
*   **UI**: ImGui destekli, oyun içi ayarlanabilir Geliştirici Konsolu ve Editör araçları.
*   **Ses**: MCI tabanlı, genişletilebilir ses sistemi.

## Kurulum ve Derleme

### Oyuncular İçin (Release)
En son kararlı sürümü oynamak için:
1.  **StartGame_Release.bat** dosyasını çalıştırın.
2.  Script, oyunun en güncel sürümünü otomatik olarak indirip başlatacaktır.

### Geliştiriciler İçin (Build)
Kaynak koddan derlemek için aşağıdaki gereksinimleri sağlayın:

**Gereksinimler:**
*   Visual Studio 2019 veya üzeri (C++17 desteği ile)
*   CMake 3.15+
*   Git

**Adımlar:**

Projeyi klonlayın:
```bash
git clone https://github.com/aybarsbarut/archura_game_engine.git
cd archura_game_engine
```

Derleme ve başlatma için:
```bash
StartGame_Dev.bat
```
