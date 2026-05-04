#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

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

int main() {
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

    std::string startCmd = "start \"\" \"" + exePath + "\"";
    RunCommand(startCmd);
  } else {
    SetColor(12); // Red
    std::cerr << "CRITICAL ERROR: Engine executable missing after build!"
              << std::endl;
    system("pause");
  }

  return 0;
}