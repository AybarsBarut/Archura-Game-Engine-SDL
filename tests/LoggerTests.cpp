#include "core/Logger.h"
#include <sstream>

int main() {
    std::ostringstream output;
    auto* previous = std::cerr.rdbuf(output.rdbuf());
    Archura::Logger::SetEnabled(true);
    Archura::Logger::Warning("missing asset");
    Archura::Logger::Error("initialization failed");
    ARCH_LOG_ERROR("shader compile failed");
    ARCH_LOG_WARN("asset import failed");
    const auto enabled = output.str();
    Archura::Logger::SetEnabled(false);
    Archura::Logger::Error("disabled");
    const bool disabled = output.str() == enabled;
    Archura::Logger::SetEnabled(true);
    std::cerr.rdbuf(previous);
    return enabled.find("[WARNING] missing asset") != std::string::npos &&
           enabled.find("[ERROR] initialization failed") != std::string::npos &&
           enabled.find("[ERROR] shader compile failed") != std::string::npos &&
           enabled.find("[WARNING] asset import failed") != std::string::npos &&
           disabled ? 0 : 1;
}
