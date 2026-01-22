#define SDL_MAIN_HANDLED
#include "core/Application.h"
#include <iostream>
#include <cstdio>
#include <windows.h>
#include <memory> 

// Harici GPU Seçimi (NVIDIA / AMD)
extern "C" {
    __declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

int main(int argc, char** argv) {
    // Basic console if needed, handled by Subsystem:Console or parent process
    
    // Log relative path helper
    const char* logPath = "logs/DEBUG_MAIN.txt";
    FILE* f = fopen(logPath, "w");
    if (f) { fprintf(f, "Main Alive\n"); fclose(f); }

    try {
        auto app = std::make_unique<Archura::Application>();
        app->Run();
    } catch (const std::exception& e) {
        if(f = fopen(logPath, "a")) { fprintf(f, "EXCEPTION: %s\n", e.what()); fclose(f); }
        std::cerr << "EXCEPTION: " << e.what() << std::endl;
        return -1;
    } catch (...) {
        if(f = fopen(logPath, "a")) { fprintf(f, "UNKNOWN EXCEPTION\n"); fclose(f); }
        std::cerr << "UNKNOWN EXCEPTION" << std::endl;
        return -1;
    }
    
    return 0;
}
