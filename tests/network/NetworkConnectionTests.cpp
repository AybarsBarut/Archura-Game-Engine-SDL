#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "network/NetworkManager.h"
#include "network/NativeSocket.h"
#include "network/AsyncResolver.h"
#include <chrono>
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <thread>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#endif

namespace {
using Clock = std::chrono::steady_clock;
using namespace Archura;
using namespace Archura::Net;
double maxCallMs = 0;
void Require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
template<class F> void Timed(F f) {
    const auto start = Clock::now(); f();
    maxCallMs = std::max(maxCallMs, std::chrono::duration<double, std::milli>(Clock::now()-start).count());
    Require(maxCallMs < 250, "transport call exceeded practical 250ms test bound");
}
template<class F> void Until(F f) {
    const auto end = Clock::now() + std::chrono::seconds(3);
    while (!f()) {
        Require(Clock::now() < end, "connection test deadline exceeded");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
std::uint16_t FreePort() {
    const auto socketHandle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    sockaddr_in address{}; address.sin_family=AF_INET; address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    Require(bind(socketHandle,reinterpret_cast<sockaddr*>(&address),sizeof(address))==0,"reserve port failed");
#ifdef _WIN32
    int length=sizeof(address);
#else
    socklen_t length=sizeof(address);
#endif
    Require(getsockname(socketHandle,reinterpret_cast<sockaddr*>(&address),&length)==0,"port lookup failed");
#ifdef _WIN32
    closesocket(socketHandle);
#else
    close(socketHandle);
#endif
    return ntohs(address.sin_port);
}
void RefusedAndCancelled() {
    auto& n=NetworkManager::Get();
    const auto port=FreePort();
    Timed([&]{Require(n.Connect("127.0.0.1",port),"numeric attempt rejected");});
    Require(n.GetState()==ConnectionState::Connecting&&!n.IsConnected(),"request mislabeled connected");
    Until([&]{Timed([&]{n.UpdateClient();});return n.GetState()==ConnectionState::Failed;});
    Require(n.GetLastDisconnectReason()==DisconnectReason::TransportError,"refusal reason lost");
    Timed([&]{Require(n.Reconnect(),"retry not accepted");n.Shutdown();});
    Require(n.GetState()==ConnectionState::Stopped,"pending TCP shutdown failed");
    Require(!n.Connect(std::string("127.0.0.1\0wrong",15),port),"embedded NUL accepted");
}
void HandshakeAndPeerClose() {
    auto& n=NetworkManager::Get();
    std::string error;
    const auto port=FreePort();
    auto listener=NativeSocket::Listen(port,error); Require(bool(listener),"native listen failed");
    Require(n.Connect("localhost",port),"localhost attempt rejected");
    NativeSocket peer;
    StreamFrameParser parser;
    std::uint64_t token=0;
    Until([&]{
        Timed([&]{n.UpdateClient();});
        if (!peer) { IoStatus status; peer=listener.Accept(status,error); Require(status!=IoStatus::Error,"accept failed"); }
        if (peer) {
            std::uint8_t bytes[1024]; const auto read=peer.Receive(bytes,sizeof(bytes),error);
            Require(read.status!=IoStatus::Error&&read.status!=IoStatus::Closed,"hello socket failed");
            if (read.status==IoStatus::Ready) {
                const auto batch=parser.Feed(bytes,read.bytes); Require(bool(batch),"invalid hello frame");
                for(const auto& frame:batch.frames) {
                    Require(frame.header.type==PacketType::Connect&&frame.header.sequence==1,"hello identity changed");
                    Require(DeserializeSessionToken(frame.payload,token),"invalid hello token");
                }
            }
        }
        return token!=0;
    });
    Require(n.GetState()==ConnectionState::Handshaking&&!n.IsConnected(),"TCP falsely completed handshake");
    const auto welcome=EncodeFrame(PacketType::ConnectAck,1,SerializeWelcome(token,42));
    std::size_t sent=0;
    Until([&]{
        const auto write=peer.Send(welcome.data()+sent,welcome.size()-sent,error);
        Require(write.status!=IoStatus::Error&&write.status!=IoStatus::Closed,"welcome send failed");
        if(write.status==IoStatus::Ready) sent+=write.bytes;
        return sent==welcome.size();
    });
    Until([&]{Timed([&]{n.UpdateClient();});return n.IsConnected();});
    Require(n.GetLocalPeerId()==42,"server identity lost");
    bool disconnected=false;
    n.SetOnDisconnected([&](DisconnectReason reason){disconnected=reason==DisconnectReason::PeerClosed;});
    peer.Close();
    Until([&]{Timed([&]{n.UpdateClient();});return n.GetState()==ConnectionState::Failed;});
    Require(disconnected,"peer-close callback missing");
    n.SetOnDisconnected({}); n.Shutdown(); n.Shutdown();
}
void ResolverLifetime() {
    std::string error;
    AsyncResolver resolver;
    Require(!resolver.Start(std::string("localhost\0bad",13),1,error),"resolver accepted NUL");
    Require(!resolver.Start(std::string(254,'a'),1,error),"resolver accepted oversized hostname");
    Require(resolver.Start("127.0.0.1",123,error),"numeric resolver failed");
    NativeAddress address; Require(resolver.Poll(address,error)==IoStatus::Ready&&address.port==123,"numeric resolution wrong");
    resolver.Cancel(); resolver.Cancel();
    auto& n=NetworkManager::Get();
#ifdef _WIN32
    // Start/cancel OS callback-mode resolution repeatedly, immediately replace
    // it with a numeric request. A late DNS completion must not touch that session.
    for(int i=0;i<64;++i) {
        Timed([&]{n.Connect("archura-cancel-test.invalid",123);n.Shutdown();});
        Require(AsyncResolver::OutstandingRequests()<=4,"DNS admission cap exceeded");
        Timed([&]{Require(n.Connect("127.0.0.1",123),"numeric request after DNS cancel rejected");});
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        Require(n.GetState()==ConnectionState::Connecting,"stale DNS completion mutated new session");
        Timed([&]{n.Shutdown();});
    }
    // Cancellation completion releases bounded request storage; do not wait
    // inside production Shutdown. This outer test watchdog observes cleanup.
    Until([&]{return AsyncResolver::OutstandingRequests()==0;});
#else
    Require(!resolver.Start("archura.invalid",123,error),"unsupported hostname unexpectedly resolved");
#endif
}
}
int main() {
    std::string error;
    try {
        Require(NativeSocket::Startup(error),"native startup failed");
        RefusedAndCancelled(); HandshakeAndPeerClose(); ResolverLifetime();
        NativeSocket::Cleanup();
        std::cout<<"Network connection/cancellation contracts passed; max_call_ms="<<maxCallMs<<'\n';
        return 0;
    } catch(const std::exception& e) {
        NetworkManager::Get().SetOnDisconnected({}); NetworkManager::Get().Shutdown();
        NativeSocket::Cleanup(); std::cerr<<e.what()<<'\n'; return 1;
    }
}
