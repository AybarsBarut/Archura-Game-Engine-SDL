#pragma once

#include "../ecs/Entity.h"
#include "../rendering/Camera.h"
#include "../rendering/Mesh.h"
#include "../ecs/Component.h"
#include <vector>
#include <string>
#include <glm/glm.hpp>

namespace Archura {

class Scene;

class ObjectManipulationTool {
public:
    enum class Mode {
        None,
        Terrain,
        ObjectDeformation
    };

    enum class TerrainAction {
        Raise,
        Lower,
        Flatten,
        Smooth
    };

    enum class DeformationType {
        Twist,
        Bend,
        Taper
    };

    struct HistoryState {
        std::shared_ptr<Mesh> targetMesh;
        std::vector<Vertex> vertices;
    };

public:
    ObjectManipulationTool();
    ~ObjectManipulationTool();

    void Init();
    void Draw(Scene* scene, Camera* camera, Entity* selectedEntity);
    void OnSceneGUI(Scene* scene, Camera* camera, Entity* selectedEntity);

    // Visibility
    bool IsOpen() const { return m_IsOpen; }
    void SetOpen(bool open) { m_IsOpen = open; }

private:
    void RenderTerrainModeUI();
    void RenderObjectModeUI();
    
    // Logic
    bool RayIntersectsMesh(const glm::vec3& rayOrigin, const glm::vec3& rayDir,
                          Mesh* mesh, const glm::mat4& modelMatrix, float& outT, glm::vec3& outHitPoint);
    
    // Visualization
    void DrawBrushCursor(Camera* camera, const glm::vec3& position, const glm::vec3& normal, float radius);

    // Terrain Ops
    void ApplyTerrainBrush(Mesh* mesh, const glm::vec3& hitPoint, const glm::mat4& modelMatrix);

    // Object Deform Ops
    void ApplyDeformation(Mesh* mesh, DeformationType type, float value, int axis);
    void RestoreOriginalMesh(Mesh* mesh); // To apply deformation from base state if needed, or we incremental? 
                                          // Incremental is risky for sliders. 
                                          // Better: Store "Base Vertex State" when selecting an entity?
                                          // For now, let's just do incremental but be careful, or maybe just 
                                          // allow the user to "Commit" or "Reset".

    // Undo/Redo
    void PushHistory(const std::shared_ptr<Mesh>& mesh);
    void Undo();

private:
    bool m_IsOpen = false;
    Mode m_CurrentMode = Mode::Terrain;
    
    // Terrain Settings
    TerrainAction m_TerrainAction = TerrainAction::Raise;
    float m_BrushRadius = 2.0f;
    float m_BrushStrength = 0.5f;

    // Object Deform Settings
    DeformationType m_DeformationType = DeformationType::Twist;
    float m_DeformationValue = 0.0f;
    int m_DeformationAxis = 1; // 0=X, 1=Y, 2=Z

    // History
    std::vector<HistoryState> m_UndoStack;
    
    // Internal State
    bool m_IsPainting = false;
    std::shared_ptr<Mesh> m_CurrentMesh;
    std::vector<Vertex> m_BaseMeshState; // State when deformation started (for slider-based non-destructive editing)
    bool m_IsDeforming = false; // Is dragging a slider?
};

} // namespace Archura
