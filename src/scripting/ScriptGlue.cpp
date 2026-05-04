#include "scripting/ScriptGlue.h"
#include "scripting/ScriptEngine.h"
#include "ecs/Entity.h"
#include "ecs/Component.h"
#include "core/Application.h"
#include "core/ResourceManager.h"
#include "rendering/Camera.h"
#include "game/RenderSystem.h"
#include "game/AudioSource.h"
#include "core/DeveloperConsole.h"
// #include "input/Input.h" 

#include <mono/metadata/object.h>
#include <mono/metadata/reflection.h>
#include <mono/jit/jit.h>
#include <iostream>
#include <glm/glm.hpp>


namespace Archura {

    #define ARCHURA_ADD_INTERNAL_CALL(Name) mono_add_internal_call("Archura.InternalCalls::" #Name, Name)

    static void NativeLog(MonoString* string, int parameter) {
        (void)parameter;
        char* cStr = mono_string_to_utf8(string);
        DeveloperConsole::GetInstance().Print(std::string("[Script] ") + cStr);
        mono_free(cStr);
    }

    #pragma region Entity Lifecycle

    static uint64_t Entity_Create(MonoString* nameStr) {
        Scene* scene = Application::Get().GetScene();
        if (scene) {
            char* cStr = mono_string_to_utf8(nameStr);
            std::string name(cStr);
            mono_free(cStr);
            Entity* newEntity = scene->CreateEntity(name);
            return newEntity ? newEntity->GetID() : 0;
        }
        return 0;
    }

    static void Entity_Destroy(uint64_t entityID) {
        Scene* scene = Application::Get().GetScene();
        if (scene) {
            scene->DestroyEntity((uint32_t)entityID);
        }
    }

    static bool Entity_Exists(uint64_t entityID) {
        Scene* scene = Application::Get().GetScene();
        return scene && scene->GetEntity((uint32_t)entityID) != nullptr;
    }

    static uint64_t Entity_FindByName(MonoString* nameStr) {
        Scene* scene = Application::Get().GetScene();
        if (!scene || !nameStr) return 0;

        char* cStr = mono_string_to_utf8(nameStr);
        std::string name(cStr);
        mono_free(cStr);

        for (const auto& entity : scene->GetEntities()) {
            if (entity && entity->GetName() == name) {
                return entity->GetID();
            }
        }
        return 0;
    }

    static MonoString* Entity_GetName(uint64_t entityID) {
        Scene* scene = Application::Get().GetScene();
        Entity* entity = scene ? scene->GetEntity((uint32_t)entityID) : nullptr;
        const std::string name = entity ? entity->GetName() : std::string();
        return mono_string_new(mono_domain_get(), name.c_str());
    }

    static void Entity_SetName(uint64_t entityID, MonoString* nameStr) {
        Scene* scene = Application::Get().GetScene();
        Entity* entity = scene ? scene->GetEntity((uint32_t)entityID) : nullptr;
        if (!entity || !nameStr) return;

        char* cStr = mono_string_to_utf8(nameStr);
        entity->SetName(cStr);
        mono_free(cStr);
    }

    static bool Entity_HasComponent(uint64_t entityID, MonoString* componentNameStr) {
        Scene* scene = Application::Get().GetScene();
        Entity* entity = scene ? scene->GetEntity((uint32_t)entityID) : nullptr;
        if (!entity || !componentNameStr) return false;

        char* cStr = mono_string_to_utf8(componentNameStr);
        std::string componentName(cStr);
        mono_free(cStr);

        if (componentName == "Transform" || componentName == "TransformComponent")
            return entity->HasComponent<Transform>();
        if (componentName == "RigidBody" || componentName == "Rigidbody")
            return entity->HasComponent<RigidBody>();
        if (componentName == "MeshRenderer")
            return entity->HasComponent<MeshRenderer>();
        if (componentName == "AudioSource")
            return entity->HasComponent<AudioSource>();
        if (componentName == "BoxCollider")
            return entity->HasComponent<BoxCollider>();
        if (componentName == "Light" || componentName == "LightComponent")
            return entity->HasComponent<LightComponent>();
        if (componentName == "Script" || componentName == "ScriptComponent")
            return entity->HasComponent<ScriptComponent>();
        return false;
    }

    #pragma endregion

    #pragma region Transform

    static void Transform_GetPosition(uint64_t entityID, glm::vec3* outTranslation) {
        // Need Scene access. ScriptEngine knows the sceneContext? 
        // Or we pass ID and find entity. 
        // Let's assume ScriptEngin::GetSceneContext() exists or use Application::Get().GetScene()
        
        Scene* scene = Application::Get().GetScene();
        if (scene) {
            Entity* entity = scene->GetEntity((uint32_t)entityID);
            if (entity) {
                *outTranslation = entity->GetComponent<Transform>()->position;
            }
        }
    }

    static void Transform_SetPosition(uint64_t entityID, glm::vec3* translation) {
        Scene* scene = Application::Get().GetScene();
        if (scene) {
            Entity* entity = scene->GetEntity((uint32_t)entityID);
            if (entity) {
                entity->GetComponent<Transform>()->position = *translation;
            }
        }
    }

    static void Transform_GetRotation(uint64_t entityID, glm::vec3* outRotation) {
        Scene* scene = Application::Get().GetScene();
        if (scene) {
            Entity* entity = scene->GetEntity((uint32_t)entityID);
            if (entity) *outRotation = entity->GetComponent<Transform>()->rotation;
        }
    }

    static void Transform_SetRotation(uint64_t entityID, glm::vec3* rotation) {
        Scene* scene = Application::Get().GetScene();
        if (scene) {
            Entity* entity = scene->GetEntity((uint32_t)entityID);
            if (entity) entity->GetComponent<Transform>()->rotation = *rotation;
        }
    }

    static void Transform_GetScale(uint64_t entityID, glm::vec3* outScale) {
        Scene* scene = Application::Get().GetScene();
        if (scene) {
            Entity* entity = scene->GetEntity((uint32_t)entityID);
            if (entity) *outScale = entity->GetComponent<Transform>()->scale;
        }
    }

    static void Transform_SetScale(uint64_t entityID, glm::vec3* scale) {
        Scene* scene = Application::Get().GetScene();
        if (scene) {
            Entity* entity = scene->GetEntity((uint32_t)entityID);
            if (entity) entity->GetComponent<Transform>()->scale = *scale;
        }
    }

    #pragma endregion

    #pragma region Input

    static bool Input_IsKeyDown(int keycode) {
        (void)keycode;
        // Placeholder for Input System access
        // return Input::IsKeyDown(keycode); 
        return false; 
    }

    #pragma endregion

    #pragma region Physics

    static void RigidBody_ApplyForce(uint64_t entityID, glm::vec3* force) {
        Scene* scene = Application::Get().GetScene();
        if (scene) {
            Entity* entity = scene->GetEntity((uint32_t)entityID);
            if (entity && entity->HasComponent<RigidBody>()) {
                entity->GetComponent<RigidBody>()->force += *force;
            }
        }
    }

    static void RigidBody_GetVelocity(uint64_t entityID, glm::vec3* outVelocity) {
        Scene* scene = Application::Get().GetScene();
        if (scene) {
            Entity* entity = scene->GetEntity((uint32_t)entityID);
            if (entity && entity->HasComponent<RigidBody>()) {
                *outVelocity = entity->GetComponent<RigidBody>()->velocity;
            }
        }
    }

    static void RigidBody_SetVelocity(uint64_t entityID, glm::vec3* velocity) {
        Scene* scene = Application::Get().GetScene();
        if (scene) {
            Entity* entity = scene->GetEntity((uint32_t)entityID);
            if (entity && entity->HasComponent<RigidBody>()) {
                entity->GetComponent<RigidBody>()->velocity = *velocity;
            }
        }
    }

    static void RigidBody_SetGravityEnabled(uint64_t entityID, bool enabled) {
        Scene* scene = Application::Get().GetScene();
        if (scene) {
            Entity* entity = scene->GetEntity((uint32_t)entityID);
            if (entity && entity->HasComponent<RigidBody>()) {
                entity->GetComponent<RigidBody>()->useGravity = enabled;
            }
        }
    }

    #pragma endregion

    #pragma region MeshRenderer

    static void MeshRenderer_GetColor(uint64_t entityID, glm::vec3* outColor) {
        Scene* scene = Application::Get().GetScene();
        if (scene) {
            Entity* entity = scene->GetEntity((uint32_t)entityID);
            if (entity && entity->HasComponent<MeshRenderer>()) {
                *outColor = entity->GetComponent<MeshRenderer>()->color;
            }
        }
    }

    static void MeshRenderer_SetColor(uint64_t entityID, glm::vec3* color) {
        Scene* scene = Application::Get().GetScene();
        if (scene) {
            Entity* entity = scene->GetEntity((uint32_t)entityID);
            if (entity && entity->HasComponent<MeshRenderer>()) {
                entity->GetComponent<MeshRenderer>()->color = *color;
            }
        }
    }

    #pragma endregion

    #pragma region Audio

    static void AudioSource_Play(uint64_t entityID) {
        Scene* scene = Application::Get().GetScene();
        if (scene) {
            Entity* entity = scene->GetEntity((uint32_t)entityID);
            if (entity && entity->HasComponent<AudioSource>()) {
                entity->GetComponent<AudioSource>()->isPlaying = true;
            }
        }
    }

    #pragma endregion

    #pragma region Camera

    static uint64_t Camera_Create(glm::vec3* position) {
        // Create new camera and register with ResourceManager for automatic cleanup
        Camera* cam = new Camera(*position);
        
        // Generate unique name for this script-created camera
        static int cameraCounter = 0;
        std::string cameraName = "ScriptCamera_" + std::to_string(cameraCounter++);
        
        // Register with ResourceManager - it will handle cleanup
        ResourceManager::Get().AddCamera(cameraName, cam);
        
        return reinterpret_cast<uint64_t>(cam);
    }

    static void Camera_SetActive(uint64_t cameraPtr) {
        Camera* cam = reinterpret_cast<Camera*>(cameraPtr);
        RenderSystem* rs = Application::Get().GetRenderSystem();
        if (rs && cam) {
            rs->SetCamera(cam);
        }
    }
    
    static void Camera_GetPosition(uint64_t cameraPtr, glm::vec3* outPos) {
        Camera* cam = reinterpret_cast<Camera*>(cameraPtr);
        if (cam) *outPos = cam->GetPosition();
    }

    static void Camera_SetPosition(uint64_t cameraPtr, glm::vec3* pos) {
        Camera* cam = reinterpret_cast<Camera*>(cameraPtr);
        if (cam) cam->SetPosition(*pos);
    }

    static float Camera_GetPitch(uint64_t cameraPtr) {
        Camera* cam = reinterpret_cast<Camera*>(cameraPtr);
        return cam ? cam->GetPitch() : 0.0f;
    }
    
    static float Camera_GetYaw(uint64_t cameraPtr) {
        Camera* cam = reinterpret_cast<Camera*>(cameraPtr);
        return cam ? cam->GetYaw() : 0.0f;
    }

    #pragma endregion

    void ScriptGlue::RegisterFunctions() {
        ARCHURA_ADD_INTERNAL_CALL(NativeLog);
        
        ARCHURA_ADD_INTERNAL_CALL(Entity_Create);
        ARCHURA_ADD_INTERNAL_CALL(Entity_Destroy);
        ARCHURA_ADD_INTERNAL_CALL(Entity_Exists);
        ARCHURA_ADD_INTERNAL_CALL(Entity_FindByName);
        ARCHURA_ADD_INTERNAL_CALL(Entity_GetName);
        ARCHURA_ADD_INTERNAL_CALL(Entity_SetName);
        ARCHURA_ADD_INTERNAL_CALL(Entity_HasComponent);

        ARCHURA_ADD_INTERNAL_CALL(Transform_GetPosition);
        ARCHURA_ADD_INTERNAL_CALL(Transform_SetPosition);
        ARCHURA_ADD_INTERNAL_CALL(Transform_GetRotation);
        ARCHURA_ADD_INTERNAL_CALL(Transform_SetRotation);
        ARCHURA_ADD_INTERNAL_CALL(Transform_GetScale);
        ARCHURA_ADD_INTERNAL_CALL(Transform_SetScale);
        
        ARCHURA_ADD_INTERNAL_CALL(Input_IsKeyDown);
        
        ARCHURA_ADD_INTERNAL_CALL(RigidBody_ApplyForce);
        ARCHURA_ADD_INTERNAL_CALL(RigidBody_GetVelocity);
        ARCHURA_ADD_INTERNAL_CALL(RigidBody_SetVelocity);
        ARCHURA_ADD_INTERNAL_CALL(RigidBody_SetGravityEnabled);

        ARCHURA_ADD_INTERNAL_CALL(MeshRenderer_GetColor);
        ARCHURA_ADD_INTERNAL_CALL(MeshRenderer_SetColor);
        
        ARCHURA_ADD_INTERNAL_CALL(AudioSource_Play);

        ARCHURA_ADD_INTERNAL_CALL(Camera_Create);
        ARCHURA_ADD_INTERNAL_CALL(Camera_SetActive);
        ARCHURA_ADD_INTERNAL_CALL(Camera_GetPosition);
        ARCHURA_ADD_INTERNAL_CALL(Camera_SetPosition);
        ARCHURA_ADD_INTERNAL_CALL(Camera_GetPitch);
        ARCHURA_ADD_INTERNAL_CALL(Camera_GetYaw);
    }

    void ScriptGlue::RegisterComponents() {
        // Register component types here
    }

}
