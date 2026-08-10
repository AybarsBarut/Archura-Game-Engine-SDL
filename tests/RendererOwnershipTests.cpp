#include "rendering/Mesh.h"
#include "rendering/PostProcess.h"
#include "rendering/Shader.h"
#include "rendering/Texture.h"
#include "ecs/Component.h"

#include <cassert>
#include <cstdint>
#include <memory>
#include <type_traits>

static_assert(!std::is_copy_constructible_v<Archura::Shader>);
static_assert(!std::is_copy_assignable_v<Archura::Shader>);
static_assert(std::is_nothrow_move_constructible_v<Archura::Shader>);
static_assert(std::is_nothrow_move_assignable_v<Archura::Shader>);

static_assert(!std::is_copy_constructible_v<Archura::Texture>);
static_assert(!std::is_copy_assignable_v<Archura::Texture>);
static_assert(std::is_nothrow_move_constructible_v<Archura::Texture>);
static_assert(std::is_nothrow_move_assignable_v<Archura::Texture>);

static_assert(!std::is_copy_constructible_v<Archura::Mesh>);
static_assert(!std::is_copy_assignable_v<Archura::Mesh>);
static_assert(std::is_nothrow_move_constructible_v<Archura::Mesh>);
static_assert(std::is_nothrow_move_assignable_v<Archura::Mesh>);

static_assert(!std::is_copy_constructible_v<Archura::PostProcess>);
static_assert(!std::is_move_constructible_v<Archura::PostProcess>);

int main() {
    auto mesh = std::shared_ptr<Archura::Mesh>(
        reinterpret_cast<Archura::Mesh*>(static_cast<uintptr_t>(1)),
        [](Archura::Mesh*) {});
    auto texture = std::shared_ptr<Archura::Texture>(
        reinterpret_cast<Archura::Texture*>(static_cast<uintptr_t>(2)),
        [](Archura::Texture*) {});
    std::weak_ptr<Archura::Mesh> meshLifetime = mesh;

    Archura::MeshRenderer original;
    original.SetMeshAsset(mesh);
    original.SetTextureAsset(texture);
    assert(original.GetMesh() == mesh.get());
    assert(original.GetTexture() == texture.get());

    Archura::MeshRenderer clipboardCopy = original;
    original.ClearMeshAsset();
    mesh.reset();
    assert(!meshLifetime.expired());
    assert(clipboardCopy.GetMesh() != nullptr);

    clipboardCopy.ClearMeshAsset();
    assert(meshLifetime.expired());
    return 0;
}
