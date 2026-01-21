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
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/${CMAKE_BUILD_TYPE})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

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

# Engine Sources
file(GLOB_RECURSE ENGINE_SOURCES 
    "src/core/*.cpp"
    "src/rendering/*.cpp"
    "src/ecs/*.cpp"

    "src/input/*.cpp"
    "src/game/*.cpp"
    "src/editor/*.cpp"
    "src/network/*.cpp"
)

file(GLOB_RECURSE ENGINE_HEADERS 
    "src/core/*.h"
    "src/rendering/*.h"
    "src/ecs/*.h"

    "src/input/*.h"
    "src/game/*.h"
    "src/editor/*.h"
    "src/network/*.h"
)

# Main Executable
add_executable(${PROJECT_NAME} 
    src/main.cpp
    ${ENGINE_SOURCES}
    ${ENGINE_HEADERS}
)

# Include Directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/external/stb
    ${CMAKE_CURRENT_SOURCE_DIR}/external/glm
    ${SDL2_INCLUDE_DIR}
)

# Linking
target_link_libraries(${PROJECT_NAME} PRIVATE
    OpenGL::GL
    SDL2
    SDL2main
    glad
    imgui
    ufbx
    ws2_32
    winmm
)

# Copy SDL2.dll to output directory
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${SDL2_BasePath}/lib/x64/SDL2.dll"
    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/SDL2.dll"
)

# Asset Copying
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets
)
