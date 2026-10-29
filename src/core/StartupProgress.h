#pragma once

#include <exception>
#include <functional>

namespace Archura {

// Completed startup boundaries, reported synchronously on the main thread.
// A callback may pump splash events and returns false to request orderly cancel.
enum class StartupStage { Starting, CoreReady, WorldReady, Ready };
using StartupProgressCallback = std::function<bool(StartupStage)>;

class StartupCancelled final : public std::exception {
public:
    const char* what() const noexcept override { return "Startup cancelled"; }
};

} // namespace Archura
