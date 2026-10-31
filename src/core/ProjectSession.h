#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace Archura {

// Project identity is separate from the .gameproj scene document.
struct ProjectDescriptor {
    std::string name;
    std::filesystem::path filePath;
    std::filesystem::path rootPath;
    std::filesystem::path contentRoot;
    std::filesystem::path startScene;
};

class ProjectSession {
public:
    static bool Read(const std::filesystem::path& descriptorPath,
                     ProjectDescriptor& out, std::string& error);
    // Creates a new folder exclusively. Existing folders are never repurposed.
    static bool Create(const std::filesystem::path& newFolder, const std::string& name,
                       ProjectDescriptor& out, std::string& error);
};

class RecentProjects {
public:
    explicit RecentProjects(std::filesystem::path storagePath = {});
    bool Load(std::string& error);
    bool Record(const std::filesystem::path& descriptorPath, std::string& error);
    const std::vector<std::filesystem::path>& Entries() const noexcept { return m_Entries; }
private:
    std::filesystem::path m_StoragePath;
    std::vector<std::filesystem::path> m_Entries;
    bool m_WriteBlocked = false;
};

} // namespace Archura
