message(STATUS "Non-Windows runtime: headless server/client; engine/editor/launcher are Windows-only")
find_package(Threads REQUIRED)
find_package(SDL2 QUIET)
if(TARGET SDL2::SDL2)
    set(archura_sdl2 SDL2::SDL2)
elseif(SDL2_FOUND AND SDL2_LIBRARIES)
    add_library(ArchuraSystemSDL2 INTERFACE)
    target_include_directories(ArchuraSystemSDL2 INTERFACE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(ArchuraSystemSDL2 INTERFACE ${SDL2_LIBRARIES})
    set(archura_sdl2 ArchuraSystemSDL2)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(ARCHURA_SYSTEM_SDL2 REQUIRED IMPORTED_TARGET sdl2)
    set(archura_sdl2 PkgConfig::ARCHURA_SYSTEM_SDL2)
endif()

set(ARCHURA_SDL_NET_TARGET "")
find_package(SDL2_net QUIET CONFIG)
if(TARGET SDL2_net::SDL2_net)
    set(ARCHURA_SDL_NET_TARGET SDL2_net::SDL2_net)
elseif(TARGET SDL2_net::SDL2_net-static)
    set(ARCHURA_SDL_NET_TARGET SDL2_net::SDL2_net-static)
else()
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(ARCHURA_SYSTEM_SDL2_NET QUIET IMPORTED_TARGET SDL2_net)
        if(TARGET PkgConfig::ARCHURA_SYSTEM_SDL2_NET)
            set(ARCHURA_SDL_NET_TARGET PkgConfig::ARCHURA_SYSTEM_SDL2_NET)
        endif()
    endif()
endif()
if(ARCHURA_SDL_NET_TARGET)
    message(STATUS "SDL_net backend enabled: ${ARCHURA_SDL_NET_TARGET}")
elseif(ARCHURA_REQUIRE_SDL_NET)
    message(FATAL_ERROR
        "ARCHURA_REQUIRE_SDL_NET=ON but SDL_net was not found. "
        "Install the system SDL2_net development package.")
else()
    message(WARNING
        "SDL_net was not found. Networking runtime will be unavailable; "
        "framing/serialization remains enabled and testable.")
endif()

file(GLOB_RECURSE archura_headless_sources CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/ecs/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/game/PhysicsSystem.cpp"
    "${CMAKE_SOURCE_DIR}/src/network/*.cpp")
add_executable(ArchuraServer src/server/server_main.cpp ${archura_headless_sources})
add_executable(ArchuraNetClient src/client/network_client_main.cpp ${archura_headless_sources})
foreach(target IN ITEMS ArchuraServer ArchuraNetClient)
    target_include_directories(${target} PRIVATE src external/glm)
    target_link_libraries(${target} PRIVATE Threads::Threads ${archura_sdl2})
    if(ARCHURA_SDL_NET_TARGET)
        target_link_libraries(${target} PRIVATE ${ARCHURA_SDL_NET_TARGET})
        target_compile_definitions(${target} PRIVATE ARCHURA_HAS_SDL_NET=1)
    endif()
endforeach()
add_custom_command(TARGET ArchuraServer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/server_config.json"
        "$<TARGET_FILE_DIR:ArchuraServer>/server_config.json")

include("${CMAKE_CURRENT_LIST_DIR}/PortableTests.cmake")
if(BUILD_TESTING AND Python3_Interpreter_FOUND AND ARCHURA_SDL_NET_TARGET)
    add_test(NAME Archura.Network.ProcessIntegration
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/network/NetworkProcessTests.py"
            --server "$<TARGET_FILE:ArchuraServer>" --client "$<TARGET_FILE:ArchuraNetClient>")xxxxxx
    set_tests_properties(Archura.Network.ProcessIntegration PROPERTIES
        LABELS "network;integration" TIMEOUT 40 RUN_SERIAL TRUE)
endif()
