#pragma once
#include "NativeSocket.h"
#include <string>
namespace Archura::Net {
// Poll-only owner; Cancel relinquishes its reference and never waits for DNS.
class AsyncResolver final {
public:
    AsyncResolver() = default;
    ~AsyncResolver();
    AsyncResolver(const AsyncResolver&) = delete;
    AsyncResolver& operator=(const AsyncResolver&) = delete;
    bool Start(const std::string& host, std::uint16_t port, std::string& error);
    IoStatus Poll(NativeAddress& address, std::string& error);
    void Cancel() noexcept;
    static unsigned OutstandingRequests() noexcept;
private:
    struct State;
    State* state_ = nullptr;
    NativeAddress immediate_{};
    bool immediateReady_ = false;
};
}
