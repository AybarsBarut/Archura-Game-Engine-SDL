#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "../rendering/GraphicsAPI.h"

// Windows headers must come first with proper guards
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

namespace fs = std::filesystem;

// Helpers
int RunCommand(const std::string &command) {
  return std::system(command.c_str());
}

void SetColor(int color) {
  SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
}

Archura::GraphicsAPI PromptForGraphicsAPI(
    Archura::GraphicsAPI currentPreference) {
  std::cout << std::endl;
  SetColor(11);
  std::cout << "Graphics API" << std::endl;
  SetColor(7);
  std::cout << "  [1] Auto (recommended)" << std::endl;
  std::cout << "  [2] Vulkan";
  if (!Archura::IsGraphicsAPICompiled(Archura::GraphicsAPI::Vulkan)) {
    std::cout << " (not available in this build; OpenGL fallback)";
  }
  std::cout << std::endl;
  std::cout << "  [3] OpenGL (compatibility)" << std::endl;
  std::cout << "Current: " << Archura::ToString(currentPreference) << std::endl;
  std::cout << "Select [Enter keeps current]: ";

  std::string choice;
  std::getline(std::cin, choice);
  if (choice.empty())
    return currentPreference;
  if (choice == "1")
    return Archura::GraphicsAPI::Auto;
  if (choice == "2") {
    if (!Archura::IsGraphicsAPICompiled(Archura::GraphicsAPI::Vulkan)) {
      SetColor(14);
      std::cout << "Vulkan is not available in this build; the engine will "
                   "use OpenGL fallback."
                << std::endl;
      SetColor(7);
    }
    return Archura::GraphicsAPI::Vulkan;
  }
  if (choice == "3")
    return Archura::GraphicsAPI::OpenGL;

  SetColor(14);
  std::cout << "Unknown selection; keeping "
            << Archura::ToString(currentPreference) << "." << std::endl;
  SetColor(7);
  return currentPreference;
}

int main(int argc, char **argv) {
  SetConsoleTitle("Archura Engine Launcher");

  SetColor(11); // Cyan
  std::cout << "=== Archura Engine Launcher (C++) ===" << std::endl;
  SetColor(7);

  // Robust path discovery: Try to find project root by looking for
  // "CMakeLists.txt"
  fs::path rootPath = fs::current_path();
  bool foundRoot = false;
  for (int i = 0; i < 4; ++i) {
    if (fs::exists(rootPath / "CMakeLists.txt")) {
      foundRoot = true;
      break;
    }
    rootPath = rootPath.parent_path();
  }

  if (foundRoot) {
    fs::current_path(rootPath);
    std::cout << "Project Root: " << rootPath.string() << std::endl;
  } else {
    SetColor(12); // Red
    std::cerr
        << "Warning: Could not reliably determine project root. Using CWD."
        << std::endl;
    SetColor(7);
  }

  Archura::GraphicsLaunchOptions graphicsOptions;
  std::string graphicsError;
  const fs::path graphicsPreferencePath = "config/graphics_api.cfg";
  if (!Archura::ResolveGraphicsLaunchOptions(
          argc, argv, graphicsPreferencePath, graphicsOptions, graphicsError)) {
    SetColor(12);
    std::cerr << "Graphics selection error: " << graphicsError << std::endl;
    SetColor(7);
    system("pause");
    return -1;
  }

  if (!graphicsOptions.explicitlySelected) {
    graphicsOptions.requestedAPI =
        PromptForGraphicsAPI(graphicsOptions.requestedAPI);
  }
  if (!Archura::SaveGraphicsPreference(graphicsPreferencePath,
                                        graphicsOptions.requestedAPI)) {
    SetColor(14);
    std::cout << "Warning: graphics preference could not be saved." << std::endl;
    SetColor(7);
  }

  // Try to find the engine executable in common locations
  std::vector<std::string> searchPaths = {
      "build/bin/Release/ArchuraEngine.exe",
      "build/bin/Debug/ArchuraEngine.exe", "bin/Release/ArchuraEngine.exe",
      "bin/Debug/ArchuraEngine.exe", "ArchuraEngine.exe"};

  std::string exePath = "";
  for (const auto &p : searchPaths) {
    if (fs::exists(p)) {
      exePath = p;
      break;
    }
  }

  bool needsBuild = exePath.empty();

  // 1. Initial Check
  if (needsBuild) {
    SetColor(14); // Yellow
    std::cout << "Engine executable not found. Build required." << std::endl;
    SetColor(7);
  }

  // 2. Update Check (Git)
  if (fs::exists(".git")) {
    std::cout << "Checking for updates..." << std::endl;
    if (RunCommand("git fetch origin") == 0) {
      SetColor(14); // Yellow
      std::cout << "Synchronizing with repository..." << std::endl;
      SetColor(7);
      if (RunCommand("git pull") == 0) {
        // Pull success might mean we need a rebuild
        needsBuild = true;
      }
    } else {
      SetColor(12); // Red
      std::cout << "Update server unreachable. Offline mode." << std::endl;
      SetColor(7);
    }
  }

  // 3. Build if needed or if build directory is missing
  if (needsBuild || !fs::exists("build")) {
    SetColor(14); // Yellow
    std::cout << "Configuring/Building engine..." << std::endl;
    SetColor(7);

    if (!fs::exists("build"))
      fs::create_directory("build");

    // Prefer Release if no exe exists
    if (RunCommand("cmake -S . -B build") != 0) {
      SetColor(12); // Red
      std::cerr << "CMake Configuration Failed!" << std::endl;
      system("pause");
      return -1;
    }

    std::cout << "Building (Release)..." << std::endl;
    if (RunCommand(
            "cmake --build build --config Release --target ArchuraEngine") !=
        0) {
      SetColor(12); // Red
      std::cerr << "Build Failed!" << std::endl;
      system("pause");
      return -1;
    }

    exePath = "build/bin/Release/ArchuraEngine.exe";
  }

  // 4. Launch
  if (fs::exists(exePath)) {
    SetColor(10); // Green
    std::cout << "Launching Engine: " << exePath << std::endl;
    SetColor(7);

    std::string startCmd = "start \"\" \"" + exePath +
                           "\" --graphics=" +
                           Archura::ToString(graphicsOptions.requestedAPI);
    if (!graphicsOptions.allowFallback) {
      startCmd += " --no-graphics-fallback";
    }
    RunCommand(startCmd);
  } else {
    SetColor(12); // Red
    std::cerr << "CRITICAL ERROR: Engine executable missing after build!"
              << std::endl;
    system("pause");
  }

  return 0;
}
