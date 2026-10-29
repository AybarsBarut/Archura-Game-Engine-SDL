#include "ObjectManipulationTool.h"
#include "../core/Application.h"
#include "../core/Engine.h"
#include "../game/RenderSystem.h"
#include <algorithm>
#include <cmath>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <imgui.h>

namespace Archura {

ObjectManipulationTool::ObjectManipulationTool() {}
ObjectManipulationTool::~ObjectManipulationTool() {}

void ObjectManipulationTool::Init() {
  // Initialize if needed
}

void ObjectManipulationTool::Draw(Scene *scene, Camera *camera,
                                  Entity *selectedEntity) {
  if (selectedEntity && (!scene || !scene->OwnsEntityPointer(selectedEntity))) {
    selectedEntity = nullptr;
  }
  (void)camera;
  if (!m_IsOpen)
    return;

  if (ImGui::Begin("Object Manipulation Tool", &m_IsOpen)) {

    // Mode Selection
    const char *modes[] = {"None", "Terrain / Sculpt", "Object Modifier"};
    int mode = (int)m_CurrentMode;
    if (ImGui::Combo("Mode", &mode, modes, 3)) {
      m_CurrentMode = (Mode)mode;
      // Mode changed, reset state
      m_IsPainting = false;
      m_IsDeforming = false;
      m_CurrentMesh.reset();
    }

    ImGui::Separator();

    if (selectedEntity) {
      MeshRenderer *mr = selectedEntity->GetComponent<MeshRenderer>();
      if (mr && mr->meshAsset) {
        if (m_CurrentMesh != mr->meshAsset) {
          m_CurrentMesh = mr->meshAsset;
          // New mesh selected, clear base state
          m_BaseMeshState.clear();
        }

        if (m_CurrentMode == Mode::Terrain) {
          RenderTerrainModeUI();
        } else if (m_CurrentMode == Mode::ObjectDeformation) {
          RenderObjectModeUI();
        }
      } else {
        ImGui::TextColored(ImVec4(1, 1, 0, 1), "Selected entity has no mesh.");
        m_CurrentMesh.reset();
      }
    } else {
      ImGui::TextDisabled("Select an entity to manipulate.");
      m_CurrentMesh.reset();
    }
  }
  ImGui::End();
}

void ObjectManipulationTool::OnSceneGUI(
    Scene *scene, Entity *selectedEntity, const glm::mat4& view,
    const glm::mat4& projection, const glm::vec2& viewportOrigin,
    const glm::vec2& viewportSize, bool hovered, ImDrawList* drawList) {

  // Toggle Isolation
  if (selectedEntity && (!scene || !scene->OwnsEntityPointer(selectedEntity))) {
    selectedEntity = nullptr;
  }
  if (!selectedEntity) {
    m_IsPainting = false;
    m_IsDeforming = false;
    m_CurrentMesh.reset();
  }
  Application *app = Application::TryGet();
  RenderSystem *renderSystem = app ? app->GetRenderSystem() : nullptr;
  if (m_IsOpen && selectedEntity) {
    if (renderSystem)
      renderSystem->SetIsolatedEntity(selectedEntity);
  } else {
    if (renderSystem)
      renderSystem->SetIsolatedEntity(nullptr);
  }

  if (!m_IsOpen || m_CurrentMode != Mode::Terrain || !selectedEntity ||
      !m_CurrentMesh || !hovered || viewportSize.x <= 0.0f || viewportSize.y <= 0.0f) {
    m_IsPainting = false;
    return;
  }

  // ImGui captures the Scene image's mouse too, so the caller's image hover
  // state replaces WantCaptureMouse. Use the exact image rectangle in logical
  // screen coordinates, independently of window decorations and framebuffer DPI.
  const ImVec2 mouse = ImGui::GetMousePos();
  const float x = 2.0f * (mouse.x - viewportOrigin.x) / viewportSize.x - 1.0f;
  const float y = 1.0f - 2.0f * (mouse.y - viewportOrigin.y) / viewportSize.y;
  if (x < -1.0f || x > 1.0f || y < -1.0f || y > 1.0f) {
    m_IsPainting = false;
    return;
  }
  const glm::mat4 inverseViewProjection = glm::inverse(projection * view);
  const glm::vec4 nearHomogeneous = inverseViewProjection * glm::vec4(x, y, -1.0f, 1.0f);
  const glm::vec4 farHomogeneous = inverseViewProjection * glm::vec4(x, y, 1.0f, 1.0f);
  if (!std::isfinite(nearHomogeneous.w) || !std::isfinite(farHomogeneous.w) ||
      std::abs(nearHomogeneous.w) < 1e-7f || std::abs(farHomogeneous.w) < 1e-7f) {
    m_IsPainting = false;
    return;
  }
  const glm::vec3 rayOrigin = glm::vec3(nearHomogeneous) / nearHomogeneous.w;
  const glm::vec3 direction = glm::vec3(farHomogeneous) / farHomogeneous.w - rayOrigin;
  const float lengthSquared = glm::dot(direction, direction);
  if (!std::isfinite(lengthSquared) || lengthSquared <= 1e-12f) {
    m_IsPainting = false;
    return;
  }
  const glm::vec3 rayDir = direction / std::sqrt(lengthSquared);

  // Use World Transform
  glm::mat4 model = selectedEntity->GetWorldTransform();

  float t;
  glm::vec3 hitPoint;
  bool hit =
      RayIntersectsMesh(rayOrigin, rayDir, m_CurrentMesh.get(), model, t, hitPoint);

  if (hit) {
    // Draw 3D Brush Cursor
    DrawBrushCursor(projection * view, viewportOrigin, viewportSize, drawList,
                    hitPoint, m_BrushRadius);

    // Apply Brush
    if (ImGui::IsMouseDown(0)) {
      if (!m_IsPainting && ImGui::IsMouseClicked(0)) {
        PushHistory(m_CurrentMesh);
        m_IsPainting = true;
      }
      if (m_IsPainting) ApplyTerrainBrush(m_CurrentMesh.get(), hitPoint, model);
    } else {
      m_IsPainting = false;
    }
  } else {
    m_IsPainting = false;
  }

  // Hotkeys
  if (ImGui::IsKeyPressed(ImGuiKey_Z) && ImGui::GetIO().KeyCtrl) {
    Undo();
  }
}

// Draw in the Scene window after its image, with no OpenGL state changes.
void ObjectManipulationTool::DrawBrushCursor(
    const glm::mat4& viewProjection, const glm::vec2& viewportOrigin,
    const glm::vec2& viewportSize, ImDrawList* drawList,
    const glm::vec3& position, float radius) {
  if (!drawList || viewportSize.x <= 0.0f || viewportSize.y <= 0.0f)
    return;

  auto project = [&](const glm::vec3& point, ImVec2& screen) {
    const glm::vec4 clip = viewProjection * glm::vec4(point, 1.0f);
    if (!std::isfinite(clip.w) || clip.w <= 1e-6f ||
        clip.z < -clip.w || clip.z > clip.w)
      return false;
    const glm::vec3 ndc = glm::vec3(clip) / clip.w;
    screen = ImVec2(viewportOrigin.x + (ndc.x + 1.0f) * 0.5f * viewportSize.x,
                    viewportOrigin.y + (1.0f - ndc.y) * 0.5f * viewportSize.y);
    return std::isfinite(screen.x) && std::isfinite(screen.y);
  };

  drawList->PushClipRect(ImVec2(viewportOrigin.x, viewportOrigin.y),
      ImVec2(viewportOrigin.x + viewportSize.x, viewportOrigin.y + viewportSize.y), true);
  constexpr int segments = 32;
  for (int i = 0; i < segments; ++i) {
    const float first = static_cast<float>(i) / segments * 6.2831853f;
    const float second = static_cast<float>(i + 1) / segments * 6.2831853f;
    const glm::vec3 lift(0.0f, 0.05f, 0.0f);
    const glm::vec3 p1 = position + lift +
        glm::vec3(std::cos(first), 0.0f, std::sin(first)) * radius;
    const glm::vec3 p2 = position + lift +
        glm::vec3(std::cos(second), 0.0f, std::sin(second)) * radius;
    ImVec2 s1, s2;
    if (project(p1, s1) && project(p2, s2))
      drawList->AddLine(s1, s2, IM_COL32(0, 255, 0, 255), 3.0f);
  }
  drawList->PopClipRect();
}

void ObjectManipulationTool::RenderTerrainModeUI() {
  ImGui::Text("Terrain / Sculpt Mode");

  // Tools
  if (ImGui::RadioButton("Raise", m_TerrainAction == TerrainAction::Raise))
    m_TerrainAction = TerrainAction::Raise;
  ImGui::SameLine();
  if (ImGui::RadioButton("Lower", m_TerrainAction == TerrainAction::Lower))
    m_TerrainAction = TerrainAction::Lower;
  if (ImGui::RadioButton("Flatten", m_TerrainAction == TerrainAction::Flatten))
    m_TerrainAction = TerrainAction::Flatten;
  ImGui::SameLine();
  if (ImGui::RadioButton("Smooth", m_TerrainAction == TerrainAction::Smooth))
    m_TerrainAction = TerrainAction::Smooth;

  ImGui::DragFloat("Brush Radius", &m_BrushRadius, 0.1f, 0.1f, 50.0f);
  ImGui::DragFloat("Strength", &m_BrushStrength, 0.01f, 0.01f, 10.0f);

  if (ImGui::Button("Reset Normals")) {
    if (m_CurrentMesh) {
      m_CurrentMesh->RecalculateNormals();
      m_CurrentMesh->UpdateVertices();
    }
  }
}

void ObjectManipulationTool::RenderObjectModeUI() {
  ImGui::Text("Object Modifier Mode");

  const char *types[] = {"Twist", "Bend", "Taper"};
  int type = (int)m_DeformationType;
  if (ImGui::Combo("Modifier", &type, types, 3)) {
    m_DeformationType = (DeformationType)type;
    m_DeformationValue = 0.0f; // Reset value on switch
    // Capture base state
    if (m_CurrentMesh && m_BaseMeshState.empty()) {
      m_BaseMeshState = static_cast<const Mesh&>(*m_CurrentMesh).GetVertices();
    }
  }

  const char *axes[] = {"X", "Y", "Z"};
  ImGui::Combo("Axis", &m_DeformationAxis, axes, 3);

  // Capture state before drag starts (Checking ImGui active state would be
  // better)
  bool valueChanged =
      ImGui::SliderFloat("Value", &m_DeformationValue, -5.0f, 5.0f);

  if (valueChanged && m_CurrentMesh) {
    if (m_BaseMeshState.empty()) {
      m_BaseMeshState = static_cast<const Mesh&>(*m_CurrentMesh).GetVertices();
    }
    ApplyDeformation(m_CurrentMesh.get(), m_DeformationType, m_DeformationValue,
                     m_DeformationAxis);
  }

  if (ImGui::IsItemDeactivatedAfterEdit()) {
    if (m_CurrentMesh) {
      m_CurrentMesh
          ->RecalculateNormals(); // Recalc only after drag ends to save perf
      m_CurrentMesh->UpdateVertices();
      // Maybe clear base state to commit?
      // Or keep it to allow further adjustment?
      // Let's keep base state until tool close or entity change.
    }
  }

  if (ImGui::Button("Commit Changes")) {
    // Clear base state, making current state the new base
    m_BaseMeshState.clear();
    m_DeformationValue = 0.0f;
  }
  ImGui::SameLine();
  if (ImGui::Button("Revert")) {
    if (m_CurrentMesh && !m_BaseMeshState.empty()) {
      m_CurrentMesh->GetVertices() = m_BaseMeshState;
      m_CurrentMesh->UpdateVertices();
      m_CurrentMesh->RecalculateNormals();
      m_BaseMeshState.clear();
      m_DeformationValue = 0.0f;
    }
  }
}

bool ObjectManipulationTool::RayIntersectsMesh(
    const glm::vec3 &rayOrigin, const glm::vec3 &rayDir, Mesh *mesh,
    const glm::mat4 &modelMatrix, float &outT, glm::vec3 &outHitPoint) {
  if (!mesh)
    return false;

  // Transform ray to local space is usually easier, but let's transform
  // triangles to world space to be safe Or Transform Ray to Local Space:
  glm::mat4 invModel = glm::inverse(modelMatrix);
  glm::vec4 localOrigin4 = invModel * glm::vec4(rayOrigin, 1.0f);
  glm::vec3 localOrigin = glm::vec3(localOrigin4);
  glm::vec3 localDir = glm::vec3(invModel * glm::vec4(rayDir, 0.0f));

  // Iterate triangles
  const auto &indices = mesh->GetIndices();
  const auto &vertices = static_cast<const Mesh&>(*mesh).GetVertices();

  float closestT = std::numeric_limits<float>::max();
  bool hit = false;

  for (size_t i = 0; i < indices.size(); i += 3) {
    if (i + 2 >= indices.size())
      break;

    glm::vec3 v0 = vertices[indices[i]].position;
    glm::vec3 v1 = vertices[indices[i + 1]].position;
    glm::vec3 v2 = vertices[indices[i + 2]].position;

    // Moller-Trumbore intersection
    const float EPSILON = 0.0000001f;
    glm::vec3 edge1 = v1 - v0;
    glm::vec3 edge2 = v2 - v0;
    glm::vec3 h = glm::cross(localDir, edge2);
    float a = glm::dot(edge1, h);

    if (a > -EPSILON && a < EPSILON)
      continue; // Parallel

    float f = 1.0f / a;
    glm::vec3 s = localOrigin - v0;
    float u = f * glm::dot(s, h);

    if (u < 0.0f || u > 1.0f)
      continue;

    glm::vec3 q = glm::cross(s, edge1);
    float v = f * glm::dot(localDir, q);

    if (v < 0.0f || u + v > 1.0f)
      continue;

    float t = f * glm::dot(edge2, q);

    if (t > EPSILON && t < closestT) {
      closestT = t;
      hit = true;
    }
  }

  if (hit) {
    outT = closestT;
    // Hit point in world space
    outHitPoint = rayOrigin + rayDir * closestT;
    return true;
  }

  return false;
}

void ObjectManipulationTool::ApplyTerrainBrush(Mesh *mesh,
                                               const glm::vec3 &hitPoint,
                                               const glm::mat4 &modelMatrix) {
  if (!mesh)
    return;

  // Transform HitPoint to Local Space
  glm::mat4 invModel = glm::inverse(modelMatrix);
  glm::vec3 localHitPoint = glm::vec3(invModel * glm::vec4(hitPoint, 1.0f));

  auto &vertices = mesh->GetVertices();
  bool changed = false;

  float radiusSq = m_BrushRadius * m_BrushRadius;
  glm::vec3 scaleVector =
      glm::vec3(modelMatrix[0][0], modelMatrix[1][1],
                modelMatrix[2][2]); // Assuming simple scaling
  // Adjust radius to local space?
  // If scaled by 2, distance in world is 2x distance in local.
  // So if world radius is 5, local radius should be 2.5?
  // Let's just use local distance.
  // But brush radius is usually defined in screen/world units.

  // Simplified: Ignore non-uniform scale for radius calc
  float localRadius = m_BrushRadius / ((scaleVector.x + scaleVector.z) * 0.5f);
  float localRadiusSq = localRadius * localRadius;

  float dt = ImGui::GetIO().DeltaTime;
  float amount = m_BrushStrength * dt * 5.0f; // Speed factor

  for (auto &v : vertices) {
    // Only modify Y?
    // Check distance in XZ plane
    float distSq =
        (v.position.x - localHitPoint.x) * (v.position.x - localHitPoint.x) +
        (v.position.z - localHitPoint.z) * (v.position.z - localHitPoint.z);

    if (distSq < localRadiusSq) {
      float dist = sqrt(distSq);
      // Falloff function (Cosine or Linear)
      float falloff = (cos(dist / localRadius * 3.14159f) + 1.0f) * 0.5f;
      // float falloff = 1.0f - (dist / localRadius);

      if (m_TerrainAction == TerrainAction::Raise) {
        v.position.y += amount * falloff;
      } else if (m_TerrainAction == TerrainAction::Lower) {
        v.position.y -= amount * falloff;
      } else if (m_TerrainAction == TerrainAction::Flatten) {
        // Move towards hit point Y
        float diff = localHitPoint.y - v.position.y;
        v.position.y += diff * amount * falloff;
      } else if (m_TerrainAction == TerrainAction::Smooth) {
        // Requires neighbor info, hard in vertex list.
        // Simple hack: move towards average of neighbors?
        // Or just move towards hitPoint Y (same as flatten?)
        // Proper smooth is expensive. Let's just average with target height 0
        // for now or skip.
      }
      changed = true;
    }
  }

  if (changed) {
    mesh->RecalculateNormals(); // Expensive every frame?
    mesh->UpdateVertices();
  }
}

void ObjectManipulationTool::ApplyDeformation(Mesh *mesh, DeformationType type,
                                              float value, int axis) {
  if (!mesh || m_BaseMeshState.empty())
    return;

  auto &vertices = mesh->GetVertices();
  const auto &baseVertices = m_BaseMeshState;

  if (vertices.size() != baseVertices.size())
    return;

  for (size_t i = 0; i < vertices.size(); ++i) {
    glm::vec3 p = baseVertices[i].position;
    glm::vec3 result = p;

    if (type == DeformationType::Twist) {
      // Twist around Axis (usually Y) based on Y height
      float angle = value * p.y;

      float c = cos(angle);
      float s = sin(angle);

      if (axis == 1) { // Twist around Y
        result.x = p.x * c - p.z * s;
        result.z = p.x * s + p.z * c;
      }
    } else if (type == DeformationType::Bend) {
      // Simple Bend
      // Assume bending Y axis around Z?
      // Let's implement a simple Bend modifier
      if (fabs(value) < 0.001f)
        continue;

      // Bend Y along X?
      // Formula:
      // r = 1/k
      // theta = k * y
      // x' = x
      // y' = sin(theta) * (r - x)
      // z' = cos(theta) * (r - x) + r ... this rotates the whole thing

      // Easier: Bend axis Y.
      float k = value;
      float r = 1.0f / k;
      float theta = p.y * k;

      float c = cos(theta);
      float s = sin(theta);

      // Bend Y into X
      result.x = p.x + (1.0f - c) * r; // Offset
      // This is getting complex to derive on the fly correctly.
      // Standard Bend:
      // x' = x
      // y' = -sin(theta) * (z - 1/k)
      // z' = cos(theta) * (z - 1/k) + 1/k

      // Let's treat 'Axis' as the UP vector, and we bend along perpendicular
      // Simplest: Taper
    } else if (type == DeformationType::Taper) {
      // Scale X/Z based on Y
      float scale = 1.0f + (p.y) * value;
      if (scale < 0)
        scale = 0;
      result.x = p.x * scale;
      result.z = p.z * scale;
    }

    vertices[i].position = result;
  }

  mesh->UpdateVertices();
  // Do NOT recalc normals here every frame if dragging slider, wait for release
  // (handled in UI)
}

void ObjectManipulationTool::PushHistory(const std::shared_ptr<Mesh>& mesh) {
  if (!mesh)
    return;
  HistoryState state;
  state.targetMesh = mesh;
  state.vertices = static_cast<const Mesh&>(*mesh).GetVertices();
  m_UndoStack.push_back(state);

  if (m_UndoStack.size() > 20) {
    m_UndoStack.erase(m_UndoStack.begin());
  }
}

void ObjectManipulationTool::Undo() {
  if (m_UndoStack.empty())
    return;

  HistoryState state = m_UndoStack.back();
  m_UndoStack.pop_back();

  if (state.targetMesh) {
    state.targetMesh->GetVertices() = state.vertices;
    state.targetMesh->UpdateVertices();
    state.targetMesh->RecalculateNormals();
  }
}

} // namespace Archura
