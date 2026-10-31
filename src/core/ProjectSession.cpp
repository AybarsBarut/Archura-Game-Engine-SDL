#include "ProjectSession.h"
#include "SerializationJson.h"
#include "AtomicFile.h"

#include <algorithm>
#include <cctype>
#include <cwctype>
#include <fstream>
#include <exception>
#include <system_error>
#include <utility>

namespace Archura {
namespace {
namespace fs = std::filesystem;
constexpr size_t MaxMetadataBytes = 64u * 1024u;
constexpr size_t MaxRecentProjects = 16;

bool PlainText(const std::string& text, size_t maxSize) {
    return !text.empty() && text.size() <= maxSize &&
        std::none_of(text.begin(), text.end(), [](unsigned char ch) { return ch < 32 || ch == 127; });
}

bool ValidName(const std::string& name) {
    if (!PlainText(name, 128) || name.find_first_not_of(' ') == std::string::npos) return false;
    Serialization::Json validation;
    return Serialization::JsonParser("\"" + Serialization::Escape(name) + "\"").Parse(validation);
}

bool PathPartEqual(const fs::path& a, const fs::path& b) {
#ifdef _WIN32
    const auto& left = a.native();
    const auto& right = b.native();
    return left.size() == right.size() && std::equal(left.begin(), left.end(), right.begin(),
        [](wchar_t x, wchar_t y) { return std::towlower(x) == std::towlower(y); });
#else
    return a == b;
#endif
}

bool Within(const fs::path& candidate, const fs::path& root) {
    auto child = candidate.begin();
    for (auto base = root.begin(); base != root.end(); ++base, ++child)
        if (child == candidate.end() || !PathPartEqual(*child, *base)) return false;
    return true;
}

bool SamePath(const fs::path& a, const fs::path& b) {
    return Within(a, b) && Within(b, a);
}

bool ExtensionIs(const fs::path& path, const std::string& expected) {
    std::string extension = path.extension().u8string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return extension == expected;
}

bool ReadText(const fs::path& path, std::string& text) {
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream) return false;
    const auto size = stream.tellg();
    if (size <= 0 || size > static_cast<std::streamoff>(MaxMetadataBytes)) return false;
    std::string content(static_cast<size_t>(size), '\0');
    stream.seekg(0);
    if (!stream.read(content.data(), static_cast<std::streamsize>(content.size()))) return false;
    text.swap(content);
    return true;
}

bool ResolveRelative(const std::string& text, const fs::path& root, fs::path& result) {
    if (!PlainText(text, 4096)) return false;
    const auto path = fs::u8path(text);
    if (path.is_absolute() || path.has_root_name() || path.has_root_directory()) return false;
    for (const auto& part : path)
        if (part == "..") return false;
    std::error_code ec;
    const auto resolved = fs::canonical(root / path, ec);
    if (ec || !Within(resolved, root)) return false;
    result = resolved;
    return true;
}

bool VersionOne(const Serialization::Json& object, const char* field) {
    const auto* version = object.Find(field);
    return object.kind == Serialization::Json::Kind::Object && version &&
        version->kind == Serialization::Json::Kind::Number && version->number == 1;
}

std::string StarterScene(const std::string& name) {
    // Scene schema v1 is intentionally accepted by the existing migration path;
    // the first normal scene save emits current persistent IDs/schema v2.
    return "{\n  \"format_version\": 1,\n  \"project\": {\"name\": \"" + Serialization::Escape(name) +
        "\", \"version\": \"1.0\", \"start_scene\": \"Main\"},\n  \"entities\": [\n"
        "    {\"name\":\"Player\",\"mesh_type\":\"cube\",\"has_mesh_renderer\":false,\"position\":[0,2,5],\"collider\":[0.6,1.8,0.6]},\n"
        "    {\"name\":\"Floor\",\"mesh_type\":\"cube\",\"has_mesh_renderer\":true,\"has_mesh\":true,\"position\":[0,-0.5,0],\"scale\":[20,1,20],\"color\":[0.65,0.68,0.72],\"collider\":[1,1,1]},\n"
        "    {\"name\":\"Sun\",\"mesh_type\":\"cube\",\"has_mesh_renderer\":false,\"rotation\":[-45,-30,0],\"light_type\":0,\"light_color\":[1,1,1],\"light_intensity\":1}\n"
        "  ]\n}\n";
}
} // namespace

bool ProjectSession::Read(const fs::path& descriptorPath, ProjectDescriptor& out, std::string& error) {
    try {
        error.clear();
        if (descriptorPath.empty() || !PlainText(descriptorPath.u8string(), 4096) ||
            !ExtensionIs(descriptorPath, ".archproj")) {
            error = "Choose a .archproj project descriptor.";
            return false;
        }
        std::error_code ec;
        const auto filePath = fs::canonical(descriptorPath, ec);
        if (ec || !fs::is_regular_file(filePath, ec) || ec) {
            error = "The project descriptor does not exist or cannot be read.";
            return false;
        }
        std::string content;
        Serialization::Json root;
        if (!ReadText(filePath, content) || !Serialization::JsonParser(content).Parse(root) ||
            !VersionOne(root, "project_version")) {
            error = "Invalid project metadata or unsupported project version.";
            return false;
        }
        ProjectDescriptor candidate;
        std::string contentRoot, startScene;
        if (!Serialization::StringField(root, "name", candidate.name, true) || !ValidName(candidate.name) ||
            !Serialization::StringField(root, "content_root", contentRoot, true) ||
            !Serialization::StringField(root, "start_scene", startScene, true)) {
            error = "The project needs a name, content_root and start_scene.";
            return false;
        }
        candidate.filePath = filePath;
        candidate.rootPath = filePath.parent_path();
        if (!ResolveRelative(contentRoot, candidate.rootPath, candidate.contentRoot) ||
            !fs::is_directory(candidate.contentRoot, ec) || ec ||
            !ResolveRelative(startScene, candidate.rootPath, candidate.startScene) ||
            !ExtensionIs(candidate.startScene, ".gameproj") ||
            !fs::is_regular_file(candidate.startScene, ec) || ec) {
            error = "Content root and start scene must exist inside the project folder.";
            return false;
        }
        out = std::move(candidate);
        return true;
    } catch (const std::exception& exception) {
        error = std::string("Cannot read the project: ") + exception.what();
        return false;
    }
}

bool ProjectSession::Create(const fs::path& newFolder, const std::string& name,
                            ProjectDescriptor& out, std::string& error) {
    fs::path ownedRoot;
    fs::path ownedParent;
    bool published = false;
    struct Cleanup {
        fs::path& root;
        fs::path& parent;
        bool& published;
        ~Cleanup() {
            // Only the exact folder exclusively created by this operation is owned.
            // Do not follow a substituted root symlink or remove its parent.
            if (published || root.empty() || parent.empty() || root.parent_path() != parent) return;
            std::error_code ec;
            const auto actual = fs::canonical(root, ec);
            if (ec || !SamePath(actual, root) || !Within(actual, parent) || SamePath(actual, parent)) return;
            fs::remove_all(root, ec);
        }
    } cleanup{ownedRoot, ownedParent, published};
    try {
        error.clear();
        if (newFolder.empty() || !PlainText(newFolder.u8string(), 4096) || !ValidName(name)) {
            error = "Enter a new project folder and a project name of at most 128 bytes.";
            return false;
        }
        std::error_code ec;
        const auto requested = fs::absolute(newFolder, ec).lexically_normal();
        if (ec || requested.filename().empty()) {
            error = "Cannot resolve the new project folder.";
            return false;
        }
        const auto parent = fs::canonical(requested.parent_path(), ec);
        if (ec || !fs::is_directory(parent, ec) || ec) {
            error = "Choose an existing parent directory for the new project.";
            return false;
        }
        const auto target = parent / requested.filename();
        if (!fs::create_directory(target, ec) || ec) {
            error = "The project folder already exists or cannot be created. Choose a new folder.";
            return false;
        }
        ownedRoot = target;
        ownedParent = parent;
        for (const auto* directory : {"assets/models", "assets/textures", "assets/audio", "assets/scripts", "scenes"}) {
            fs::create_directories(target / directory, ec);
            if (ec) { error = "Cannot create the project content folders."; return false; }
        }
        const auto descriptorPath = target / "project.archproj";
        const std::string descriptor = "{\n  \"project_version\": 1,\n  \"name\": \"" + Serialization::Escape(name) +
            "\",\n  \"content_root\": \".\",\n  \"start_scene\": \"scenes/Main.gameproj\"\n}\n";
        if (!Serialization::AtomicWritePath(target / "scenes/Main.gameproj", StarterScene(name)) ||
            !Serialization::AtomicWritePath(descriptorPath, descriptor)) {
            error = "Cannot write the starter scene or project descriptor.";
            return false;
        }
        ProjectDescriptor candidate;
        if (!Read(descriptorPath, candidate, error)) return false;
        out = std::move(candidate);
        published = true;
        return true;
    } catch (const std::exception& exception) {
        error = std::string("Cannot create the project: ") + exception.what();
        return false;
    }
}

RecentProjects::RecentProjects(fs::path storagePath) : m_StoragePath(std::move(storagePath)) {}

bool RecentProjects::Load(std::string& error) {
    const auto fail = [&](const char* message) { m_WriteBlocked = true; error = message; return false; };
    try {
        error.clear();
        if (m_StoragePath.empty()) return fail("Recent project storage is not configured.");
        std::error_code ec;
        const bool exists = fs::exists(m_StoragePath, ec);
        if (ec) return fail("Cannot inspect the recent project list.");
        if (!exists) {
            m_Entries.clear();
            m_WriteBlocked = false;
            return true;
        }
        std::string content;
        Serialization::Json root;
        if (!ReadText(m_StoragePath, content) || !Serialization::JsonParser(content).Parse(root) ||
            !VersionOne(root, "recent_version")) return fail("Invalid recent project list; the existing file was retained.");
        const auto* entries = root.Find("projects");
        if (!entries || entries->kind != Serialization::Json::Kind::Array || entries->array.size() > MaxRecentProjects)
            return fail("Invalid recent project entries; the existing file was retained.");
        std::vector<fs::path> candidate;
        for (const auto& entry : entries->array) {
            if (entry.kind != Serialization::Json::Kind::String || !PlainText(entry.string, 4096))
                return fail("Invalid path in the recent project list.");
            const auto path = fs::u8path(entry.string);
            if (!path.is_absolute() || !ExtensionIs(path, ".archproj")) return fail("Recent project paths must be absolute .archproj files.");
            const auto canonical = fs::weakly_canonical(path, ec);
            if (ec) return fail("Cannot resolve a recent project path.");
            if (std::none_of(candidate.begin(), candidate.end(), [&](const auto& existing) { return SamePath(existing, canonical); }))
                candidate.push_back(canonical);
        }
        m_Entries.swap(candidate);
        m_WriteBlocked = false;
        return true;
    } catch (const std::exception& exception) {
        m_WriteBlocked = true;
        error = std::string("Cannot read recent projects: ") + exception.what();
        return false;
    }
}

bool RecentProjects::Record(const fs::path& descriptorPath, std::string& error) {
    try {
        error.clear();
        if (m_WriteBlocked) { error = "Recent project updates are disabled until the list is loaded successfully."; return false; }
        // Reload before writing to merge external updates and avoid replacing a
        // preferences file that became malformed since the editor last read it.
        RecentProjects stored(m_StoragePath);
        if (!stored.Load(error)) { m_WriteBlocked = true; return false; }
        if (descriptorPath.empty() || !PlainText(descriptorPath.u8string(), 4096) || !ExtensionIs(descriptorPath, ".archproj")) {
            error = "Cannot record an invalid project descriptor path.";
            return false;
        }
        std::error_code ec;
        const auto absolute = fs::absolute(descriptorPath, ec);
        if (ec) { error = "Cannot resolve the recent project path."; return false; }
        const auto path = fs::weakly_canonical(absolute, ec);
        if (ec) { error = "Cannot resolve the recent project path."; return false; }
        std::vector<fs::path> candidate{path};
        for (const auto& existing : stored.Entries()) {
            if (!SamePath(existing, path)) candidate.push_back(existing);
            if (candidate.size() == MaxRecentProjects) break;
        }
        std::string content = "{\n  \"recent_version\": 1,\n  \"projects\": [";
        for (size_t index = 0; index < candidate.size(); ++index) {
            if (index) content += ',';
            content += "\n    \"" + Serialization::Escape(candidate[index].u8string()) + "\"";
        }
        content += "\n  ]\n}\n";
        if (content.size() > MaxMetadataBytes || !Serialization::AtomicWritePath(m_StoragePath, content)) {
            error = "Cannot save recent projects; the previous list was retained.";
            return false;
        }
        m_Entries.swap(candidate);
        return true;
    } catch (const std::exception& exception) {
        error = std::string("Cannot update recent projects: ") + exception.what();
        return false;
    }
}

} // namespace Archura
