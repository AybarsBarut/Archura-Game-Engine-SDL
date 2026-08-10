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
#include <limits>
#include <memory>
#include <unordered_map>
#include <glm/glm.hpp>


namespace Archura {

    // Stable blittable ABI mirror of ScriptCore.Vector3. Never expose GLM
    // types directly across the managed boundary: their size/alignment may
    // change with GLM build flags.
    struct InteropVector3 final { float x; float y; float z; };
    static_assert(sizeof(InteropVector3) == 12, "managed Vector3 ABI changed");

    static glm::vec3 FromInterop(const InteropVector3& value) noexcept {
        return {value.x, value.y, value.z};
    }
    static InteropVector3 ToInterop(const glm::vec3& value) noexcept {
        return {value.x, value.y, value.z};
    }

    static std::unordered_map<uint64_t, std::string> s_ScriptCameras;
    static uint64_t s_NextCameraHandle = 1;

    static Camera* ResolveCamera(uint64_t handle) {
        const auto found = s_ScriptCameras.find(handle);
        return found == s_ScriptCameras.end()
                   ? nullptr
                   : ResourceManager::Get().GetCamera(found->second);
    }

    #define ARCHURA_ADD_INTERNAL_CALL(Name) mono_add_internal_call("Archura.InternalCalls::" #Name, Name)

    static void NativeLog(MonoString* string, int parameter) {
        (void)parameter;
        if (!string) return;
        char* cStr = mono_string_to_utf8(string);
        DeveloperConsole::GetInstance().Print(std::string("[Script] ") + cStr);
        mono_free(cStr);
    }

    #pragma region Entity Lifecycle

    static Entity* ResolveEntity(uint64_t entityHandle) {
        Scene* scene = Application::Get().GetScene();
        return scene ? scene->GetEntity(EntityHandle::FromValue(entityHandle))
                     : nullptr;
    }

    static uint64_t Entity_Create(MonoString* nameStr) {
        Scene* scene = Application::Get().GetScene();
        if (scene && nameStr) {
          try {
            char* cStr = mono_string_to_utf8(nameStr);
            if (!cStr) return 0;
            std::string name(cStr);
            mono_free(cStr);
            Entity* newEntity = scene->CreateEntity(name);
            return newEntity ? newEntity->GetHandle().Value() : 0;
          } catch (const std::exception& error) {
            std::cerr << "[ScriptGlue] Entity_Create failed: " << error.what()
                      << '\n';
          } catch (...) {
            std::cerr << "[ScriptGlue] Entity_Create failed\n";
          }
        }
        return 0;
    }

    static void Entity_Destroy(uint64_t entityID) {
        Scene* scene = Application::Get().GetScene();
        if (scene) {
            scene->DestroyEntity(EntityHandle::FromValue(entityID));
        }
    }

    static mono_bool Entity_Exists(uint64_t entityID) {
        Scene* scene = Application::Get().GetScene();
        return scene && scene->GetEntity(EntityHandle::FromValue(entityID)) != nullptr;
    }

    static uint64_t Entity_FindByName(MonoString* nameStr) {
        Scene* scene = Application::Get().GetScene();
        if (!scene || !nameStr) return 0;

        char* cStr = mono_string_to_utf8(nameStr);
        std::string name(cStr);
        mono_free(cStr);

        for (const auto& entity : scene->GetEntities()) {
            if (entity && entity->GetName() == name) {
                return entity->GetHandle().Value();
            }
        }
        return 0;
    }

    static MonoString* Entity_GetName(uint64_t entityID) {
        Entity* entity = ResolveEntity(entityID);
        const std::string name = entity ? entity->GetName() : std::string();
        return mono_string_new(mono_domain_get(), name.c_str());
    }

    static void Entity_SetName(uint64_t entityID, MonoString* nameStr) {
        Entity* entity = ResolveEntity(entityID);
        if (!entity || !nameStr) return;

        char* cStr = mono_string_to_utf8(nameStr);
        entity->SetName(cStr);
        mono_free(cStr);
    }

    static mono_bool Entity_HasComponent(uint64_t entityID, MonoString* componentNameStr) {
        Entity* entity = ResolveEntity(entityID);
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

    static void Transform_GetPosition(uint64_t entityID, InteropVector3* outTranslation) {
        // Need Scene access. ScriptEngine knows the sceneContext? 
        // Or we pass ID and find entity. 
        // Let's assume ScriptEngin::GetSceneContext() exists or use Application::Get().GetScene()
        
        if (!outTranslation) return;
        *outTranslation = {};
        Entity* entity = ResolveEntity(entityID);
        Transform* transform = entity ? entity->GetComponent<Transform>() : nullptr;
        if (transform) *outTranslation = ToInterop(transform->position);
    }

    static void Transform_SetPosition(uint64_t entityID, InteropVector3* translation) {
        if (!translation) return;
        Entity* entity = ResolveEntity(entityID);
        Transform* transform = entity ? entity->GetComponent<Transform>() : nullptr;
        if (transform) transform->position = FromInterop(*translation);
    }

    static void Transform_GetRotation(uint64_t entityID, InteropVector3* outRotation) {
        if (!outRotation) return;
        *outRotation = {};
        Entity* entity = ResolveEntity(entityID);
        Transform* transform = entity ? entity->GetComponent<Transform>() : nullptr;
        if (transform) *outRotation = ToInterop(transform->rotation);
    }

    static void Transform_SetRotation(uint64_t entityID, InteropVector3* rotation) {
        if (!rotation) return;
        Entity* entity = ResolveEntity(entityID);
        Transform* transform = entity ? entity->GetComponent<Transform>() : nullptr;
        if (transform) transform->rotation = FromInterop(*rotation);
    }

    static void Transform_GetScale(uint64_t entityID, InteropVector3* outScale) {
        if (!outScale) return;
        *outScale = {};
        Entity* entity = ResolveEntity(entityID);
        Transform* transform = entity ? entity->GetComponent<Transform>() : nullptr;
        if (transform) *outScale = ToInterop(transform->scale);
    }

    static void Transform_SetScale(uint64_t entityID, InteropVector3* scale) {
        if (!scale) return;
        Entity* entity = ResolveEntity(entityID);
        Transform* transform = entity ? entity->GetComponent<Transform>() : nullptr;
        if (transform) transform->scale = FromInterop(*scale);
    }

    #pragma endregion

    #pragma region Input

    static mono_bool Input_IsKeyDown(int keycode) {
        (void)keycode;
        // Placeholder for Input System access
        // return Input::IsKeyDown(keycode); 
        return false; 
    }

    #pragma endregion

    #pragma region Physics

    static void RigidBody_ApplyForce(uint64_t entityID, InteropVector3* force) {
        if (!force) return;
        Scene* scene = Application::Get().GetScene();
        if (scene) {
            Entity* entity = ResolveEntity(entityID);
            if (entity && entity->HasComponent<RigidBody>()) {
                entity->GetComponent<RigidBody>()->force += FromInterop(*force);
            }
        }
    }

    static void RigidBody_GetVelocity(uint64_t entityID, InteropVector3* outVelocity) {
        if (!outVelocity) return;
        *outVelocity = {};
        Scene* scene = Application::Get().GetScene();
        if (scene) {
            Entity* entity = ResolveEntity(entityID);
            if (entity && entity->HasComponent<RigidBody>()) {
                *outVelocity = ToInterop(entity->GetComponent<RigidBody>()->velocity);
            }
        }
    }

    static void RigidBody_SetVelocity(uint64_t entityID, InteropVector3* velocity) {
        if (!velocity) return;
        Scene* scene = Application::Get().GetScene();
        if (scene) {
            Entity* entity = ResolveEntity(entityID);
            if (entity && entity->HasComponent<RigidBody>()) {
                entity->GetComponent<RigidBody>()->velocity = FromInterop(*velocity);
            }
        }
    }

    static void RigidBody_SetGravityEnabled(uint64_t entityID, mono_bool enabled) {
        Scene* scene = Application::Get().GetScene();
        if (scene) {
            Entity* entity = ResolveEntity(entityID);
            if (entity && entity->HasComponent<RigidBody>()) {
                entity->GetComponent<RigidBody>()->useGravity = enabled != 0;
            }
        }
    }

    #pragma endregion

    #pragma region MeshRenderer

    static void MeshRenderer_GetColor(uint64_t entityID, InteropVector3* outColor) {
        if (!outColor) return;
        *outColor = {};
        Scene* scene = Application::Get().GetScene();
        if (scene) {
            Entity* entity = ResolveEntity(entityID);
            if (entity && entity->HasComponent<MeshRenderer>()) {
                *outColor = ToInterop(entity->GetComponent<MeshRenderer>()->color);
            }
        }
    }

    static void MeshRenderer_SetColor(uint64_t entityID, InteropVector3* color) {
        if (!color) return;
        Scene* scene = Application::Get().GetScene();
        if (scene) {
            Entity* entity = ResolveEntity(entityID);
            if (entity && entity->HasComponent<MeshRenderer>()) {
                entity->GetComponent<MeshRenderer>()->color = FromInterop(*color);
            }
        }
    }

    #pragma endregion

    #pragma region Audio

    static void AudioSource_Play(uint64_t entityID) {
        Scene* scene = Application::Get().GetScene();
        if (scene) {
            Entity* entity = ResolveEntity(entityID);
            if (entity && entity->HasComponent<AudioSource>()) {
                entity->GetComponent<AudioSource>()->isPlaying = true;
            }
        }
    }

    #pragma endregion

    #pragma region Camera

    static uint64_t Camera_Create(InteropVector3* position) {
        if (!position) return 0;
        try {
            auto cam = std::make_unique<Camera>(FromInterop(*position));
            if (s_NextCameraHandle == std::numeric_limits<uint64_t>::max())
                return 0;

            const uint64_t handle = s_NextCameraHandle;
            std::string cameraName = "ScriptCamera_" + std::to_string(handle);
            s_ScriptCameras.emplace(handle, cameraName);
            try {
                ResourceManager::Get().AddCamera(cameraName, cam.get());
            } catch (...) {
                s_ScriptCameras.erase(handle);
                throw;
            }
            cam.release(); // ResourceManager owns it after successful insertion.
            ++s_NextCameraHandle;
            return handle;
        } catch (const std::exception& error) {
            std::cerr << "[ScriptGlue] Camera_Create failed: " << error.what()
                      << '\n';
        } catch (...) {
            std::cerr << "[ScriptGlue] Camera_Create failed\n";
        }
        return 0;
    }

    static void Camera_SetActive(uint64_t cameraHandle) {
        Camera* cam = ResolveCamera(cameraHandle);
        RenderSystem* rs = Application::Get().GetRenderSystem();
        if (rs && cam) {
            rs->SetCamera(cam);
        }
    }
    
    static void Camera_GetPosition(uint64_t cameraHandle, InteropVector3* outPos) {
        if (!outPos) return;
        *outPos = {};
        Camera* cam = ResolveCamera(cameraHandle);
        if (cam) *outPos = ToInterop(cam->GetPosition());
    }

    static void Camera_SetPosition(uint64_t cameraHandle, InteropVector3* pos) {
        if (!pos) return;
        Camera* cam = ResolveCamera(cameraHandle);
        if (cam) cam->SetPosition(FromInterop(*pos));
    }

    static float Camera_GetPitch(uint64_t cameraHandle) {
        Camera* cam = ResolveCamera(cameraHandle);
        return cam ? cam->GetPitch() : 0.0f;
    }
    
    static float Camera_GetYaw(uint64_t cameraHandle) {
        Camera* cam = ResolveCamera(cameraHandle);
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
