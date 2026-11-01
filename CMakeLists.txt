cmake_minimum_required(VERSION 3.21)

if(POLICY CMP0077)
  cmake_policy(SET CMP0077 NEW)
endif()

project(ArchuraEngine VERSION 1.0.0 LANGUAGES CXX C)

# C++ Standard
# Define APIENTRY globally to fix Windows SDK syntax errors
if(WIN32)
    add_compile_definitions(APIENTRY=__stdcall _APIENTRY_DEFINED)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ARCHURA_BUILD_MEMORY_BENCHMARKS "Build allocator benchmark executable" OFF)
option(ARCHURA_BUILD_ECS_BENCHMARKS "Build ECS storage benchmark executable" OFF)
option(ARCHURA_BUILD_RENDERER_TESTS "Build renderer ownership contract tests" OFF)
option(ARCHURA_BUILD_ECS_TESTS "Build focused ECS core contract tests" ON)
option(ARCHURA_BUILD_PHYSICS_TESTS "Build focused physics contract tests" ON)
option(ARCHURA_DEBUG_PHYSICS "Build the F3 physics debug wireframe overlay" ON)
option(ARCHURA_OPENAL "Build OpenAL Soft spatial audio support" OFF)
option(ARCHURA_REQUIRE_SDL_NET "Fail configuration when SDL_net is unavailable" OFF)
option(ARCHURA_BUILD_STARTUP_TESTS "Enable startup smoke tests (requires a desktop/OpenGL context)" OFF)
option(ARCHURA_LOOP_TESTS_ONLY "Build SDK-free game loop contracts only" OFF)
option(ARCHURA_BUILD_LOOP_BENCHMARK "Build the synthetic game loop benchmark" OFF)
option(ARCHURA_BUILD_RUNTIME "Build ArchuraEngine and stage its Mono runtime" ON)

# Keep this path before SDL/OpenGL/Mono discovery. It tests the production loop,
# not a duplicate implementation, on machines without the runtime SDKs.
include(CTest)
if(BUILD_TESTING)
    add_executable(ArchuraGameLoopTests tests/GameLoopHarnessTests.cpp src/core/GameLoopHarness.cpp)
    target_include_directories(ArchuraGameLoopTests PRIVATE src)
    add_test(NAME Archura.Core.GameLoop COMMAND ArchuraGameLoopTests)
    set_tests_properties(Archura.Core.GameLoop PROPERTIES LABELS "contract;loop" TIMEOUT 30)
endif()
if(ARCHURA_BUILD_LOOP_BENCHMARK)
    add_executable(ArchuraGameLoopBenchmark benchmarks/GameLoopBenchmark.cpp src/core/GameLoopHarness.cpp)
    target_include_directories(ArchuraGameLoopBenchmark PRIVATE src)
endif()
if(ARCHURA_LOOP_TESTS_ONLY)
    return()
endif()
if(ARCHURA_BUILD_STARTUP_TESTS AND NOT ARCHURA_BUILD_RUNTIME)
    message(FATAL_ERROR "ARCHURA_BUILD_STARTUP_TESTS requires ARCHURA_BUILD_RUNTIME=ON")
endif()

# Force MSVC Runtime to be Dynamic (DLL) to avoid LIBCMT conflicts
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

# Compiler-specific optimizations
if(MSVC)
    add_compile_options(
        $<$<CONFIG:Release>:/O2>
        $<$<CONFIG:Release>:/Ob2>
        $<$<CONFIG:Release>:/Oi>
        $<$<CONFIG:Release>:/Ot>
        $<$<CONFIG:Release>:/GL>
        $<$<CONFIG:Debug>:/Od>
        $<$<CONFIG:Debug>:/Zi>
    )
    add_link_options($<$<CONFIG:Release>:/LTCG>)
    # Ignore LIBCMT to prevent LNK4098 warning when using /MD
    add_link_options("/NODEFAULTLIB:LIBCMT")
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    add_compile_options(/W4 /MP /wd4201)
else()
    # Keep Debug numerics and portability intact. Fast-math can turn malformed
    # input into undefined behaviour; native CPU tuning also makes binaries
    # unusable on older/other machines. Apply only safe optimizations to Release.
    add_compile_options(
        $<$<CONFIG:Release>:-O3>
        $<$<CONFIG:Release>:-flto>
    )
endif()

# Output Directories
# Config-specific variables override the default so MSVC multi-config generator
# does NOT append an extra /Release or /Debug subfolder automatically.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY              ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG        ${CMAKE_BINARY_DIR}/bin/Debug)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE      ${CMAKE_BINARY_DIR}/bin/Release)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_BINARY_DIR}/bin/RelWithDebInfo)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY              ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG        ${CMAKE_BINARY_DIR}/lib/Debug)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE      ${CMAKE_BINARY_DIR}/lib/Release)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY              ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG        ${CMAKE_BINARY_DIR}/lib/Debug)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE      ${CMAKE_BINARY_DIR}/lib/Release)

# Dependencies
find_package(OpenGL REQUIRED)

# SDL2 Setup
set(SDL2_BasePath "${CMAKE_SOURCE_DIR}/external/SDL2")
set(SDL2_INCLUDE_DIR "${SDL2_BasePath}/include")
set(SDL2_LIBRARY "${SDL2_BasePath}/lib/x64/SDL2.lib")
set(SDL2MAIN_LIBRARY "${SDL2_BasePath}/lib/x64/SDL2main.lib")

add_library(SDL2 STATIC IMPORTED)
set_target_properties(SDL2 PROPERTIES
    IMPORTED_LOCATION "${SDL2_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${SDL2_INCLUDE_DIR}"
)

set(SDL2_TTF_BasePath "${CMAKE_SOURCE_DIR}/external/SDL2_ttf/SDL2_ttf-2.22.0")
set(SDL2_TTF_INCLUDE_DIR "${SDL2_TTF_BasePath}/include")
set(SDL2_TTF_LIBRARY "${SDL2_TTF_BasePath}/lib/x64/SDL2_ttf.lib")

add_library(SDL2_ttf STATIC IMPORTED)
set_target_properties(SDL2_ttf PROPERTIES
    IMPORTED_LOCATION "${SDL2_TTF_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${SDL2_TTF_INCLUDE_DIR}"
)

add_library(SDL2main STATIC IMPORTED)
set_target_properties(SDL2main PROPERTIES
    IMPORTED_LOCATION "${SDL2MAIN_LIBRARY}"
)

message(STATUS "Using SDL2 from: ${SDL2_BasePath}")

# SDL_net is optional at configure time so the engine and protocol tests remain
# buildable from source archives that do not bundle the SDK. Runtime networking
# explicitly reports Unavailable unless this backend is present.
set(ARCHURA_SDL_NET_TARGET "")
set(SDL2_NET_BASE_PATH "${CMAKE_SOURCE_DIR}/external/SDL2_net")
find_path(SDL2_NET_INCLUDE_DIR
    NAMES SDL_net.h SDL2/SDL_net.h
    HINTS "${SDL2_NET_BASE_PATH}/include"
)
find_library(SDL2_NET_LIBRARY
    NAMES SDL2_net SDL2_net-static
    HINTS "${SDL2_NET_BASE_PATH}/lib/x64" "${SDL2_NET_BASE_PATH}/lib"
)
if(SDL2_NET_INCLUDE_DIR AND SDL2_NET_LIBRARY)
    add_library(ArchuraSDL2Net UNKNOWN IMPORTED)
    set_target_properties(ArchuraSDL2Net PROPERTIES
        IMPORTED_LOCATION "${SDL2_NET_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${SDL2_NET_INCLUDE_DIR}"
    )
    set(ARCHURA_SDL_NET_TARGET ArchuraSDL2Net)
    message(STATUS "SDL_net backend enabled: ${SDL2_NET_LIBRARY}")
elseif(ARCHURA_REQUIRE_SDL_NET)
    message(FATAL_ERROR
        "ARCHURA_REQUIRE_SDL_NET=ON but SDL_net was not found. "
        "Install SDL2_net or place it under external/SDL2_net.")
else()
    message(WARNING
        "SDL_net was not found. Networking runtime will be unavailable; "
        "framing/serialization remains enabled and testable.")
endif()

# GLAD - Use wrapper to ensure proper Windows header order
add_library(glad src/glad_wrapper.c)
target_include_directories(glad PUBLIC external/glad/include)

# ImGui - Use wrappers to ensure proper Windows header order
set(IMGUI_DIR external/imgui)
add_library(imgui STATIC
    src/imgui_wrapper.cpp
    src/imgui_demo_wrapper.cpp
    src/imgui_draw_wrapper.cpp
    src/imgui_tables_wrapper.cpp
    src/imgui_widgets_wrapper.cpp
    src/imgui_impl_sdl2_wrapper.cpp
    src/imgui_impl_opengl3_wrapper.cpp
)
target_include_directories(imgui PUBLIC
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    external/glad/include
    ${SDL2_INCLUDE_DIR}
)
target_link_libraries(imgui PUBLIC SDL2 SDL2main OpenGL::GL)

# Force define APIENTRY to avoid Windows SDK syntax errors
if(WIN32)
    target_compile_definitions(imgui PRIVATE APIENTRY=__stdcall)
    target_compile_definitions(glad PRIVATE APIENTRY=__stdcall)
endif()

# ufbx (FBX Loader)
add_library(ufbx external/ufbx/ufbx.c)
target_include_directories(ufbx PUBLIC external/ufbx)

# fast_obj (OBJ Loader)
add_library(fast_obj external/fast_obj/fast_obj.c)
target_include_directories(fast_obj PUBLIC external/fast_obj)

# Engine Sources (Shared between client and server)
# Currently minimal - server will have its own game logic
file(GLOB_RECURSE SHARED_SOURCES CONFIGURE_DEPENDS
    "src/ecs/*.cpp"
    "src/game/PhysicsSystem.cpp"
    "src/network/*.cpp"
)

file(GLOB_RECURSE SHARED_HEADERS CONFIGURE_DEPENDS
    "src/ecs/*.h"
    "src/game/PhysicsSystem.h"
    "src/network/*.h"
)

# Client-only sources (Graphics, Input, Editor, etc.)
file(GLOB_RECURSE CLIENT_SOURCES CONFIGURE_DEPENDS
    "src/core/Engine.cpp"
    "src/core/Application.cpp"
    "src/core/GameLoopHarness.cpp"
    "src/core/Window.cpp"
    "src/core/ImGuiLayer.cpp"
    "src/core/AudioSystem.cpp"
    "src/core/DeveloperConsole.cpp"
    "src/core/ConsoleCommand.cpp"
    "src/core/ResourceManager.cpp"
    "src/core/Logger.cpp"
    "src/core/threading/JobSystem.cpp"
    "src/core/FrameTelemetry.cpp"
    "src/core/memory/MemoryTracker.cpp"
    "src/core/ProjectSerializer.cpp"
    "src/core/ProjectSession.cpp"
    "src/core/GameSaveManager.cpp"
    "src/rendering/*.cpp"
    "src/input/*.cpp"
    "src/scripting/*.cpp"
    "src/game/FPSController.cpp"
    "src/game/RenderSystem.cpp"
    "src/game/ParticleSystem.cpp"
    "src/game/GPUParticleSystem.cpp"
    "src/game/Projectile.cpp"
    "src/game/ProjectileSystem.cpp"
    "src/game/Weapon.cpp"
    "src/game/DevConsole.cpp"
    "src/game/CommandRegistry.cpp"
    "src/game/FPSConsoleCommands.cpp"
    "src/game/PauseMenu.cpp"
    "src/game/ScriptSystem.cpp"
    "src/game/GameModeManager.cpp"
    "src/game/GraphicalNetworkSession.cpp"
    "src/editor/*.cpp"
)

file(GLOB_RECURSE CLIENT_HEADERS CONFIGURE_DEPENDS
    "src/core/Engine.h"
    "src/core/Application.h"
    "src/core/FrameTelemetry.h"
    "src/core/Window.h"
    "src/core/ImGuiLayer.h"
    "src/core/AudioSystem.h"
    "src/core/memory/*.h"
    "src/core/threading/*.h"
    "src/rendering/*.h"
    "src/input/*.h"
    "src/scripting/*.h"
    "src/ui/*.h"
    "src/game/FPSController.h"
    "src/game/RenderSystem.h"
    "src/game/ParticleSystem.h"
    "src/game/GPUParticleSystem.h"
    "src/game/CollisionEvent.h"
    "src/game/Projectile.h"
    "src/game/ProjectileSystem.h"
    "src/game/Weapon.h"
    "src/game/DevConsole.h"
    "src/game/CommandRegistry.h"
    "src/game/FPSConsoleCommands.h"
    "src/game/PauseMenu.h"
    "src/game/ScriptSystem.h"
    "src/core/GpuBuffer.h"
    "src/editor/*.h"
)

if(ARCHURA_DEBUG_PHYSICS)
    list(APPEND CLIENT_SOURCES
        "src/systems/DebugPhysicsSystem.cpp"
    )
    list(APPEND CLIENT_HEADERS
        "src/systems/DebugPhysicsSystem.h"
    )
endif()

if(ARCHURA_OPENAL)
    find_package(OpenAL REQUIRED)
    list(APPEND CLIENT_SOURCES
        "src/audio/OpenALAudioSystem.cpp"
    )
    list(APPEND CLIENT_HEADERS
        "src/audio/OpenALAudioSystem.h"
        "src/ecs/components/AudioSourceComponent.h"
    )
endif()

# ============================================
# CLIENT EXECUTABLE (ArchuraEngine)
# ============================================
if(ARCHURA_BUILD_RUNTIME)
add_executable(${PROJECT_NAME}
    src/main.cpp
    ${SHARED_SOURCES}
    ${SHARED_HEADERS}
    ${CLIENT_SOURCES}
    ${CLIENT_HEADERS}
)

# Integration fixtures are test-owned and opt-in; normal client builds do not
# compile or include their private-access test bodies.
if(ARCHURA_BUILD_STARTUP_TESTS)
    target_sources(${PROJECT_NAME} PRIVATE tests/support/editor/EditorSmokeSuite.cpp)
    target_compile_definitions(${PROJECT_NAME} PRIVATE ARCHURA_ENABLE_EDITOR_STARTUP_CHECKS=1)
endif()

# Mono Setup (Assumes external/mono layout)
set(MONO_DIR "${CMAKE_SOURCE_DIR}/external/mono")
set(MONO_INCLUDE_DIR "${MONO_DIR}/include/mono-2.0")
set(MONO_LIB_DIR "${MONO_DIR}/lib")

# Fail at configuration with the missing file, rather than at compile/link or
# after a lengthy successful build during deployment. The current bundled SDK
# layout targets Windows x64; portability needs separate dependency discovery.
if(WIN32)
    foreach(required_file IN ITEMS
        "${SDL2_INCLUDE_DIR}/SDL.h" "${SDL2_LIBRARY}" "${SDL2MAIN_LIBRARY}"
        "${SDL2_BasePath}/lib/x64/SDL2.dll"
        "${SDL2_TTF_INCLUDE_DIR}/SDL_ttf.h" "${SDL2_TTF_LIBRARY}"
        "${SDL2_TTF_BasePath}/lib/x64/SDL2_ttf.dll"
        "${MONO_INCLUDE_DIR}/mono/jit/jit.h"
        "${MONO_LIB_DIR}/mono-2.0-sgen.lib" "${MONO_DIR}/bin/mono-2.0-sgen.dll"
        "${MONO_DIR}/lib/mono/4.5/mscorlib.dll")
        if(NOT EXISTS "${required_file}")
            message(FATAL_ERROR "Required bundled Windows x64 dependency missing: ${required_file}")
        endif()
    endforeach()
endif()

# Include Directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/external/stb
    ${CMAKE_CURRENT_SOURCE_DIR}/external/dr_wav
    ${CMAKE_CURRENT_SOURCE_DIR}/external/glm
    ${SDL2_INCLUDE_DIR}
    ${MONO_INCLUDE_DIR}
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    "$<$<CONFIG:Debug>:ARCHURA_DEBUG>"
    "$<$<BOOL:${ARCHURA_DEBUG_PHYSICS}>:ARCHURA_DEBUG_PHYSICS>"
    "$<$<BOOL:${ARCHURA_OPENAL}>:ARCHURA_OPENAL>"
)

# Linking
target_link_libraries(${PROJECT_NAME} PRIVATE
    SDL2
    SDL2_ttf
    glad
    imgui
    ufbx
    fast_obj
    ws2_32
    winmm
    "${MONO_LIB_DIR}/mono-2.0-sgen.lib"
)

if(ARCHURA_SDL_NET_TARGET)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${ARCHURA_SDL_NET_TARGET})
    target_compile_definitions(${PROJECT_NAME} PRIVATE ARCHURA_HAS_SDL_NET=1)
endif()

if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE shell32)
endif()

if(ARCHURA_OPENAL)
    if(TARGET OpenAL::OpenAL)
        target_link_libraries(${PROJECT_NAME} PRIVATE OpenAL::OpenAL)
    else()
        target_include_directories(${PROJECT_NAME} PRIVATE ${OPENAL_INCLUDE_DIR})
        target_link_libraries(${PROJECT_NAME} PRIVATE ${OPENAL_LIBRARY})
    endif()
endif()

set(ARCHURA_COPY_DIRECTORY_COMMAND copy_directory)
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.26")
    set(ARCHURA_COPY_DIRECTORY_COMMAND copy_directory_if_different)
endif()

# Copy SDL2.dll to output directory
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${SDL2_BasePath}/lib/x64/SDL2.dll"
    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/SDL2.dll"
)

# Copy SDL2_ttf.dll to output directory
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${SDL2_TTF_BasePath}/lib/x64/SDL2_ttf.dll"
    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/SDL2_ttf.dll"
)

# Copy Mono DLL to output directory
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${MONO_DIR}/bin/mono-2.0-sgen.dll"
    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/mono-2.0-sgen.dll"
)

# Copy Mono Class Libraries (mscorlib etc.)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "Copying Mono class libraries..."
    COMMAND ${CMAKE_COMMAND} -E ${ARCHURA_COPY_DIRECTORY_COMMAND}
    "${MONO_DIR}/lib/mono"
    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/mono/lib/mono"
)

# Asset Copying
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "Copying assets..."
    COMMAND ${CMAKE_COMMAND} -E ${ARCHURA_COPY_DIRECTORY_COMMAND}
    ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets
)

# ScriptCore + managed resources
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "Copying managed scripting resources..."
    COMMAND ${CMAKE_COMMAND} -E ${ARCHURA_COPY_DIRECTORY_COMMAND}
    ${CMAKE_SOURCE_DIR}/Resources $<TARGET_FILE_DIR:${PROJECT_NAME}>/Resources
)
endif()

# ============================================
# DEDICATED SERVER EXECUTABLE (ArchuraServer)
# ============================================
add_executable(ArchuraServer
    src/core/GameLoopHarness.cpp
    src/server/server_main.cpp
    ${SHARED_SOURCES}
    ${SHARED_HEADERS}
)

# Server doesn't need graphics libraries
target_include_directories(ArchuraServer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/external/glm
    ${SDL2_INCLUDE_DIR}
)

# Server only needs network libraries
target_link_libraries(ArchuraServer PRIVATE
    ws2_32
)

if(ARCHURA_SDL_NET_TARGET)
    target_link_libraries(ArchuraServer PRIVATE SDL2 ${ARCHURA_SDL_NET_TARGET})
    target_compile_definitions(ArchuraServer PRIVATE ARCHURA_HAS_SDL_NET=1)
    add_executable(ArchuraNetClient src/client/network_client_main.cpp ${SHARED_SOURCES})
    target_include_directories(ArchuraNetClient PRIVATE src external/glm)
    target_link_libraries(ArchuraNetClient PRIVATE SDL2 ${ARCHURA_SDL_NET_TARGET})
    if(WIN32)
        target_link_libraries(ArchuraNetClient PRIVATE ws2_32)
    endif()
    target_compile_definitions(ArchuraNetClient PRIVATE ARCHURA_HAS_SDL_NET=1)
    # Executables must carry the transport backend; do not rely on developer PATH.
    set(network_targets ArchuraServer ArchuraNetClient)
    if(ARCHURA_BUILD_RUNTIME)
        list(APPEND network_targets ArchuraEngine)
    endif()
    foreach(network_target IN LISTS network_targets)
        add_custom_command(TARGET ${network_target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${network_target}>/licenses"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${SDL2_NET_BASE_PATH}/LICENSE.txt"
                "$<TARGET_FILE_DIR:${network_target}>/licenses/SDL2_net.txt"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${SDL2_NET_BASE_PATH}/lib/x64/SDL2_net.dll" "$<TARGET_FILE_DIR:${network_target}>/SDL2_net.dll"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${SDL2_BasePath}/lib/x64/SDL2.dll" "$<TARGET_FILE_DIR:${network_target}>/SDL2.dll")
    endforeach()
endif()

include(cmake/NativeValidation.cmake)

# ============================================
# LAUNCHER EXECUTABLE (ArchuraLauncher)
# ============================================
add_executable(ArchuraLauncher
    src/launcher/Launcher.cpp
    src/rendering/GraphicsAPI.cpp
)

target_include_directories(ArchuraLauncher PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Launcher only needs standard OS libraries
if(WIN32)
    target_link_libraries(ArchuraLauncher PRIVATE User32 Shell32)
endif()

# Copy server config template
add_custom_command(TARGET ArchuraServer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_SOURCE_DIR}/server_config.json
    $<TARGET_FILE_DIR:ArchuraServer>/server_config.json
)

