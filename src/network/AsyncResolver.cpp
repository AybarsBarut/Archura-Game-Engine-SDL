#ifdef _WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0602
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif
#include "AsyncResolver.h"
#if defined(ARCHURA_HAS_SDL_NET)
#include <atomic>
#include <exception>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif
namespace Archura::Net {
#ifdef _WIN32
namespace { std::atomic<unsigned> liveQueries{0}; }
struct AsyncResolver::State {
    struct Completion : OVERLAPPED { State* owner = nullptr; } overlapped{};
    std::atomic<unsigned> refs{2}; // owner + completion, installed before launch
    std::atomic<bool> done{false};
    std::wstring host;
    ADDRINFOEXW hints{};
    PADDRINFOEXW result = nullptr;
    HANDLE cancel = nullptr;
    int status = WSA_IO_PENDING;
    std::uint16_t port = 0;
    bool winsock = false;
    void Release() noexcept {
        if (refs.fetch_sub(1, std::memory_order_acq_rel) != 1) return;
        if (result) FreeAddrInfoExW(result);
        if (winsock) WSACleanup();
        liveQueries.fetch_sub(1, std::memory_order_relaxed);
        delete this;
    }
    void Complete(int code) noexcept {
        status = code;
        done.store(true, std::memory_order_release);
        Release();
    }
    static void CALLBACK Completed(DWORD code, DWORD, LPOVERLAPPED overlapped) noexcept {
        static_cast<Completion*>(overlapped)->owner->Complete(static_cast<int>(code));
    }
};
#else
struct AsyncResolver::State {};
#endif
AsyncResolver::~AsyncResolver() { Cancel(); }
bool AsyncResolver::Start(const std::string& host, std::uint16_t port, std::string& error) {
    Cancel();
    if (host.empty() || host.size() > 253 || host.find('\0') != std::string::npos || port == 0) {
        error = "invalid hostname or port"; return false;
    }
    in_addr numeric{};
    if (host == "localhost") numeric.s_addr = htonl(INADDR_LOOPBACK);
    else if (inet_pton(AF_INET, host.c_str(), &numeric) != 1) {
#ifdef _WIN32
        unsigned count = liveQueries.load(std::memory_order_relaxed);
        do { if (count >= 4) { error = "asynchronous DNS capacity exhausted"; return false; } }
        while (!liveQueries.compare_exchange_weak(count, count + 1, std::memory_order_relaxed));
        State* state = nullptr;
        try {
            state = new State;
            WSADATA data{};
            const int startup = WSAStartup(MAKEWORD(2, 2), &data);
            if (startup != 0) { error = "DNS Winsock startup failed"; state->refs.store(1); state->Release(); return false; }
            state->winsock = true;
            // UTF-8 conversion is bounded by the validated host length.
            const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, host.data(), static_cast<int>(host.size()), nullptr, 0);
            if (length <= 0) { error = "invalid UTF-8 hostname"; state->refs.store(1); state->Release(); return false; }
            state->host.resize(static_cast<std::size_t>(length));
            MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, host.data(), static_cast<int>(host.size()), state->host.data(), length);
            state->hints.ai_family = AF_INET;
            state->hints.ai_socktype = SOCK_STREAM;
            state->hints.ai_protocol = IPPROTO_TCP;
            state->port = port;
            state->overlapped.owner = state;
            state_ = state;
            const int result = GetAddrInfoExW(state->host.c_str(), nullptr, NS_DNS, nullptr,
                &state->hints, &state->result, nullptr, &state->overlapped, State::Completed, &state->cancel);
            // Callback is promised only for pending operations.
            if (result != WSA_IO_PENDING) state->Complete(result);
            return true;
        } catch (const std::exception&) {
            state_ = nullptr;
            if (state) { state->refs.store(1); state->Release(); }
            else liveQueries.fetch_sub(1, std::memory_order_relaxed);
            error = "asynchronous DNS allocation failed";
            return false;
        }
#else
        error = "hostname DNS is unsupported on this platform; use IPv4 or localhost";
        return false;
#endif
    }
    immediate_ = {numeric.s_addr, port}; immediateReady_ = true; return true;
}
IoStatus AsyncResolver::Poll(NativeAddress& address, std::string& error) {
    if (immediateReady_) { address = immediate_; return IoStatus::Ready; }
#ifdef _WIN32
    if (state_ && !state_->done.load(std::memory_order_acquire)) return IoStatus::Pending;
    if (state_) {
        if (state_->status == 0) {
            for (auto* entry = state_->result; entry; entry = entry->ai_next) {
                if (entry->ai_family == AF_INET && entry->ai_addrlen >= sizeof(sockaddr_in)) {
                    address = {reinterpret_cast<const sockaddr_in*>(entry->ai_addr)->sin_addr.s_addr, state_->port};
                    return IoStatus::Ready;
                }
            }
        }
        error = "asynchronous DNS failed (error " + std::to_string(state_->status) + ")";
    }
#else
    (void)address;
#endif
    if (error.empty()) error = "resolver has no active request";
    return IoStatus::Error;
}
unsigned AsyncResolver::OutstandingRequests() noexcept {
#ifdef _WIN32
    return liveQueries.load(std::memory_order_relaxed);
#else
    return 0;
#endif
}
void AsyncResolver::Cancel() noexcept {
    immediateReady_ = false;
#ifdef _WIN32
    auto* state = state_; state_ = nullptr;
    if (!state) return;
    if (!state->done.load(std::memory_order_acquire) && state->cancel) GetAddrInfoExCancel(&state->cancel);
    state->Release();
#endif
}
}
#endif
