// OverteClient.cpp
#include "OverteClient.hpp"

#include <chrono>
#include <cmath>
#include <iostream>
#include <glm/gtc/matrix_transform.hpp>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>

using namespace std::chrono_literals;

bool OverteClient::connect() {
    // Parse ws://host:port
    std::string url = m_domainUrl;
    if (url.empty()) url = "ws://127.0.0.1:40102";
    if (url.rfind("ws://", 0) == 0) url = url.substr(5);
    auto colon = url.find(':');
    m_host = colon == std::string::npos ? url : url.substr(0, colon);
    m_port = colon == std::string::npos ? 40102 : std::stoi(url.substr(colon + 1));

    // Resolve host:port
    addrinfo hints{}; hints.ai_socktype = SOCK_STREAM; hints.ai_family = AF_UNSPEC;
    addrinfo* res = nullptr;
    int gai = ::getaddrinfo(m_host.c_str(), std::to_string(m_port).c_str(), &hints, &res);
    if (gai != 0) {
        std::cerr << "[OverteClient] getaddrinfo failed for " << m_host << ":" << m_port << " - " << gai_strerror(gai) << std::endl;
    } else {
        // Attempt TCP reachability for diagnostics
        int fd = -1; addrinfo* rp = res;
        for (; rp; rp = rp->ai_next) {
            fd = ::socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
            if (fd == -1) continue;
            ::fcntl(fd, F_SETFL, O_NONBLOCK);
            int c = ::connect(fd, rp->ai_addr, rp->ai_addrlen);
            if (c == 0 || (c == -1 && errno == EINPROGRESS)) {
                std::cout << "[OverteClient] TCP reachable (non-blocking) to " << m_host << ":" << m_port << std::endl;
                ::close(fd); fd = -1; break;
            }
            ::close(fd); fd = -1;
        }
        ::freeaddrinfo(res);
        if (fd == -1) {
            // Not necessarily fatal; mixers are UDP. Continue with UDP.
        }
    }

    // Setup UDP to target (avatar mixer guess: same port by default)
    addrinfo uhints{}; uhints.ai_socktype = SOCK_DGRAM; uhints.ai_family = AF_UNSPEC;
    addrinfo* ures = nullptr;
    int ugai = ::getaddrinfo(m_host.c_str(), std::to_string(m_port).c_str(), &uhints, &ures);
    if (ugai != 0) {
        std::cerr << "[OverteClient] UDP resolve failed: " << gai_strerror(ugai) << std::endl;
    } else {
        for (addrinfo* rp = ures; rp; rp = rp->ai_next) {
            m_udpFd = ::socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
            if (m_udpFd == -1) continue;
            ::fcntl(m_udpFd, F_SETFL, O_NONBLOCK);
            std::memcpy(&m_udpAddr, rp->ai_addr, rp->ai_addrlen);
            m_udpAddrLen = rp->ai_addrlen;
            m_udpReady = true;
            std::cout << "[OverteClient] UDP socket ready for " << m_host << ":" << m_port << std::endl;
            break;
        }
        ::freeaddrinfo(ures);
    }

    // Simulate successful connections to mixers.
    m_connected = connectAvatarMixer() && connectEntityServer() && connectAudioMixer();
    if (!m_connected) {
        std::cerr << "OverteClient: failed to connect one or more mixers" << std::endl;
        return false;
    }

    // Seed a couple of demo entities.
    OverteEntity a{ m_nextEntityId++, "CubeA", glm::mat4(1.0f) };
    OverteEntity b{ m_nextEntityId++, "CubeB", glm::mat4(1.0f) };
    m_entities.emplace(a.id, a);
    m_entities.emplace(b.id, b);
    m_updateQueue.push_back(a.id);
    m_updateQueue.push_back(b.id);
    return true;
}

bool OverteClient::connectAvatarMixer() {
    // For now, consider UDP socket readiness as mixer connectivity proxy.
    m_avatarMixer = m_udpReady;
    return true;
}

bool OverteClient::connectEntityServer() {
    // TODO: Connect to EntityServer and subscribe to updates.
    m_entityServer = true;
    return true;
}

bool OverteClient::connectAudioMixer() {
    // TODO: Connect AudioMixer for voice chat.
    m_audioMixer = true;
    return true;
}

void OverteClient::poll() {
    if (!m_connected) return;

    // Try a lightweight UDP ping if ready
    if (m_udpReady && m_udpFd != -1) {
        const char ping[4] = {'P','I','N','G'};
        ssize_t s = ::sendto(m_udpFd, ping, sizeof(ping), 0, reinterpret_cast<sockaddr*>(&m_udpAddr), m_udpAddrLen);
        if (s == -1 && errno != EWOULDBLOCK && errno != EAGAIN) {
            std::cerr << "[OverteClient] UDP send failed: " << std::strerror(errno) << std::endl;
        }
        char buf[1500];
        sockaddr_storage from{}; socklen_t fromlen = sizeof(from);
        ssize_t r = ::recvfrom(m_udpFd, buf, sizeof(buf), 0, reinterpret_cast<sockaddr*>(&from), &fromlen);
        if (r > 0) {
            std::cout << "[OverteClient] UDP packet received (" << r << " bytes)" << std::endl;
        }
    }

    // Simulate entity transforms changing slightly over time.
    static auto t0 = std::chrono::steady_clock::now();
    const float t = std::chrono::duration<float>(std::chrono::steady_clock::now() - t0).count();

    for (auto& [id, e] : m_entities) {
        const float r = 0.25f + 0.05f * static_cast<float>(id);
        const float x = std::cos(t * 0.5f + static_cast<float>(id)) * r;
        const float z = std::sin(t * 0.5f + static_cast<float>(id)) * r;
        e.transform = glm::translate(glm::mat4(1.0f), glm::vec3{x, 1.25f, z});
        m_updateQueue.push_back(id);
    }
}

void OverteClient::sendMovementInput(const glm::vec3& linearVelocity) {
    // TODO: Package and send to AvatarMixer as appropriate (e.g. MyAvatar data).
    (void)linearVelocity; // silence unused warning in the stub
}

std::vector<OverteEntity> OverteClient::consumeUpdatedEntities() {
    std::vector<OverteEntity> out;
    out.reserve(m_updateQueue.size());
    for (auto id : m_updateQueue) {
        auto it = m_entities.find(id);
        if (it != m_entities.end()) out.push_back(it->second);
    }
    m_updateQueue.clear();
    return out;
}
