#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
namespace Archura::Net {
// Internal IPv4 address in network byte order.
struct NativeAddress { std::uint32_t ipv4 = 0; std::uint16_t port = 0; };
enum class IoStatus { Ready, Pending, Closed, Error };
struct IoResult { IoStatus status; std::size_t bytes = 0; };
class NativeSocket final {
public:
    NativeSocket() noexcept = default;
    ~NativeSocket();
    NativeSocket(NativeSocket&& other) noexcept;
    NativeSocket& operator=(NativeSocket&& other) noexcept;
    NativeSocket(const NativeSocket&) = delete;
    NativeSocket& operator=(const NativeSocket&) = delete;
    explicit operator bool() const noexcept { return handle_ != invalid; }
    static bool Startup(std::string& error);
    static void Cleanup() noexcept;
    static NativeSocket Listen(std::uint16_t port, std::string& error);
    static NativeSocket Connect(const NativeAddress&, IoStatus&, std::string& error);
    NativeSocket Accept(IoStatus&, std::string& error);
    IoStatus PollConnected(std::string& error);
    IoResult Send(const std::uint8_t*, std::size_t, std::string& error);
    IoResult Receive(std::uint8_t*, std::size_t, std::string& error);
    void Close() noexcept;
private:
    static constexpr std::uintptr_t invalid = ~std::uintptr_t{0};
    explicit NativeSocket(std::uintptr_t handle) noexcept : handle_(handle) {}
    bool SetNonblocking(std::string& error, bool stream = true);
    std::uintptr_t handle_ = invalid;
};
}
