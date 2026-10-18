#pragma once

#include <vector>
#include <string>
#include <glm/glm.hpp>
#include <memory>

namespace Archura {

class Shader;

/**
 * @brief Vertex yapısı - mesh'in her bir vertex'i için data
 */
#define MAX_BONE_INFLUENCE 4

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoords;
    glm::vec3 color;
    
    // Animation
    int m_BoneIDs[MAX_BONE_INFLUENCE] = {0};
    float m_Weights[MAX_BONE_INFLUENCE] = {0.0f};

    Vertex(
        const glm::vec3& pos = glm::vec3(0.0f),
        const glm::vec3& norm = glm::vec3(0.0f, 1.0f, 0.0f),
        const glm::vec2& tex = glm::vec2(0.0f),
        const glm::vec3& col = glm::vec3(1.0f)
    ) : position(pos), normal(norm), texCoords(tex), color(col) {}
};

/**
 * @brief Mesh sınıfı - 3D geometri yönetimi
 * 
 * VAO/VBO/EBO ile optimize edilmiş rendering
 */
// Persist the recipe used to build a mesh instead of inferring its identity from
// vertex counts. Custom meshes deliberately have no reconstruction recipe.
enum class MeshSourceType { Custom, Cube, Plane, Sphere, Capsule, Stairs, Ramp, OBJ, FBX };

struct MeshSource {
    MeshSourceType type = MeshSourceType::Custom;
    float size = 1.0f;
    float width = 10.0f;
    float height = 10.0f;
    float depth = 1.0f;
    float uvScale = 1.0f;
    float radius = 1.0f;
    int segments = 32;
    int steps = 5;
    std::string path;
};

class Mesh {
public:
    Mesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices);
    ~Mesh();

    // Copy constructor ve assignment operator'ı disable et
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    Mesh(Mesh&& other) noexcept;
    Mesh& operator=(Mesh&& other) noexcept;

    void Draw(Shader* shader);
    void DrawInstanced(Shader* shader, const std::vector<glm::mat4>& models);

    // Procedural mesh oluşturucular
    static Mesh* CreateCube(float size = 1.0f);
    static Mesh* CreatePlane(float width = 10.0f, float height = 10.0f, float uvScale = 1.0f);
    static Mesh* CreateSphere(float radius = 1.0f, int segments = 32);
    static Mesh* CreateCapsule(float radius = 0.5f, float height = 2.0f);
    static Mesh* CreateStairs(float width = 1.0f, float height = 1.0f, float depth = 1.0f, int steps = 5);
    static Mesh* CreateRamp(float width = 1.0f, float height = 1.0f, float depth = 1.0f);
    
    const MeshSource& GetSource() const noexcept { return m_Source; }
    // Returns nullptr for custom or invalid recipes and unavailable model files.
    static std::shared_ptr<Mesh> CreateFromSource(const MeshSource& source);

    // Model Loader
    static Mesh* LoadFromOBJ(const std::string& path);
    static Mesh* LoadFromFBX(const std::string& path);

    // Preferred C++17 ownership API. Raw factories above remain for source
    // compatibility, but callers that attach a mesh to an entity should retain
    // one of these handles through MeshRenderer::SetMeshAsset().
    static std::shared_ptr<Mesh> CreateCubeShared(float size = 1.0f);
    static std::shared_ptr<Mesh> CreatePlaneShared(float width = 10.0f, float height = 10.0f, float uvScale = 1.0f);
    static std::shared_ptr<Mesh> CreateSphereShared(float radius = 1.0f, int segments = 32);
    static std::shared_ptr<Mesh> CreateCapsuleShared(float radius = 0.5f, float height = 2.0f);
    static std::shared_ptr<Mesh> CreateStairsShared(float width = 1.0f, float height = 1.0f, float depth = 1.0f, int steps = 5);
    static std::shared_ptr<Mesh> CreateRampShared(float width = 1.0f, float height = 1.0f, float depth = 1.0f);
    static std::shared_ptr<Mesh> LoadFromOBJShared(const std::string& path);
    static std::shared_ptr<Mesh> LoadFromFBXShared(const std::string& path);

private:
    void Release() noexcept;
    void SetupMesh();
    void SetupInstancedAttributes();

public:
    // Dynamic Mesh Modification
    // Granting mutable access conservatively invalidates the reconstruction
    // recipe, so edited geometry cannot later reload as its original shape.
    std::vector<Vertex>& GetVertices() { m_Source = MeshSource{}; return m_Vertices; }
    const std::vector<Vertex>& GetVertices() const { return m_Vertices; }
    const std::vector<unsigned int>& GetIndices() const { return m_Indices; }
    
    void UpdateVertices(); // Re-upload vertices to GPU
    void RecalculateNormals(); // Recalculate normals based on current positions

private:
    std::vector<Vertex> m_Vertices;
    std::vector<unsigned int> m_Indices;

    MeshSource m_Source;

    unsigned int m_VAO = 0;  // Vertex Array Object
    unsigned int m_VBO = 0;  // Vertex Buffer Object
    unsigned int m_EBO = 0;  // Element Buffer Object
    size_t m_VertexCapacityBytes = 0;
    
    unsigned int m_InstanceVBO = 0; // For instanced rendering
    size_t m_InstanceCapacity = 0;  // To avoid reallocating VBO constantly
    bool m_InstancedSetup = false;
};

} // namespace Archura
