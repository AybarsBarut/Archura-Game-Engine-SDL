#pragma once

#include <string>
#include <filesystem>

// Forward declarations for Mono types to avoid including mono headers in header file
typedef struct _MonoClass MonoClass;
typedef struct _MonoObject MonoObject;
typedef struct _MonoMethod MonoMethod;
typedef struct _MonoAssembly MonoAssembly;
typedef struct _MonoImage MonoImage;

namespace Archura {

    struct EntityHandle;

    class ScriptEngine {
    public:
        static void Init();
        static void Shutdown();

        static void LoadAssembly(const std::filesystem::path& filepath);
        
        static void OnRuntimeStart(class Scene* scene);
        static void OnRuntimeStop();

        static bool ClassExists(const std::string& fullClassName);
        static bool OnCreateEntity(const class Entity& entity);
        static bool OnUpdateEntity(const class Entity& entity, float ts);
        static void OnDestroyEntity(EntityHandle entity);

        static MonoImage* GetCoreAssemblyImage();

    private:
        static void InitMono();
        static void ShutdownMono();
        static MonoObject* InstantiateClass(MonoClass* monoClass);
        static void LoadAssemblyClasses(MonoAssembly* assembly);
        
        friend class ScriptClass;
        friend class ScriptGlue;
    };

}
