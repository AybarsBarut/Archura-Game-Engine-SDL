#include "scripting/ScriptGlue.h"
#include "scripting/ScriptEngine.h"
#include "ecs/Entity.h"
#include "ecs/Component.h"
#include "core/Application.h"
#include "core/ResourceManager.h"
#include "rendering/Camera.h"
#include "game/RenderSystem.h"
#include "game/AudioSource.h"
// #include "input/Input.h" 

#include <mono/metadata/object.h>
#include <mono/metadata/reflection.h>
#include <iostream>
#include <glm/glm.hpp>


namespace Archura {

    #define ARCHURA_ADD_INTERNAL_CALL(Name) mono_add_internal_call("Archura.InternalCalls::" #Name, Name)

    static void NativeLog(MonoString* string, int parameter) {
        char* cStr = mono_string_to_utf8(string);
        // std::cout << "[Script Log] " << cStr << ", " << parameter << std::endl;
        // Ideally use DevConsole
        mono_free(cStr);
    }

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

    #pragma endregion

    #pragma region Input

    static bool Input_IsKeyDown(int keycode) {
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
        
        ARCHURA_ADD_INTERNAL_CALL(Transform_GetPosition);
        ARCHURA_ADD_INTERNAL_CALL(Transform_SetPosition);
        
        ARCHURA_ADD_INTERNAL_CALL(Input_IsKeyDown);
        
        ARCHURA_ADD_INTERNAL_CALL(RigidBody_ApplyForce);
        
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
