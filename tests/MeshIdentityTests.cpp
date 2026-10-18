#include "rendering/Mesh.h"
#include "rendering/RenderThread.h"
#include "core/Logger.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <utility>

using namespace Archura;

namespace {
void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void CheckReconstruction(const std::shared_ptr<Mesh>& original, MeshSourceType type) {
    Require(original != nullptr, "mesh factory failed");
    Require(original->GetSource().type == type, "factory lost mesh type");
    auto rebuilt = Mesh::CreateFromSource(original->GetSource());
    Require(rebuilt != nullptr, "mesh recipe reconstruction failed");
    Require(original->GetIndices() == rebuilt->GetIndices(), "recipe changed indices");
    const auto& a = static_cast<const Mesh&>(*original).GetVertices();
    const auto& b = static_cast<const Mesh&>(*rebuilt).GetVertices();
    Require(a.size() == b.size(), "recipe changed vertex count");
    for (size_t i = 0; i < a.size(); ++i) {
        Require(a[i].position == b[i].position && a[i].normal == b[i].normal &&
                a[i].texCoords == b[i].texCoords && a[i].color == b[i].color,
                "recipe changed vertex data");
    }
}

struct TempDirectory {
    std::filesystem::path path = std::filesystem::temp_directory_path() /
        ("archura_mesh_identity_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    TempDirectory() { std::filesystem::create_directories(path); }
    ~TempDirectory() { std::error_code ec; std::filesystem::remove_all(path, ec); }
};
}

int main() {
    try {
        // No render owner is attached: factories retain CPU geometry without
        // calling any OpenGL function, allowing this test to run without a GPU.
        RenderThread::Detach();
        Logger::SetEnabled(false);
        CheckReconstruction(Mesh::CreateCubeShared(2.5f), MeshSourceType::Cube);
        auto plane = Mesh::CreatePlaneShared(3.0f, 5.0f, 7.0f);
        Require(plane->GetSource().width == 3.0f && plane->GetSource().height == 5.0f &&
                plane->GetSource().uvScale == 7.0f, "plane recipe lost dimensions or UV scale");
        CheckReconstruction(plane, MeshSourceType::Plane);
        auto sphere = Mesh::CreateSphereShared(2.25f, 18);
        Require(sphere->GetSource().radius == 2.25f && sphere->GetSource().segments == 18,
                "sphere recipe lost radius or segments");
        CheckReconstruction(sphere, MeshSourceType::Sphere);
        CheckReconstruction(Mesh::CreateCapsuleShared(0.75f, 3.25f), MeshSourceType::Capsule);
        CheckReconstruction(Mesh::CreateStairsShared(2.0f, 3.0f, 4.0f, 7), MeshSourceType::Stairs);
        CheckReconstruction(Mesh::CreateRampShared(2.0f, 3.0f, 4.0f), MeshSourceType::Ramp);

        Mesh moved(std::move(*plane));
        Require(moved.GetSource().type == MeshSourceType::Plane && moved.GetSource().uvScale == 7.0f,
                "move construction lost source");
        Require(plane->GetSource().type == MeshSourceType::Custom, "moved-from mesh kept stale source");
        Mesh assigned({}, {});
        assigned = std::move(moved);
        Require(assigned.GetSource().type == MeshSourceType::Plane, "move assignment lost source");

        TempDirectory temp;
        auto objPath = temp.path / "triangle model.obj";
        {
            std::ofstream obj(objPath);
            obj << "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n";
        }
        auto imported = Mesh::LoadFromOBJShared(objPath.string());
        Require(imported && imported->GetSource().path == objPath.string(), "OBJ source path lost");
        CheckReconstruction(imported, MeshSourceType::OBJ);

        auto fbxPath = temp.path / "triangle model.fbx";
        {
            std::ofstream fbx(fbxPath);
            fbx << R"FBX(; FBX 7.4.0 project file
FBXHeaderExtension: { FBXVersion: 7400 }
Objects: {
    Geometry: 1, "Geometry::Triangle", "Mesh" {
        Vertices: *9 { a: 0,0,0,1,0,0,0,1,0 }
        PolygonVertexIndex: *3 { a: 0,1,-3 }
    }
    Model: 2, "Model::Triangle", "Mesh" { }
}
Connections: {
    C: "OO",1,2
    C: "OO",2,0
}
)FBX";
        }
        auto fbx = Mesh::LoadFromFBXShared(fbxPath.string());
        Require(fbx && fbx->GetSource().path == fbxPath.string(), "FBX source path lost");
        CheckReconstruction(fbx, MeshSourceType::FBX);

        auto edited = Mesh::CreateCubeShared(2.0f);
        auto& editableVertices = edited->GetVertices();
        editableVertices[0].position.x += 1.0f;
        Require(edited->GetSource().type == MeshSourceType::Custom,
                "mutable geometry retained a stale primitive recipe");
        Require(!Mesh::CreateFromSource(edited->GetSource()), "edited mesh reconstructed original shape");
        auto renormalized = Mesh::CreateCubeShared();
        renormalized->RecalculateNormals();
        Require(renormalized->GetSource().type == MeshSourceType::Custom,
                "normal edits retained a stale recipe");
        auto uploaded = Mesh::CreateCubeShared();
        uploaded->UpdateVertices();
        Require(uploaded->GetSource().type == MeshSourceType::Custom,
                "vertex updates retained a stale recipe");

        Mesh custom({}, {});
        Require(custom.GetSource().type == MeshSourceType::Custom, "custom mesh acquired a primitive identity");
        Require(!Mesh::CreateFromSource(custom.GetSource()), "custom recipe fabricated geometry");
        MeshSource bad;
        bad.type = MeshSourceType::Sphere;
        bad.segments = 0;
        Require(!Mesh::CreateFromSource(bad), "invalid sphere recipe accepted");
        bad.segments = std::numeric_limits<int>::max();
        Require(!Mesh::CreateFromSource(bad), "unbounded sphere recipe accepted");
        bad.segments = 16;
        bad.radius = std::numeric_limits<float>::quiet_NaN();
        Require(!Mesh::CreateFromSource(bad), "nonfinite recipe accepted");
        bad.type = MeshSourceType::OBJ;
        bad.path = (temp.path / "missing.obj").string();
        Require(!Mesh::CreateFromSource(bad), "missing OBJ fabricated geometry");
        bad.type = MeshSourceType::FBX;
        bad.path.clear();
        Require(!Mesh::CreateFromSource(bad), "empty FBX path accepted");
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
