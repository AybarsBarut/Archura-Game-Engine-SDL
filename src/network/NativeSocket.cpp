#include "NativeSocket.h"
#if defined(ARCHURA_HAS_SDL_NET)
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <poll.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#endif
#include <algorithm>
#include <limits>
#include <utility>
namespace Archura::Net {
namespace {
#ifdef _WIN32
using Handle = SOCKET;
int LastError() { return WSAGetLastError(); }
bool Pending(int e) { return e == WSAEWOULDBLOCK || e == WSAEINPROGRESS || e == WSAEALREADY || e == WSAEINTR; }
#else
using Handle = int;
int LastError() { return errno; }
bool Pending(int e) { return e == EWOULDBLOCK || e == EAGAIN || e == EINPROGRESS || e == EALREADY || e == EINTR; }
#endif
void Error(std::string& message, const char* operation, int e) { message = std::string(operation) + " failed (socket error " + std::to_string(e) + ")"; }
}
NativeSocket::~NativeSocket() { Close(); }
NativeSocket::NativeSocket(NativeSocket&& other) noexcept : handle_(std::exchange(other.handle_, invalid)) {}
NativeSocket& NativeSocket::operator=(NativeSocket&& other) noexcept { if (this != &other) { Close(); handle_ = std::exchange(other.handle_, invalid); } return *this; }
bool NativeSocket::Startup(std::string& error) {
#ifdef _WIN32
    WSADATA data{}; const int result = WSAStartup(MAKEWORD(2,2), &data); if (result) { Error(error, "WSAStartup", result); return false; }
#else
    (void)error;
#endif
    return true;
}
void NativeSocket::Cleanup() noexcept {
#ifdef _WIN32
    WSACleanup();
#endif
}
void NativeSocket::Close() noexcept { if (!*this) return;
#ifdef _WIN32
    closesocket(static_cast<Handle>(handle_));
#else
    close(static_cast<Handle>(handle_));
#endif
    handle_ = invalid;
}
bool NativeSocket::SetNonblocking(std::string& error, bool stream) {
#ifdef _WIN32
    u_long enabled = 1; const int result = ioctlsocket(static_cast<Handle>(handle_), FIONBIO, &enabled);
#else
    const int flags = fcntl(static_cast<Handle>(handle_), F_GETFL, 0);
    const int result = flags < 0 ? -1 : fcntl(static_cast<Handle>(handle_), F_SETFL, flags | O_NONBLOCK);
#endif
    if (result != 0) { Error(error, "nonblocking socket", LastError()); return false; }
#ifdef SO_NOSIGPIPE
    int enabled = 1; setsockopt(static_cast<Handle>(handle_), SOL_SOCKET, SO_NOSIGPIPE, &enabled, sizeof(enabled));
#endif
    if (stream) {
        int noDelay = 1;
        if (setsockopt(static_cast<Handle>(handle_), IPPROTO_TCP, TCP_NODELAY,
            reinterpret_cast<const char*>(&noDelay), sizeof(noDelay)) != 0) {
            Error(error, "TCP_NODELAY", LastError()); return false;
        }
    }
    return true;
}
NativeSocket NativeSocket::Listen(std::uint16_t port, std::string& error) {
    NativeSocket s(static_cast<std::uintptr_t>(socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)));
    if (!s) { Error(error, "socket", LastError()); return {}; }
    if (!s.SetNonblocking(error, false)) return {};
#ifndef _WIN32
    int reuse = 1;
    setsockopt(static_cast<Handle>(s.handle_), SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));
#endif
    sockaddr_in address{}; address.sin_family = AF_INET; address.sin_addr.s_addr = htonl(INADDR_ANY); address.sin_port = htons(port);
    if (bind(static_cast<Handle>(s.handle_), reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 || listen(static_cast<Handle>(s.handle_), SOMAXCONN) != 0) { Error(error, "listen", LastError()); return {}; }
    return s;
}
NativeSocket NativeSocket::Connect(const NativeAddress& address, IoStatus& status, std::string& error) {
    status = IoStatus::Error;
    NativeSocket s(static_cast<std::uintptr_t>(socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)));
    if (!s) { Error(error, "socket", LastError()); return {}; }
    if (!s.SetNonblocking(error)) return {};
    sockaddr_in target{}; target.sin_family = AF_INET; target.sin_addr.s_addr = address.ipv4; target.sin_port = htons(address.port);
    if (connect(static_cast<Handle>(s.handle_), reinterpret_cast<sockaddr*>(&target), sizeof(target)) == 0) status = IoStatus::Ready;
    else { const int e = LastError(); if (!Pending(e)) { Error(error, "connect", e); return {}; } status = IoStatus::Pending; }
    return s;
}
NativeSocket NativeSocket::Accept(IoStatus& status, std::string& error) {
    status = IoStatus::Error;
    NativeSocket s(static_cast<std::uintptr_t>(accept(static_cast<Handle>(handle_), nullptr, nullptr)));
    if (!s) { const int e = LastError(); if (Pending(e)) status = IoStatus::Pending; else Error(error, "accept", e); return {}; }
    if (!s.SetNonblocking(error)) return {};
    status = IoStatus::Ready; return s;
}
IoStatus NativeSocket::PollConnected(std::string& error) {
#ifdef _WIN32
    fd_set writable, failed;
    FD_ZERO(&writable); FD_ZERO(&failed);
    FD_SET(static_cast<Handle>(handle_), &writable); FD_SET(static_cast<Handle>(handle_), &failed);
    timeval zero{};
    const int result = select(0, nullptr, &writable, &failed, &zero);
#else
    pollfd descriptor{};
    descriptor.fd = static_cast<Handle>(handle_);
    descriptor.events = POLLOUT;
    const int result = poll(&descriptor, 1, 0);
#endif
    if (result == 0) return IoStatus::Pending;
    if (result < 0) { const int e = LastError(); if (Pending(e)) return IoStatus::Pending; Error(error, "connect readiness", e); return IoStatus::Error; }
    int e = 0;
#ifdef _WIN32
    int length = sizeof(e);
#else
    socklen_t length = sizeof(e);
#endif
    if (getsockopt(static_cast<Handle>(handle_), SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&e), &length) != 0) e = LastError();
    if (e) { Error(error, "connect", e); return IoStatus::Error; }
    return IoStatus::Ready;
}
IoResult NativeSocket::Send(const std::uint8_t* data, std::size_t size, std::string& error) {
    const int count = static_cast<int>(std::min(size, static_cast<std::size_t>(std::numeric_limits<int>::max())));
    const auto result = send(static_cast<Handle>(handle_), reinterpret_cast<const char*>(data), count,
#ifdef MSG_NOSIGNAL
        MSG_NOSIGNAL
#else
        0
#endif
    );
    if (result > 0) return {IoStatus::Ready, static_cast<std::size_t>(result)};
    if (result == 0) return {IoStatus::Closed};
    const int e = LastError(); if (Pending(e)) return {IoStatus::Pending}; Error(error, "send", e); return {IoStatus::Error};
}
IoResult NativeSocket::Receive(std::uint8_t* data, std::size_t size, std::string& error) {
    const int result = static_cast<int>(recv(static_cast<Handle>(handle_), reinterpret_cast<char*>(data), static_cast<int>(size), 0));
    if (result > 0) return {IoStatus::Ready, static_cast<std::size_t>(result)};
    if (result == 0) return {IoStatus::Closed};
    const int e = LastError(); if (Pending(e)) return {IoStatus::Pending}; Error(error, "receive", e); return {IoStatus::Error};
}
}
#endif
