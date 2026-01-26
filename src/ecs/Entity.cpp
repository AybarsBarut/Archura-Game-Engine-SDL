#include "Entity.h"
#include <algorithm>

namespace Archura {

Entity::Entity(EntityID id, const std::string& name)
    : m_ID(id), m_Name(name)
{
    // Her varlik otomatik olarak Donusum bilesenine sahip
    // Her varlik otomatik olarak Donusum bilesenine sahip
    AddComponent<Transform>();
}

void Entity::SetParent(Entity* parent) {
    if (m_Parent) {
        // Remove from old parent
        auto& kids = m_Parent->m_Children;
        kids.erase(std::remove(kids.begin(), kids.end(), this), kids.end());
    }
    
    m_Parent = parent;
    
    if (m_Parent) {
        m_Parent->m_Children.push_back(this);
    }
}

glm::mat4 Entity::GetWorldTransform() {
    auto* transform = GetComponent<Transform>();
    glm::mat4 model = transform ? transform->GetModelMatrix() : glm::mat4(1.0f);
    
    if (m_Parent) {
        return m_Parent->GetWorldTransform() * model;
    }
    return model;
}

// ==================== Scene ====================

Scene::Scene(const std::string& name)
    : m_Name(name)
{
}

Entity* Scene::CreateEntity(const std::string& name) {
    auto entity = std::make_shared<Entity>(m_NextEntityID++, name);
    m_Entities.push_back(entity);
    return entity.get();
}

void Scene::DestroyEntity(EntityID id) {
    auto it = std::remove_if(m_Entities.begin(), m_Entities.end(),
        [id](const std::shared_ptr<Entity>& entity) {
            return entity->GetID() == id;
        });
    m_Entities.erase(it, m_Entities.end());
}

Entity* Scene::GetEntity(EntityID id) {
    for (auto& entity : m_Entities) {
        if (entity->GetID() == id) {
            return entity.get();
        }
    }
    return nullptr;
}

} // namespace Archura
