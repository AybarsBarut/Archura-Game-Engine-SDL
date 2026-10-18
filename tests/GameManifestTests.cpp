#include "core/GameManifest.h"
#include <chrono>
#include <iostream>

int main() {
    using Archura::GameManifest;
    int failures = 0;
    auto check = [&](bool valid, const char* label) {
        if (!valid) { ++failures; std::cerr << "FAIL: " << label << '\n'; }
    };
    const std::string valid = R"({"version":1,"title":"Sample","scene":"scenes/start.scene","player_id":"10000000000000000000000000000001","content":["scenes/start.scene"]})";
    GameManifest manifest;
    check(GameManifest::Parse(valid, manifest), "valid manifest");
    for (const auto& pair : {
            std::pair<std::string, std::string>{"\"version\":1", "\"version\":2"},
            {"\"version\":1", "\"version\":true"},
            {"\"title\":\"Sample\"", "\"title\":\"\""},
            {"\"title\":\"Sample\"", "\"title\":\"Sample\",\"title\":\"Other\""},
            {"scenes/start.scene", "../start.scene"},
            {"scenes/start.scene", "C:/start.scene"},
            {"scenes/start.scene", "scenes//start.scene"},
            {"scenes/start.scene", "scenes/./start.scene"},
            {"scenes/start.scene", "scenes/start.scene."},
            {"scenes/start.scene", "scenes/NUL.scene"},
            {"scenes/start.scene", "scenes/file*.scene"},
            {"10000000000000000000000000000001", "00000000000000000000000000000000"},
            {"\"content\":[\"scenes/start.scene\"]", "\"content\":[]"},
            {"\"content\":[\"scenes/start.scene\"]", "\"content\":[\"scenes/other.scene\"]"},
            {"\"content\":[\"scenes/start.scene\"]", "\"content\":[\"scenes/start.scene\",\"Scenes/start.scene\"]"}}) {
        auto bad = valid;
        bad.replace(bad.find(pair.first), pair.first.size(), pair.second);
        check(!GameManifest::Parse(bad, manifest) && manifest.title == "Sample", "invalid manifest preserves destination");
    }
    check(!GameManifest::Parse(valid + "{}", manifest), "trailing input");
    check(!GameManifest::Parse(std::string(65537, ' '), manifest), "manifest size bound");
    auto base = std::filesystem::temp_directory_path() /
        ("archura-manifest-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(base / "scenes");
    std::ofstream(base / "game.json") << valid;
    std::string error;
    check(!GameManifest::Load(base / "game.json", manifest, error), "missing declared content rejects");
    std::ofstream(base / "scenes/start.scene") << "{}";
    check(GameManifest::Load(base / "game.json", manifest, error) && manifest.root == std::filesystem::canonical(base),
          "root anchored to manifest");
    // Exact, unique test-owned directory; never clean an inferred project path.
    if (base.parent_path() == std::filesystem::temp_directory_path() &&
        base.filename().string().find("archura-manifest-") == 0)
        std::filesystem::remove_all(base);
    return failures ? 1 : 0;
}
