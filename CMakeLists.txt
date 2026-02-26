    cmake_minimum_required(VERSION 3.15)

if(POLICY CMP0077)
  cmake_policy(SET CMP0077 NEW)
endif()

project(ArchuraEngine VERSION 1.0.0 LANGUAGES CXX C)

# C++ Standard
# Define APIENTRY globally to fix Windows SDK syntax errors
add_compile_definitions(APIENTRY=__stdcall)
add_compile_definitions(_APIENTRY_DEFINED)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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
    add_compile_options(/W4 /MP)
else()
    add_compile_options(-O3 -march=native -ffast-math -flto)
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
target_compile_definitions(imgui PRIVATE APIENTRY=__stdcall)
target_compile_definitions(glad PRIVATE APIENTRY=__stdcall)

# ufbx (FBX Loader)
add_library(ufbx external/ufbx/ufbx.c)
target_include_directories(ufbx PUBLIC external/ufbx)

# fast_obj (OBJ Loader)
add_library(fast_obj external/fast_obj/fast_obj.c)
target_include_directories(fast_obj PUBLIC external/fast_obj)

# Engine Sources (Shared between client and server)
# Currently minimal - server will have its own game logic
file(GLOB_RECURSE SHARED_SOURCES 
    "src/ecs/*.cpp"
    "src/game/PhysicsSystem.cpp"
    "src/network/*.cpp"
)

file(GLOB_RECURSE SHARED_HEADERS 
    "src/ecs/*.h"
    "src/game/PhysicsSystem.h"
    "src/network/*.h"
)

# Client-only sources (Graphics, Input, Editor, etc.)
file(GLOB_RECURSE CLIENT_SOURCES
    "src/core/Engine.cpp"
    "src/core/Application.cpp"
    "src/core/Window.cpp"
    "src/core/ImGuiLayer.cpp"
    "src/core/AudioSystem.cpp"
    "src/core/DeveloperConsole.cpp"
    "src/core/ResourceManager.cpp"
    "src/core/Logger.cpp"
    "src/core/ProjectSerializer.cpp"
    "src/rendering/*.cpp"
    "src/input/*.cpp"
    "src/scripting/*.cpp"
    "src/game/FPSController.cpp"
    "src/game/RenderSystem.cpp"
    "src/game/ParticleSystem.cpp"
    "src/game/Projectile.cpp"
    "src/game/ProjectileSystem.cpp"
    "src/game/Weapon.cpp"
    "src/game/DevConsole.cpp"
    "src/game/CommandRegistry.cpp"
    "src/game/FPSConsoleCommands.cpp"
    "src/game/PauseMenu.cpp"
    "src/game/ScriptSystem.cpp"
    "src/editor/*.cpp"
)

file(GLOB_RECURSE CLIENT_HEADERS
    "src/core/Engine.h"
    "src/core/Application.h"
    "src/core/Window.h"
    "src/core/ImGuiLayer.h"
    "src/core/AudioSystem.h"
    "src/rendering/*.h"
    "src/input/*.h"
    "src/scripting/*.h"
    "src/game/FPSController.h"
    "src/game/RenderSystem.h"
    "src/game/ParticleSystem.h"
    "src/game/Projectile.h"
    "src/game/ProjectileSystem.h"
    "src/game/Weapon.h"
    "src/game/DevConsole.h"
    "src/game/CommandRegistry.h"
    "src/game/FPSConsoleCommands.h"
    "src/game/PauseMenu.h"
    "src/game/ScriptSystem.h"
    "src/editor/*.h"
)

# ============================================
# CLIENT EXECUTABLE (ArchuraEngine)
# ============================================
add_executable(${PROJECT_NAME} 
    src/main.cpp
    ${SHARED_SOURCES}
    ${SHARED_HEADERS}
    ${CLIENT_SOURCES}
    ${CLIENT_HEADERS}
)

# Mono Setup (Assumes external/mono layout)
set(MONO_DIR "${CMAKE_SOURCE_DIR}/external/mono")
set(MONO_INCLUDE_DIR "${MONO_DIR}/include/mono-2.0")
set(MONO_LIB_DIR "${MONO_DIR}/lib")

# Include Directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/external/stb
    ${CMAKE_CURRENT_SOURCE_DIR}/external/glm
    ${SDL2_INCLUDE_DIR}
    ${MONO_INCLUDE_DIR}
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
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${MONO_DIR}/lib/mono"
    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/mono/lib/mono"
)

# Asset Copying
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets
)

# ============================================
# DEDICATED SERVER EXECUTABLE (ArchuraServer)
# ============================================
add_executable(ArchuraServer
    src/server/server_main.cpp
    ${SHARED_SOURCES}
    ${SHARED_HEADERS}
)

# Server doesn't need graphics libraries
target_include_directories(ArchuraServer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/external/glm
)

# Server only needs network libraries
target_link_libraries(ArchuraServer PRIVATE
    ws2_32
)

# Server doesn't need SDL, ImGui, GLAD, etc.
# Headless mode - no graphics dependencies

# Copy server config template
add_custom_command(TARGET ArchuraServer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_SOURCE_DIR}/server_config.json
    $<TARGET_FILE_DIR:ArchuraServer>/server_config.json
)

