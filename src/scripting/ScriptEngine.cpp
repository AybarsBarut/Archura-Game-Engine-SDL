#include "scripting/ScriptEngine.h"
#include "scripting/ScriptGlue.h"
#include "ecs/Entity.h"
#include "ecs/Component.h"

#include <mono/jit/jit.h>
#include <mono/metadata/assembly.h>
#include <mono/metadata/class.h>
#include <mono/metadata/object.h>
#include <fstream>
#include <iostream>
#include <vector>
#include <unordered_map>

namespace Archura {

    struct ScriptEngineData {
        struct ScriptMethods {
            MonoMethod* OnCreate = nullptr;
            MonoMethod* OnStart = nullptr;
            MonoMethod* OnUpdate = nullptr;
            MonoMethod* OnDestroy = nullptr;
        };
        MonoDomain* RootDomain = nullptr;
        MonoDomain* AppDomain = nullptr;

        MonoAssembly* CoreAssembly = nullptr;
        MonoImage* CoreAssemblyImage = nullptr;

        std::filesystem::path CoreAssemblyPath;
        
        // Generation-checked entity handle -> rooted managed object handle.
        // Raw MonoObject pointers are movable/collectable and must not be cached.
        std::unordered_map<uint64_t, uint32_t> EntityScriptInstances;
        std::unordered_map<MonoClass*, ScriptMethods> MethodCache;
        MonoMethod* EntityIdSetter = nullptr;
    };

    static ScriptEngineData* s_Data = nullptr;

    // Helper to read file bytes
    static char* ReadBytes(const std::filesystem::path& filepath, uint32_t* outSize) {
        std::ifstream stream(filepath, std::ios::binary | std::ios::ate);

        if (!stream) {
            // Failed to open file
            return nullptr;
        }

        std::streampos end = stream.tellg();
        stream.seekg(0, std::ios::beg);
        uint32_t size = (uint32_t)(end - stream.tellg());

        if (size == 0) {
            // File is empty
            return nullptr;
        }

        char* buffer = new char[size];
        stream.read((char*)buffer, size);
        stream.close();

        *outSize = size;
        return buffer;
    }

    static MonoAssembly* LoadMonoAssembly(const std::filesystem::path& assemblyPath) {
        uint32_t fileSize = 0;
        char* fileData = ReadBytes(assemblyPath, &fileSize);
        if (!fileData || fileSize == 0) {
            std::cout << "[ScriptEngine] Failed to read assembly: " << assemblyPath << std::endl;
            return nullptr;
        }

        // NOTE: We can't use this image for anything other than loading the assembly because this image doesn't have a reference to the assembly
        MonoImageOpenStatus status;
        MonoImage* image = mono_image_open_from_data_full(fileData, fileSize, 1, &status, 0);

        if (status != MONO_IMAGE_OK) {
            // Log Error
            std::cout << "[ScriptEngine] Mono Image Load Error: " << mono_image_strerror(status) << std::endl;
            delete[] fileData;
            return nullptr;
        }

        MonoAssembly* assembly = mono_assembly_load_from_full(image, assemblyPath.string().c_str(), &status, 0);
        mono_image_close(image);

        // Don't forget to free the file data (FIXED: delete[] for array allocation)
        delete[] fileData;

        return assembly;
    }

    static void ReportMonoException(MonoObject* exception) {
        if (!exception) return;
        MonoObject* formattingException = nullptr;
        MonoString* exceptionString =
            mono_object_to_string(exception, &formattingException);
        if (!exceptionString || formattingException) {
            std::cout << "[ScriptEngine] Managed exception (ToString failed)\n";
            return;
        }
        char* message = mono_string_to_utf8(exceptionString);
        std::cout << "[ScriptEngine] Exception: "
                  << (message ? message : "<unavailable>") << std::endl;
        if (message) mono_free(message);
    }

    static MonoMethod* FindMethod(MonoClass* monoClass, const char* name,
                                  int parameterCount) {
        for (MonoClass* current = monoClass; current;
             current = mono_class_get_parent(current)) {
            if (MonoMethod* method = mono_class_get_method_from_name(
                    current, name, parameterCount))
                return method;
        }
        return nullptr;
    }

    static bool Invoke(MonoObject* instance, MonoMethod* method, void** args) {
        if (!instance || !method) return false;
        MonoObject* exception = nullptr;
        mono_runtime_invoke(method, instance, args, &exception);
        ReportMonoException(exception);
        return exception == nullptr;
    }

    static ScriptEngineData::ScriptMethods& MethodsFor(MonoClass* monoClass) {
        const auto found = s_Data->MethodCache.find(monoClass);
        if (found != s_Data->MethodCache.end()) return found->second;
        ScriptEngineData::ScriptMethods methods;
        methods.OnCreate = FindMethod(monoClass, "OnCreate", 0);
        methods.OnStart = FindMethod(monoClass, "OnStart", 0);
        methods.OnUpdate = FindMethod(monoClass, "OnUpdate", 1);
        methods.OnDestroy = FindMethod(monoClass, "OnDestroy", 0);
        return s_Data->MethodCache.emplace(monoClass, methods).first->second;
    }

    static bool SetScriptInstanceEntityID(MonoObject* instance,
                                          const Entity& entity) {
        if (!s_Data->EntityIdSetter) {
            MonoClass* entityClass = mono_class_from_name(
                s_Data->CoreAssemblyImage, "Archura", "Entity");
            s_Data->EntityIdSetter = entityClass
                ? mono_class_get_method_from_name(entityClass, "set_ID", 1)
                : nullptr;
        }
        if (!s_Data->EntityIdSetter) return false;

        uint64_t entityId = entity.GetHandle().Value();
        void* args[1] = { &entityId };
        return Invoke(instance, s_Data->EntityIdSetter, args);
    }

    void ScriptEngine::Init() {
        if (s_Data) return;
        s_Data = new ScriptEngineData();

        InitMono();

        // Only proceed if Mono initialized successfully
        if (s_Data->RootDomain == nullptr) {
            std::cout << "[ScriptEngine] Scripting unavailable (Mono not loaded)." << std::endl;
            return;
        }

        std::filesystem::path coreAssembly = "Resources/Scripts/net472/ScriptCore.dll";
        if (!std::filesystem::exists(coreAssembly)) {
            coreAssembly = "Resources/Scripts/ScriptCore.dll";
        }
        LoadAssembly(coreAssembly);
        ScriptGlue::RegisterFunctions();
        std::cout << "ScriptEngine Initialized!" << std::endl;
    }

    void ScriptEngine::Shutdown() {
        if (!s_Data) return;
        OnRuntimeStop();
        ShutdownMono();
        delete s_Data;
        s_Data = nullptr;
    }

    void ScriptEngine::InitMono() {
        std::filesystem::path exePath = std::filesystem::current_path();
        std::filesystem::path assemblyPath = exePath / "mono/lib";
        std::filesystem::path configPath   = exePath / "mono/etc";

        // Guard: if Mono runtime DLLs aren't deployed, skip init entirely.
        // mono_jit_init will hard-crash if the DLL is missing from PATH on Windows.
        if (!std::filesystem::exists(assemblyPath)) {
            std::cout << "[ScriptEngine] Mono lib path not found (" << assemblyPath
                      << "). Scripting disabled." << std::endl;
            return;
        }

        mono_set_dirs(assemblyPath.string().c_str(), configPath.string().c_str());
        std::cout << "[ScriptEngine] Setting Mono Assembly Path: " << assemblyPath << std::endl;

        MonoDomain* rootDomain = mono_jit_init("ArchuraJIT");
        if (rootDomain == nullptr) {
            std::cout << "[ScriptEngine] Failed to initialize Mono JIT!" << std::endl;
            return;
        }
        s_Data->RootDomain = rootDomain;
    }

    void ScriptEngine::ShutdownMono() {
        if (!s_Data) return;
        if (s_Data->AppDomain) {
            mono_domain_set(s_Data->RootDomain, true);
            mono_domain_unload(s_Data->AppDomain);
            s_Data->AppDomain = nullptr;
        }
        s_Data->CoreAssembly = nullptr;
        s_Data->CoreAssemblyImage = nullptr;
        s_Data->MethodCache.clear();
        s_Data->EntityIdSetter = nullptr;
        if (s_Data->RootDomain) {
            MonoDomain* root = s_Data->RootDomain;
            s_Data->RootDomain = nullptr;
            mono_jit_cleanup(root);
        }
    }

    void ScriptEngine::LoadAssembly(const std::filesystem::path& filepath) {
        if (!s_Data || !s_Data->RootDomain) {
            std::cout << "[ScriptEngine] Cannot load assembly before Mono is initialized." << std::endl;
            return;
        }

        if (!std::filesystem::exists(filepath)) {
            std::cout << "[ScriptEngine] Assembly not found: " << filepath << std::endl;
            return;
        }

        OnRuntimeStop();
        if (s_Data->AppDomain) {
            mono_domain_set(s_Data->RootDomain, true);
            mono_domain_unload(s_Data->AppDomain);
            s_Data->AppDomain = nullptr;
        }
        s_Data->CoreAssembly = nullptr;
        s_Data->CoreAssemblyImage = nullptr;
        s_Data->MethodCache.clear();
        s_Data->EntityIdSetter = nullptr;

        s_Data->AppDomain = mono_domain_create_appdomain(
            const_cast<char*>("ArchuraScriptRuntime"), nullptr);
        if (!s_Data->AppDomain || !mono_domain_set(s_Data->AppDomain, true)) {
            std::cout << "[ScriptEngine] Failed to create/set app domain\n";
            s_Data->AppDomain = nullptr;
            return;
        }

        s_Data->CoreAssemblyPath = filepath;
        s_Data->CoreAssembly = LoadMonoAssembly(filepath);
        if (s_Data->CoreAssembly)
            s_Data->CoreAssemblyImage = mono_assembly_get_image(s_Data->CoreAssembly);
        else {
            std::cout << "Could not load assembly: " << filepath << std::endl;
            mono_domain_set(s_Data->RootDomain, true);
            mono_domain_unload(s_Data->AppDomain);
            s_Data->AppDomain = nullptr;
        }
    }

    void ScriptEngine::OnRuntimeStart(Scene* /*scene*/) {
        // Override with scene context
    }

    void ScriptEngine::OnRuntimeStop() {
        if (!s_Data) return;
        for (const auto& entry : s_Data->EntityScriptInstances) {
            MonoObject* instance = mono_gchandle_get_target(entry.second);
            if (instance) {
                auto& methods = MethodsFor(mono_object_get_class(instance));
                if (methods.OnDestroy) Invoke(instance, methods.OnDestroy, nullptr);
            }
            mono_gchandle_free(entry.second);
        }
        s_Data->EntityScriptInstances.clear();
    }

    bool ScriptEngine::OnCreateEntity(const Entity& entity) {
        if (!s_Data || !s_Data->CoreAssemblyImage) return false;
        if (!entity.HasComponent<ScriptComponent>()) return false;
        
        auto& sc = entity.GetComponent<ScriptComponent>()->className;
        if (sc.empty()) return false;

        if (ClassExists(sc)) {
             MonoClass* monoClass = mono_class_from_name(s_Data->CoreAssemblyImage, "Archura", sc.c_str());
             MonoObject* instance = InstantiateClass(monoClass);
             if (!instance) return false;
             if (!SetScriptInstanceEntityID(instance, entity)) return false;
             const uint64_t key = entity.GetHandle().Value();
             const auto existing = s_Data->EntityScriptInstances.find(key);
             if (existing != s_Data->EntityScriptInstances.end()) {
                 mono_gchandle_free(existing->second);
                 s_Data->EntityScriptInstances.erase(existing);
             }
             s_Data->EntityScriptInstances.emplace(
                 key, mono_gchandle_new(instance, false));
             
             auto& methods = MethodsFor(monoClass);
             if (!methods.OnCreate ||
                 !Invoke(instance, methods.OnCreate, nullptr) ||
                 !methods.OnStart ||
                 !Invoke(instance, methods.OnStart, nullptr)) {
                 if (methods.OnDestroy)
                     Invoke(instance, methods.OnDestroy, nullptr);
                 const auto failed = s_Data->EntityScriptInstances.find(key);
                 mono_gchandle_free(failed->second);
                 s_Data->EntityScriptInstances.erase(failed);
                 return false;
             }
             return true;
        }
        return false;
    }
    
    bool ScriptEngine::OnUpdateEntity(const Entity& entity, float ts) {
        if (!s_Data) return false;
        const uint64_t key = entity.GetHandle().Value();
        const auto found = s_Data->EntityScriptInstances.find(key);
        if (found == s_Data->EntityScriptInstances.end()) return false;

        MonoObject* instance = mono_gchandle_get_target(found->second);
        if (!instance) return false;
        MonoClass* monoClass = mono_object_get_class(instance);
        auto& methods = MethodsFor(monoClass);
        if (methods.OnUpdate) {
             void* args[1];
             args[0] = &ts;
             return Invoke(instance, methods.OnUpdate, args);
        }
        return false;
    }

    void ScriptEngine::OnDestroyEntity(EntityHandle entity) {
        if (!s_Data) return;
        const auto found = s_Data->EntityScriptInstances.find(entity.Value());
        if (found == s_Data->EntityScriptInstances.end()) return;

        MonoObject* instance = mono_gchandle_get_target(found->second);
        if (instance) {
            auto& methods = MethodsFor(mono_object_get_class(instance));
            if (methods.OnDestroy) Invoke(instance, methods.OnDestroy, nullptr);
        }
        mono_gchandle_free(found->second);
        s_Data->EntityScriptInstances.erase(found);
    }

    MonoObject* ScriptEngine::InstantiateClass(MonoClass* monoClass) {
        if (!s_Data || !s_Data->AppDomain || !monoClass) return nullptr;
        MonoObject* instance = mono_object_new(s_Data->AppDomain, monoClass);
        if (!instance) return nullptr;
        MonoMethod* constructor = mono_class_get_method_from_name(
            monoClass, ".ctor", 0);
        if (!constructor || !Invoke(instance, constructor, nullptr))
            return nullptr;
        return instance;
    }

    bool ScriptEngine::ClassExists(const std::string& fullClassName) {
        if (!s_Data || !s_Data->CoreAssemblyImage) return false;
        // Currently assuming "Archura" namespace for everything or simpler lookup
        // Ideally parse fullClassName for namespace
        // For simplicity:
        MonoClass* monoClass = mono_class_from_name(s_Data->CoreAssemblyImage, "Archura", fullClassName.c_str());
        return monoClass != nullptr;
    }

    MonoImage* ScriptEngine::GetCoreAssemblyImage() {
        return s_Data ? s_Data->CoreAssemblyImage : nullptr;
    }

}
