# Native contracts, optional benchmarks and graphical startup fixtures.
# Included from the project root after runtime/dependency targets are defined.

if(BUILD_TESTING)
    add_executable(ArchuraPendingSendQueueTests tests/network/PendingSendQueueTests.cpp)
    target_include_directories(ArchuraPendingSendQueueTests PRIVATE src)
    add_test(NAME Archura.Network.PendingSendQueue COMMAND ArchuraPendingSendQueueTests)
    if(ARCHURA_SDL_NET_TARGET)
        add_executable(ArchuraNetworkConnectionTests
            tests/network/NetworkConnectionTests.cpp
            src/network/NetworkManager.cpp src/network/NetworkProtocol.cpp
            src/network/NativeSocket.cpp src/network/AsyncResolver.cpp)
        target_include_directories(ArchuraNetworkConnectionTests PRIVATE src)
        target_compile_definitions(ArchuraNetworkConnectionTests PRIVATE ARCHURA_HAS_SDL_NET=1)
        if(WIN32)
            target_link_libraries(ArchuraNetworkConnectionTests PRIVATE ws2_32)
        endif()
        add_test(NAME Archura.Network.Connection COMMAND ArchuraNetworkConnectionTests)
        set_tests_properties(Archura.Network.Connection PROPERTIES RUN_SERIAL TRUE)
        add_executable(ArchuraNetworkBackpressureTests
            tests/network/NetworkBackpressureTests.cpp
            src/network/NetworkManager.cpp src/network/NetworkProtocol.cpp
            src/network/NativeSocket.cpp src/network/AsyncResolver.cpp)
        target_include_directories(ArchuraNetworkBackpressureTests PRIVATE src)
        target_compile_definitions(ArchuraNetworkBackpressureTests PRIVATE ARCHURA_HAS_SDL_NET=1)
        if(WIN32)
            target_link_libraries(ArchuraNetworkBackpressureTests PRIVATE ws2_32)
        endif()
    endif()
    add_executable(ArchuraWeaponSerializationTests tests/WeaponSerializationTests.cpp)
    target_include_directories(ArchuraWeaponSerializationTests PRIVATE src external/glm)
    add_test(NAME Archura.Core.WeaponSerialization COMMAND ArchuraWeaponSerializationTests)

    add_executable(ArchuraLoggerTests tests/LoggerTests.cpp src/core/Logger.cpp)
    target_include_directories(ArchuraLoggerTests PRIVATE src)
    add_test(NAME Archura.Core.Logger COMMAND ArchuraLoggerTests)

    add_executable(ArchuraProjectDirectoryCacheTests
        tests/ProjectDirectoryCacheTests.cpp src/editor/ProjectDirectoryCache.cpp)
    target_include_directories(ArchuraProjectDirectoryCacheTests PRIVATE src)
    add_test(NAME Archura.Editor.ProjectDirectoryCache COMMAND ArchuraProjectDirectoryCacheTests)

    add_executable(ArchuraSceneViewportTests tests/SceneViewportTests.cpp)
    target_include_directories(ArchuraSceneViewportTests PRIVATE src external/glm)
    add_test(NAME Archura.Editor.SceneViewport COMMAND ArchuraSceneViewportTests)

    add_executable(ArchuraProjectSessionTests tests/ProjectSessionTests.cpp src/core/ProjectSession.cpp)
    target_include_directories(ArchuraProjectSessionTests PRIVATE src)
    add_test(NAME Archura.Core.ProjectSession COMMAND ArchuraProjectSessionTests)

    add_executable(ArchuraSceneDocumentStateTests tests/SceneDocumentStateTests.cpp)
    target_include_directories(ArchuraSceneDocumentStateTests PRIVATE src)
    add_test(NAME Archura.Editor.SceneDocumentState COMMAND ArchuraSceneDocumentStateTests)

    add_executable(ArchuraInspectorTextStateTests tests/InspectorTextStateTests.cpp
        src/ecs/Entity.cpp src/ecs/Component.cpp)
    target_include_directories(ArchuraInspectorTextStateTests PRIVATE src external/glm)
    add_test(NAME Archura.Editor.InspectorTextState COMMAND ArchuraInspectorTextStateTests)

    add_executable(ArchuraLauncherDiscoveryTests src/launcher/RuntimeDiscoveryTests.cpp)
    target_include_directories(ArchuraLauncherDiscoveryTests PRIVATE src/launcher)
    add_test(NAME Archura.Launcher.RuntimeDiscovery COMMAND ArchuraLauncherDiscoveryTests)

    add_executable(ArchuraEntityEditCommitTests
        tests/EntityEditCommitTests.cpp src/ecs/Component.cpp src/ecs/Entity.cpp)
    target_include_directories(ArchuraEntityEditCommitTests PRIVATE src external/glm)
    add_test(NAME Archura.ECS.EntityEditCommit COMMAND ArchuraEntityEditCommitTests)

    add_executable(ArchuraStructuralHistoryTests
        tests/StructuralHistoryTests.cpp src/editor/HierarchyEditCommand.cpp src/ecs/Component.cpp src/ecs/Entity.cpp)
    target_include_directories(ArchuraStructuralHistoryTests PRIVATE src external/glm)
    add_test(NAME Archura.Editor.StructuralHistory COMMAND ArchuraStructuralHistoryTests)

    add_executable(ArchuraTransformUndoTests
        tests/TransformUndoHistoryTests.cpp src/ecs/Component.cpp src/ecs/Entity.cpp)
    target_include_directories(ArchuraTransformUndoTests PRIVATE src external/glm)
    add_test(NAME Archura.Editor.TransformUndo COMMAND ArchuraTransformUndoTests)

    add_executable(ArchuraMeshIdentityTests
        tests/MeshIdentityTests.cpp src/rendering/Mesh.cpp
        src/rendering/Shader.cpp src/core/Logger.cpp)
    target_include_directories(ArchuraMeshIdentityTests PRIVATE src external/glm external/glad/include)
    target_link_libraries(ArchuraMeshIdentityTests PRIVATE glad ufbx fast_obj)
    add_test(NAME Archura.Rendering.MeshIdentity COMMAND ArchuraMeshIdentityTests)

    add_executable(ArchuraGraphicsAPISelectionTests
        tests/GraphicsAPISelectionTests.cpp
        src/rendering/GraphicsAPI.cpp
    )
    target_include_directories(ArchuraGraphicsAPISelectionTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    add_test(NAME Archura.Rendering.GraphicsAPISelection
             COMMAND ArchuraGraphicsAPISelectionTests)

    add_executable(ArchuraDirectionalShadowTests tests/DirectionalShadowTests.cpp)
    target_include_directories(ArchuraDirectionalShadowTests PRIVATE src external/glm)
    add_test(NAME Archura.Rendering.DirectionalShadow COMMAND ArchuraDirectionalShadowTests)

    add_executable(ArchuraDevConsoleInputTests
        tests/DevConsoleInputTests.cpp src/game/DevConsole.cpp
        src/core/DeveloperConsole.cpp src/core/ConsoleCommand.cpp src/input/Input.cpp)
    target_include_directories(ArchuraDevConsoleInputTests PRIVATE src external/glm)
    target_link_libraries(ArchuraDevConsoleInputTests PRIVATE imgui)
    add_custom_command(TARGET ArchuraDevConsoleInputTests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SDL2_BasePath}/lib/x64/SDL2.dll" "$<TARGET_FILE_DIR:ArchuraDevConsoleInputTests>/SDL2.dll")
    add_test(NAME Archura.Editor.DevConsoleInput COMMAND ArchuraDevConsoleInputTests)

    add_executable(ArchuraEditorPlayStopTests
        tests/EditorPlayStopLifecycleTests.cpp
        src/editor/PlayModeSession.cpp
        src/ecs/Component.cpp
        src/ecs/Entity.cpp
        src/game/PhysicsSystem.cpp
        src/game/FPSController.cpp
        src/game/Weapon.cpp
        src/game/Projectile.cpp
        src/game/ProjectileSystem.cpp
        src/game/ParticleSystem.cpp
        src/input/Input.cpp
        src/rendering/Camera.cpp
        src/rendering/Mesh.cpp
        src/rendering/Shader.cpp
        src/rendering/Texture.cpp
        src/core/ResourceManager.cpp
        src/core/Logger.cpp
    )
    target_include_directories(ArchuraEditorPlayStopTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/external/glm
        ${CMAKE_CURRENT_SOURCE_DIR}/external/glad/include
        ${CMAKE_CURRENT_SOURCE_DIR}/external/stb
        ${SDL2_INCLUDE_DIR}
    )
    target_link_libraries(ArchuraEditorPlayStopTests PRIVATE
        glad ufbx fast_obj
    )
    add_test(NAME Archura.Editor.PlayStopLifecycle
             COMMAND ArchuraEditorPlayStopTests)

    add_executable(ArchuraWeaponProgressionTests
        tests/WeaponProgressionTests.cpp
        src/ecs/Component.cpp
        src/ecs/Entity.cpp
        src/game/Weapon.cpp
    )
    target_include_directories(ArchuraWeaponProgressionTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/external/glm
        ${SDL2_INCLUDE_DIR}
    )
    add_test(NAME Archura.Weapon.Progression COMMAND ArchuraWeaponProgressionTests)

    add_executable(ArchuraJobSystemTests
        tests/JobSystemTests.cpp
        src/core/threading/JobSystem.cpp
    )
    target_include_directories(ArchuraJobSystemTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    add_test(NAME Archura.Core.JobSystem COMMAND ArchuraJobSystemTests)
    add_test(NAME Archura.Core.JobSystem.EnqueueFailure COMMAND ArchuraJobSystemTests enqueue-failure)
    add_test(NAME Archura.Core.JobSystem.WorkerCopy COMMAND ArchuraJobSystemTests worker-copy)
    add_test(NAME Archura.Core.JobSystem.InitFailure COMMAND ArchuraJobSystemTests init-failure)
    target_compile_definitions(ArchuraJobSystemTests PRIVATE ARCHURA_JOB_SYSTEM_TESTS=1)
    set_tests_properties(Archura.Core.JobSystem Archura.Core.JobSystem.EnqueueFailure
        Archura.Core.JobSystem.WorkerCopy Archura.Core.JobSystem.InitFailure
        PROPERTIES LABELS "contract;jobs" TIMEOUT 30)

    add_executable(ArchuraProjectSerializerTests
        tests/ProjectSerializerTests.cpp
        src/core/ProjectSerializer.cpp
        src/core/GameSaveManager.cpp
        src/rendering/Mesh.cpp
        src/rendering/Texture.cpp
        src/rendering/Shader.cpp
        src/core/Logger.cpp
        src/ecs/Component.cpp
        src/ecs/Entity.cpp
        src/core/ResourceManager.cpp
        src/rendering/Camera.cpp
    )
    target_include_directories(ArchuraProjectSerializerTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/external/glm
        ${CMAKE_CURRENT_SOURCE_DIR}/external/stb
    )
    target_link_libraries(ArchuraProjectSerializerTests PRIVATE glad ufbx fast_obj)
    add_test(NAME Archura.Core.ProjectSerializer COMMAND ArchuraProjectSerializerTests)

    add_executable(ArchuraGameSaveManagerTests
        tests/GameSaveManagerTests.cpp src/editor/TextureEditCommand.cpp src/editor/EntityEditCommand.cpp src/core/GameSaveManager.cpp
        src/rendering/Mesh.cpp src/rendering/Texture.cpp
        src/rendering/Shader.cpp src/core/Logger.cpp
        src/ecs/Component.cpp src/ecs/Entity.cpp src/core/ResourceManager.cpp src/rendering/Camera.cpp src/rendering/Terrain.cpp src/core/AudioSystem.cpp src/editor/GameBuilderPanel.cpp)
    target_include_directories(ArchuraGameSaveManagerTests PRIVATE src external/glm external/stb external/imgui ${SDL2_INCLUDE_DIR})
    target_link_libraries(ArchuraGameSaveManagerTests PRIVATE glad ufbx fast_obj imgui SDL2)
    add_test(NAME Archura.Core.GameSaveManager COMMAND ArchuraGameSaveManagerTests)

    add_executable(ArchuraServerConfigTests
        tests/ServerConfigTests.cpp
        src/network/ServerConfig.cpp
    )
    target_include_directories(ArchuraServerConfigTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    add_test(NAME Archura.Network.ServerConfig COMMAND ArchuraServerConfigTests)

    add_executable(ArchuraGameplayScrollTests
        tests/GameplayScrollTests.cpp
        src/game/FPSController.cpp
        src/input/Input.cpp
        src/rendering/Camera.cpp
    )
    target_include_directories(ArchuraGameplayScrollTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/external/glm
        ${SDL2_INCLUDE_DIR}
    )
    add_test(NAME Archura.Gameplay.Scroll COMMAND ArchuraGameplayScrollTests)

    add_executable(ArchuraInputMouseButtonTests
        tests/InputMouseButtonTests.cpp
        src/input/Input.cpp
    )
    target_include_directories(ArchuraInputMouseButtonTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/external/glm
        ${SDL2_INCLUDE_DIR}
    )
    add_test(NAME Archura.Input.MouseButtons COMMAND ArchuraInputMouseButtonTests)

    add_executable(ArchuraFrameTelemetryTests
        tests/FrameTelemetryTests.cpp
        src/core/FrameTelemetry.cpp
    )
    target_include_directories(ArchuraFrameTelemetryTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    add_test(NAME Archura.Core.FrameTelemetry COMMAND ArchuraFrameTelemetryTests)

    add_executable(ArchuraNetworkProtocolTests
        tests/network/NetworkProtocolTests.cpp
        src/network/NetworkProtocol.cpp
    )
    target_include_directories(ArchuraNetworkProtocolTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    add_test(NAME ArchuraNetworkProtocolTests COMMAND ArchuraNetworkProtocolTests)

    # Intentionally compiled without ARCHURA_HAS_SDL_NET. This verifies that
    # source archives without the optional SDK still produce a safe, explicit
    # runtime stub instead of link failures or a false connected state.
    add_executable(ArchuraNetworkNoBackendTests
        tests/network/NetworkNoBackendTests.cpp
        src/network/NetworkManager.cpp
        src/network/NetworkProtocol.cpp
    )
    target_include_directories(ArchuraNetworkNoBackendTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    add_test(NAME ArchuraNetworkNoBackendTests COMMAND ArchuraNetworkNoBackendTests)

    add_executable(ArchuraNetworkConsoleTests
        tests/network/NetworkConsoleTests.cpp
        src/network/NetworkConsole.cpp
        src/network/NetworkManager.cpp
        src/network/NetworkProtocol.cpp
    )
    target_include_directories(ArchuraNetworkConsoleTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_compile_definitions(ArchuraNetworkConsoleTests PRIVATE
        ARCHURA_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    )
    add_test(NAME Archura.Network.Console COMMAND ArchuraNetworkConsoleTests)
endif()

if(ARCHURA_BUILD_MEMORY_BENCHMARKS)
    add_executable(ArchuraAllocatorBenchmark
        benchmarks/AllocatorBenchmark.cpp
        src/core/memory/MemoryTracker.cpp
    )

    target_include_directories(ArchuraAllocatorBenchmark PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_compile_definitions(ArchuraAllocatorBenchmark PRIVATE
        "$<$<CONFIG:Debug>:ARCHURA_DEBUG>"
    )
endif()

if(ARCHURA_BUILD_ECS_BENCHMARKS)
    add_executable(ArchuraECSStorageBenchmark
        benchmarks/ECSStorageBenchmark.cpp
        src/ecs/Component.cpp
    )

    target_include_directories(ArchuraECSStorageBenchmark PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/external/glm
    )
endif()

if(BUILD_TESTING AND ARCHURA_BUILD_RENDERER_TESTS)
    add_executable(ArchuraRendererOwnershipTests
        tests/RendererOwnershipTests.cpp
    )
    target_include_directories(ArchuraRendererOwnershipTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/external/glm
    )
    add_test(NAME Archura.Renderer.Ownership COMMAND ArchuraRendererOwnershipTests)
endif()

if(BUILD_TESTING AND ARCHURA_BUILD_ECS_TESTS)
    include(CTest)
    add_executable(ArchuraECSCoreTests
        tests/ECSCoreTests.cpp
        src/ecs/Component.cpp
        src/ecs/Entity.cpp
    )
    target_include_directories(ArchuraECSCoreTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/external/glm
    )
    add_test(NAME Archura.ECS.Core COMMAND ArchuraECSCoreTests)
endif()

if(BUILD_TESTING AND ARCHURA_BUILD_PHYSICS_TESTS)
    include(CTest)
    add_executable(ArchuraPhysicsCoreTests
        tests/PhysicsCoreTests.cpp
        src/ecs/Component.cpp
        src/ecs/Entity.cpp
        src/game/PhysicsSystem.cpp
    )
    target_include_directories(ArchuraPhysicsCoreTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/external/glm
    )
    add_test(NAME Archura.Physics.Core COMMAND ArchuraPhysicsCoreTests)
    add_executable(ArchuraPhysicsPresentationTests tests/PhysicsPresentationTests.cpp
        src/core/GameLoopHarness.cpp src/ecs/Component.cpp src/ecs/Entity.cpp src/game/PhysicsSystem.cpp)
    target_include_directories(ArchuraPhysicsPresentationTests PRIVATE src external/glm)
    add_test(NAME Archura.Physics.Presentation COMMAND ArchuraPhysicsPresentationTests)
    add_executable(ArchuraPhysicsGrabTests tests/PhysicsGrabTests.cpp
        src/ecs/Component.cpp src/ecs/Entity.cpp src/game/PhysicsSystem.cpp)
    target_include_directories(ArchuraPhysicsGrabTests PRIVATE src external/glm)
    add_test(NAME Archura.Physics.Grab COMMAND ArchuraPhysicsGrabTests)
    add_executable(ArchuraSurfaceAttachmentTests tests/SurfaceAttachmentTests.cpp
        src/ecs/Component.cpp src/ecs/Entity.cpp)
    target_include_directories(ArchuraSurfaceAttachmentTests PRIVATE src external/glm)
    add_test(NAME Archura.Projectile.SurfaceAttachment COMMAND ArchuraSurfaceAttachmentTests)
endif()

if(BUILD_TESTING)
    add_executable(ArchuraGameManifestTests tests/GameManifestTests.cpp)
    target_include_directories(ArchuraGameManifestTests PRIVATE src)
    add_test(NAME Archura.Core.GameManifest COMMAND ArchuraGameManifestTests)
    add_executable(ArchuraAuthoritativeWorldTests tests/network/AuthoritativeWorldTests.cpp
        src/network/AuthoritativeWorld.cpp src/network/NetworkProtocol.cpp src/ecs/Entity.cpp src/ecs/Component.cpp)
    target_include_directories(ArchuraAuthoritativeWorldTests PRIVATE src external/glm)
    add_test(NAME Archura.Network.AuthoritativeWorld COMMAND ArchuraAuthoritativeWorldTests)
    add_executable(ArchuraSnapshotInterpolationTests tests/network/SnapshotInterpolationTests.cpp
        src/network/AuthoritativeWorld.cpp src/network/NetworkProtocol.cpp)
    target_include_directories(ArchuraSnapshotInterpolationTests PRIVATE src)
    add_test(NAME Archura.Network.SnapshotInterpolation COMMAND ArchuraSnapshotInterpolationTests)
    find_package(Python3 3.9 COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND)
        add_test(NAME Archura.Packaging.Contracts
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/PackageGameTests.py")
        add_test(NAME Archura.Build.LocalValidation
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/LocalValidationTests.py")
        add_test(NAME Archura.Build.DependencyChecker
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/DependencyCheckTests.py")
    endif()
    get_property(archura_contract_tests DIRECTORY PROPERTY TESTS)
    set_tests_properties(${archura_contract_tests} PROPERTIES LABELS "contract" TIMEOUT 30)
    if(Python3_Interpreter_FOUND)
        add_test(NAME Archura.Network.ServerConfigProcess
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/network/ServerConfigProcessTests.py"
                --server "$<TARGET_FILE:ArchuraServer>")
        set_tests_properties(Archura.Network.ServerConfigProcess PROPERTIES
            LABELS "network;integration" TIMEOUT 30 RUN_SERIAL TRUE)
    endif()
    if(Python3_Interpreter_FOUND AND ARCHURA_SDL_NET_TARGET)
        add_test(NAME Archura.Network.Backpressure
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/network/NetworkBackpressureProcessTests.py"
                --fixture "$<TARGET_FILE:ArchuraNetworkBackpressureTests>")
        set_tests_properties(Archura.Network.Backpressure PROPERTIES
            LABELS "network;integration" TIMEOUT 15 RUN_SERIAL TRUE)
        add_test(NAME Archura.Network.ProcessIntegration
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/network/NetworkProcessTests.py"
                --server "$<TARGET_FILE:ArchuraServer>" --client "$<TARGET_FILE:ArchuraNetClient>")
        set_tests_properties(Archura.Network.ProcessIntegration PROPERTIES
            LABELS "network;integration" TIMEOUT 40 RUN_SERIAL TRUE)
    endif()
    if(ARCHURA_BUILD_STARTUP_TESTS)
        if(Python3_Interpreter_FOUND AND ARCHURA_SDL_NET_TARGET)
            add_test(NAME Archura.Network.GraphicalIntegration
                COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/network/GraphicalNetworkProcessTests.py"
                    --server "$<TARGET_FILE:ArchuraServer>" --engine "$<TARGET_FILE:ArchuraEngine>")
            set_tests_properties(Archura.Network.GraphicalIntegration PROPERTIES
                LABELS "startup;integration;network" TIMEOUT 60 RUN_SERIAL TRUE)
        endif()
        if(Python3_Interpreter_FOUND)
            add_test(NAME Archura.Startup.ProjectSession
                COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/EditorProjectStartupTests.py"
                    --engine "$<TARGET_FILE:ArchuraEngine>")
            set_tests_properties(Archura.Startup.ProjectSession PROPERTIES
                LABELS "startup;integration" TIMEOUT 150 RUN_SERIAL TRUE)
            add_test(NAME Archura.Startup.Failure
                COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/StartupFailureSmoke.py"
                    --engine "$<TARGET_FILE:ArchuraEngine>")
            set_tests_properties(Archura.Startup.Failure PROPERTIES
                LABELS "startup;integration" TIMEOUT 100 RUN_SERIAL TRUE)
            add_test(NAME Archura.Startup.StandalonePackage
                COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/StandalonePackageSmoke.py"
                    --runtime-dir "$<TARGET_FILE_DIR:ArchuraEngine>")
            set_tests_properties(Archura.Startup.StandalonePackage PROPERTIES
                LABELS "startup;integration;packaging" TIMEOUT 90 RUN_SERIAL TRUE)
        endif()
        add_test(NAME Archura.Startup.OpenGL
            COMMAND ArchuraEngine --smoke-test --graphics=opengl --no-graphics-fallback)
        add_test(NAME Archura.Startup.UnsupportedBackend
            COMMAND ArchuraEngine --smoke-test --graphics=vulkan --no-graphics-fallback)
        add_test(NAME Archura.Startup.PlayStop
            COMMAND ArchuraEngine --play-stop-smoke-test --graphics=opengl --no-graphics-fallback)
        add_test(NAME Archura.Startup.Cancel
            COMMAND ArchuraEngine --startup-cancel-test --graphics=opengl --no-graphics-fallback)
        add_test(NAME Archura.Startup.Splash
            COMMAND ArchuraEngine --startup-smoke-test --graphics=opengl --no-graphics-fallback)
        add_test(NAME Archura.Startup.SplashAcceleratedFramebuffer
            COMMAND ArchuraEngine --startup-smoke-test --graphics=opengl --no-graphics-fallback)
        set_tests_properties(Archura.Startup.SplashAcceleratedFramebuffer PROPERTIES
            ENVIRONMENT "SDL_FRAMEBUFFER_ACCELERATION=opengl")
        set_tests_properties(Archura.Startup.UnsupportedBackend PROPERTIES WILL_FAIL TRUE)
        set_tests_properties(Archura.Startup.OpenGL Archura.Startup.UnsupportedBackend Archura.Startup.PlayStop Archura.Startup.Cancel Archura.Startup.Splash Archura.Startup.SplashAcceleratedFramebuffer PROPERTIES
            LABELS "startup;integration" TIMEOUT 60
            WORKING_DIRECTORY "$<TARGET_FILE_DIR:ArchuraEngine>")
    endif()
endif()
