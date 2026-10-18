#include "editor/TransformUndoHistory.h"
#include <iostream>

int main() {
    Archura::Scene scene;
    Archura::TransformUndoHistory history;
    auto* entity = scene.CreateEntity("edited");
    auto* transform = entity->GetComponent<Archura::Transform>();
    transform->position = {1, 2, 3};
    transform->rotation = {4, 5, 6};
    transform->scale = {2, 3, 4};
    history.Capture(*entity);
    *transform = Archura::Transform{};
    if (history.Undo(scene) != entity || transform->position != glm::vec3(1, 2, 3)
        || transform->rotation != glm::vec3(4, 5, 6)
        || transform->scale != glm::vec3(2, 3, 4)) return 1;
    history.Capture(*entity);
    scene.DestroyEntity(entity->GetHandle());
    auto* replacement = scene.CreateEntity("replacement");
    if (history.Undo(scene) || replacement->GetComponent<Archura::Transform>()->position != glm::vec3(0)) return 2;
    history.Capture(*replacement);
    replacement->RemoveComponent<Archura::Transform>();
    if (history.Undo(scene) || history.Undo(scene)) return 3;
    replacement->AddComponent<Archura::Transform>();
    for (int i = 0; i < 60; ++i) history.Capture(*replacement);
    for (int i = 0; i < 50; ++i) if (!history.Undo(scene)) return 4;
    if (history.Undo(scene)) return 5;
    history.Capture(*replacement);
    history.Clear();
    if (history.Undo(scene)) return 6;
    Archura::Scene otherScene;
    auto* other = otherScene.CreateEntity("same initial handle, different scene");
    history.Capture(*other);
    if (history.Undo(scene)) return 7;
    std::cout << "Transform undo: restore, deletion, component removal, bounds passed\n";
    return 0;
}
