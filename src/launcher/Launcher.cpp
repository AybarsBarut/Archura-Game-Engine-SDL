#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "../rendering/GraphicsAPI.h"
#include "RuntimeDiscovery.h"

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

  // Resolve the checkout from either the invoking shell or launcher location.
  std::vector<wchar_t> modulePath(32768);
  const DWORD moduleLength = GetModuleFileNameW(nullptr, modulePath.data(),
                                               static_cast<DWORD>(modulePath.size()));
  if (moduleLength == 0 || moduleLength >= modulePath.size()) {
    std::cerr << "Could not resolve launcher location." << std::endl;
    return 1;
  }
  const fs::path launcherDirectory = fs::path(modulePath.data()).parent_path();
  fs::path rootPath = Archura::Launcher::FindRepositoryRoot(launcherDirectory);
  if (rootPath.empty())
    rootPath = Archura::Launcher::FindRepositoryRoot(fs::current_path());
  const bool foundRoot = !rootPath.empty();
  fs::current_path(foundRoot ? rootPath : launcherDirectory);
  if (foundRoot)
    std::cout << "Project Root: " << rootPath.string() << std::endl;

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

  fs::path exePath = Archura::Launcher::FindRuntime(rootPath, launcherDirectory);
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

  // 3. Build through the documented preset when needed.
  if (needsBuild) {
    if (!foundRoot) {
      std::cerr << "Engine runtime missing beside launcher; no source checkout found." << std::endl;
      return 1;
    }
    SetColor(14); // Yellow
    std::cout << "Configuring/Building engine..." << std::endl;
    SetColor(7);

    // Prefer Release if no exe exists
    if (RunCommand("cmake --preset windows-x64") != 0) {
      SetColor(12); // Red
      std::cerr << "CMake Configuration Failed!" << std::endl;
      system("pause");
      return -1;
    }

    std::cout << "Building (Release)..." << std::endl;
    if (RunCommand(
            "cmake --build --preset windows-release --target ArchuraEngine") !=
        0) {
      SetColor(12); // Red
      std::cerr << "Build Failed!" << std::endl;
      system("pause");
      return -1;
    }

    exePath = rootPath / "build/windows-x64/bin/Release/ArchuraEngine.exe";
  }

  // 4. Launch
  if (fs::exists(exePath)) {
    SetColor(10); // Green
    std::cout << "Launching Engine: " << exePath << std::endl;
    SetColor(7);

    std::wstring arguments = L"\"" + exePath.wstring() + L"\" --graphics=";
    const std::string graphicsName = Archura::ToString(graphicsOptions.requestedAPI);
    arguments.append(graphicsName.begin(), graphicsName.end());
    if (!graphicsOptions.allowFallback)
      arguments += L" --no-graphics-fallback";
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    // Relative assets, Mono and editor settings belong to the selected runtime.
    if (!CreateProcessW(exePath.c_str(), arguments.data(), nullptr, nullptr,
                        FALSE, 0, nullptr, exePath.parent_path().c_str(),
                        &startup, &process)) {
      std::cerr << "Engine launch failed (Windows error " << GetLastError() << ")." << std::endl;
      return 1;
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
  } else {
    SetColor(12); // Red
    std::cerr << "CRITICAL ERROR: Engine executable missing after build!"
              << std::endl;
    system("pause");
    return 1;
  }

  return 0;
}
