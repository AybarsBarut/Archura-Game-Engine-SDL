#include "core/ProjectSession.h"
#include "core/SerializationJson.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace {
namespace fs = std::filesystem;
int failures = 0;
#define CHECK(condition) do { if (!(condition)) { std::cerr << __LINE__ << ": failed: " << #condition << '\n'; ++failures; } } while (false)

struct TemporaryDirectory {
    fs::path path;
    TemporaryDirectory() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        const auto parent = fs::temp_directory_path();
        for (unsigned attempt = 0; attempt != 64; ++attempt) {
            const auto candidate = parent / ("archura-session-tests-" + std::to_string(stamp) + "-" + std::to_string(attempt));
            if (fs::create_directory(candidate)) { path = candidate; return; }
        }
        throw std::runtime_error("Cannot create unique test directory");
    }
    ~TemporaryDirectory() { std::error_code ec; if (!path.empty()) fs::remove_all(path, ec); }
};
void Write(const fs::path& path, const std::string& text) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file << text;
}
std::string Read(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    std::ostringstream text;
    text << file.rdbuf();
    return text.str();
}
std::string Metadata(const std::string& content = ".", const std::string& scene = "scenes/Main.gameproj") {
    return "{\"project_version\":1,\"name\":\"Fixture\",\"content_root\":\"" +
        Archura::Serialization::Escape(content) + "\",\"start_scene\":\"" + Archura::Serialization::Escape(scene) + "\"}";
}

void TestCreateAndOpen(const fs::path& base) {
    std::string error;
    Archura::ProjectDescriptor descriptor;
    const auto projectFolder = base / fs::u8path(u8"Proje_\u0130stanbul_\u65e5\u672c");
    CHECK(Archura::ProjectSession::Create(projectFolder, u8"Yeni \u00d6rnek \u65e5\u672c", descriptor, error));
    CHECK(error.empty());
    CHECK(descriptor.name == u8"Yeni \u00d6rnek \u65e5\u672c");
    CHECK(descriptor.filePath == fs::canonical(projectFolder / "project.archproj"));
    CHECK(descriptor.rootPath == fs::canonical(projectFolder));
    CHECK(descriptor.contentRoot == descriptor.rootPath);
    CHECK(descriptor.startScene == fs::canonical(projectFolder / "scenes/Main.gameproj"));
    for (const auto* folder : {"assets/models", "assets/textures", "assets/audio", "assets/scripts", "scenes"})
        CHECK(fs::is_directory(projectFolder / folder));
    const auto sceneText = Read(descriptor.startScene);
    Archura::Serialization::Json starter;
    CHECK(Archura::Serialization::JsonParser(sceneText).Parse(starter));
    CHECK(starter.Find("entities") && starter.Find("entities")->array.size() == 3);
    CHECK(sceneText.find("\"Player\"") != std::string::npos);
    CHECK(sceneText.find("\"Floor\"") != std::string::npos);
    CHECK(sceneText.find("\"Sun\"") != std::string::npos);

    const auto originalCwd = fs::current_path();
    fs::current_path(base);
    Archura::ProjectDescriptor reopened;
    CHECK(Archura::ProjectSession::Read(descriptor.filePath, reopened, error));
    CHECK(reopened.contentRoot == descriptor.contentRoot && reopened.startScene == descriptor.startScene);
    fs::current_path(originalCwd);

    const std::string before = Read(descriptor.filePath);
    Archura::ProjectDescriptor retained = descriptor;
    CHECK(!Archura::ProjectSession::Create(projectFolder, "Replacement", retained, error));
    CHECK(retained.filePath == descriptor.filePath && retained.name == descriptor.name);
    CHECK(Read(descriptor.filePath) == before && Read(descriptor.startScene) == sceneText);
    CHECK(!Archura::ProjectSession::Create(base / "missing-parent" / "nested", "Project", retained, error));
    CHECK(!fs::exists(base / "missing-parent"));
    CHECK(!Archura::ProjectSession::Create(base / "invalid", std::string(129, 'N'), retained, error));
    CHECK(!Archura::ProjectSession::Create(base / "invalid", "   ", retained, error));
    CHECK(!Archura::ProjectSession::Create(base / "invalid", std::string("bad\0name", 8), retained, error));
    CHECK(!fs::exists(base / "invalid"));
}

void TestInvalidMetadata(const fs::path& base) {
    std::string error;
    Archura::ProjectDescriptor valid;
    CHECK(Archura::ProjectSession::Create(base / "validation", "Valid", valid, error));
    const auto path = valid.filePath;
    const auto originalText = Read(path);
    const auto checkBad = [&](const std::string& metadata) {
        Write(path, metadata);
        Archura::ProjectDescriptor out = valid;
        CHECK(!Archura::ProjectSession::Read(path, out, error));
        CHECK(!error.empty());
        CHECK(out.name == valid.name && out.filePath == valid.filePath && out.contentRoot == valid.contentRoot);
        CHECK(Read(path) == metadata);
    };
    checkBad("{");
    checkBad("{\"project_version\":2,\"name\":\"Future\",\"content_root\":\".\",\"start_scene\":\"scenes/Main.gameproj\"}");
    checkBad("{\"name\":\"Missing version\",\"content_root\":\".\",\"start_scene\":\"scenes/Main.gameproj\"}");
    checkBad("{\"project_version\":1,\"name\":false,\"content_root\":\".\",\"start_scene\":\"scenes/Main.gameproj\"}");
    checkBad("{\"project_version\":1,\"name\":\"bad\\u0000name\",\"content_root\":\".\",\"start_scene\":\"scenes/Main.gameproj\"}");
    checkBad(Metadata("../"));
    checkBad(Metadata("missing-content"));
    checkBad(Metadata(".", "../outside.gameproj"));
    checkBad(Metadata(".", "scenes"));
    checkBad(Metadata(".", "scenes/missing.gameproj"));
    checkBad(Metadata(valid.rootPath.u8string()));
    checkBad(std::string(64u * 1024u + 1u, ' '));
    Write(path, originalText);

    const auto outside = base / "outside";
    fs::create_directory(outside);
    Write(outside / "outside.gameproj", "{}");
    std::error_code ec;
    fs::create_directory_symlink(outside, valid.rootPath / "linked-content", ec);
    if (ec) std::cout << "SKIP: directory symlink unavailable: " << ec.message() << '\n';
    else {
        checkBad(Metadata("linked-content"));
        checkBad(Metadata(".", "linked-content/outside.gameproj"));
    }
    Write(path, originalText);
    Archura::ProjectDescriptor out = valid;
    CHECK(!Archura::ProjectSession::Read(base / "absent.archproj", out, error));
    CHECK(out.filePath == valid.filePath);
    CHECK(!Archura::ProjectSession::Read(valid.startScene, out, error));
}

void TestRecentProjects(const fs::path& base) {
    std::string error;
    const auto prefs = base / fs::u8path(u8"prefs_\u65e5\u672c") / "recent.json";
    Archura::RecentProjects recent(prefs);
    CHECK(recent.Load(error) && recent.Entries().empty());
    const auto first = base / fs::u8path(u8"project_\u65e5\u672c.archproj");
    const auto second = base / "second.archproj";
    CHECK(recent.Record(first, error));
    CHECK(recent.Record(second, error));
    CHECK(recent.Record(base / "." / first.filename(), error));
    CHECK(recent.Entries().size() == 2 && recent.Entries().front() == fs::weakly_canonical(first));
    Archura::RecentProjects restarted(prefs);
    CHECK(restarted.Load(error));
    CHECK(restarted.Entries() == recent.Entries());
    for (int index = 0; index < 20; ++index)
        CHECK(recent.Record(base / ("recent-" + std::to_string(index) + ".archproj"), error));
    CHECK(recent.Entries().size() == 16);
    CHECK(recent.Entries().front().filename() == "recent-19.archproj");
    CHECK(recent.Entries().back().filename() == "recent-4.archproj");

    const auto retained = recent.Entries();
    const std::string corrupted = "{not a recent project list";
    Write(prefs, corrupted);
    CHECK(!recent.Load(error) && recent.Entries() == retained);
    CHECK(!recent.Record(first, error));
    CHECK(Read(prefs) == corrupted && recent.Entries() == retained);
    Write(prefs, "{\"recent_version\":1,\"projects\":[]}");
    CHECK(recent.Load(error) && recent.Entries().empty());
    CHECK(recent.Record(first, error));
    const auto lastGood = recent.Entries();
    Write(prefs, "{\"recent_version\":99,\"projects\":[]}");
    CHECK(!recent.Record(second, error));
    CHECK(recent.Entries() == lastGood && Read(prefs).find("99") != std::string::npos);

    Write(prefs, "{\"recent_version\":1,\"projects\":[\"relative.archproj\"]}");
    Archura::RecentProjects invalid(prefs);
    CHECK(!invalid.Load(error));
    Write(prefs, "{\"recent_version\":1,\"projects\":[false]}");
    CHECK(!invalid.Load(error));
    Write(prefs, std::string(64u * 1024u + 1u, ' '));
    CHECK(!invalid.Load(error));

    const auto blockedParent = base / "parent-is-a-file";
    Write(blockedParent, "sentinel");
    Archura::RecentProjects unwritable(blockedParent / "recent.json");
    CHECK(!unwritable.Record(first, error));
    CHECK(unwritable.Entries().empty() && Read(blockedParent) == "sentinel");
}
} // namespace

int main() {
    TemporaryDirectory temporary;
    TestCreateAndOpen(temporary.path);
    TestInvalidMetadata(temporary.path);
    TestRecentProjects(temporary.path);
    if (failures) return 1;
    std::cout << "Project sessions: metadata, exclusive creation, roots and persistent recents passed\n";
    return 0;
}
