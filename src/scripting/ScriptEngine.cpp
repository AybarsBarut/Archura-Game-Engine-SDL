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
        MonoDomain* RootDomain = nullptr;
        MonoDomain* AppDomain = nullptr;

        MonoAssembly* CoreAssembly = nullptr;
        MonoImage* CoreAssemblyImage = nullptr;

        std::filesystem::path CoreAssemblyPath;
        
        // Map Entity ID -> Script Instance
        std::unordered_map<uint32_t, MonoObject*> EntityScriptInstances;
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
        MonoString* exceptionString = mono_object_to_string(exception, nullptr);
        char* message = mono_string_to_utf8(exceptionString);
        std::cout << "[ScriptEngine] Exception: " << message << std::endl;
        mono_free(message);
    }

    static bool InvokeNoArg(MonoObject* instance, MonoClass* monoClass, const char* methodName) {
        MonoMethod* method = nullptr;
        for (MonoClass* current = monoClass; current && !method; current = mono_class_get_parent(current)) {
            method = mono_class_get_method_from_name(current, methodName, 0);
        }
        if (!method) return false;

        MonoObject* exception = nullptr;
        mono_runtime_invoke(method, instance, nullptr, &exception);
        ReportMonoException(exception);
        return exception == nullptr;
    }

    static void SetScriptInstanceEntityID(MonoObject* instance, MonoClass* /*monoClass*/, Entity entity) {
        MonoClass* entityClass = mono_class_from_name(s_Data->CoreAssemblyImage, "Archura", "Entity");
        MonoMethod* setId = entityClass ? mono_class_get_method_from_name(entityClass, "set_ID", 1) : nullptr;
        if (!setId) return;

        uint64_t entityId = entity.GetID();
        void* args[1] = { &entityId };
        MonoObject* exception = nullptr;
        mono_runtime_invoke(setId, instance, args, &exception);
        ReportMonoException(exception);
    }

    void ScriptEngine::Init() {
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
        ShutdownMono();
        delete s_Data;
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
        // mono_domain_unload(s_Data->AppDomain); // Often causes crashes if not careful
        s_Data->AppDomain = nullptr;
        
        // mono_jit_cleanup(s_Data->RootDomain);
        s_Data->RootDomain = nullptr;
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

        s_Data->AppDomain = mono_domain_create_appdomain("ArchuraScriptRuntime", nullptr);
        mono_domain_set(s_Data->AppDomain, true);

        s_Data->CoreAssemblyPath = filepath;
        s_Data->CoreAssembly = LoadMonoAssembly(filepath);
        if (s_Data->CoreAssembly)
            s_Data->CoreAssemblyImage = mono_assembly_get_image(s_Data->CoreAssembly);
        else
            std::cout << "Could not load assembly: " << filepath << std::endl;
    }

    void ScriptEngine::OnRuntimeStart(Scene* /*scene*/) {
        // Override with scene context
    }

    void ScriptEngine::OnRuntimeStop() {
        // Clear context
    }

    void ScriptEngine::OnCreateEntity(Entity entity) {
        if (!s_Data || !s_Data->CoreAssemblyImage) return;
        if (!entity.HasComponent<ScriptComponent>()) return;
        
        auto& sc = entity.GetComponent<ScriptComponent>()->className;
        if (sc.empty()) return;

        if (ClassExists(sc)) {
             MonoClass* monoClass = mono_class_from_name(s_Data->CoreAssemblyImage, "Archura", sc.c_str());
             MonoObject* instance = InstantiateClass(monoClass);
             SetScriptInstanceEntityID(instance, monoClass, entity);
             s_Data->EntityScriptInstances[entity.GetID()] = instance;
             
             InvokeNoArg(instance, monoClass, "OnCreate");
             InvokeNoArg(instance, monoClass, "OnStart");
        }
    }
    
    void ScriptEngine::OnUpdateEntity(Entity entity, float ts) {
        if (!s_Data) return;
        if (s_Data->EntityScriptInstances.find(entity.GetID()) == s_Data->EntityScriptInstances.end()) {
            return;
        }

        MonoObject* instance = s_Data->EntityScriptInstances.at(entity.GetID());
        MonoClass* monoClass = mono_object_get_class(instance);
        
        // Optimize: Cache this method
        MonoMethod* onUpdateMethod = nullptr;
        for (MonoClass* current = monoClass; current && !onUpdateMethod; current = mono_class_get_parent(current)) {
            onUpdateMethod = mono_class_get_method_from_name(current, "OnUpdate", 1);
        }
        if (onUpdateMethod) {
             void* args[1];
             args[0] = &ts;
             MonoObject* exception = nullptr;
             mono_runtime_invoke(onUpdateMethod, instance, args, &exception);
             ReportMonoException(exception);
        }
    }

    MonoObject* ScriptEngine::InstantiateClass(MonoClass* monoClass) {
        MonoObject* instance = mono_object_new(s_Data->AppDomain, monoClass);
        mono_runtime_object_init(instance);
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
        return s_Data->CoreAssemblyImage;
    }

}
