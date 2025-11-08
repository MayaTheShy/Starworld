// OverteClient.cpp
#include "OverteClient.hpp"

#include <chrono>
#include <cmath>
#include <iostream>
#include <glm/gtc/matrix_transform.hpp>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

using namespace std::chrono_literals;

bool OverteClient::connect() {
    // Basic reachability check (TCP) if ws://host:port specified.
    // Format expected: ws://host:port
    auto posScheme = m_domainUrl.find("ws://");
    if (posScheme != 0) {
        std::cerr << "[OverteClient] Unexpected URL scheme; expected ws://" << std::endl;
    }
    auto hostPort = m_domainUrl.substr(5); // strip ws://
    auto colon = hostPort.find(':');
    std::string host = colon == std::string::npos ? hostPort : hostPort.substr(0, colon);
    int port = colon == std::string::npos ? 40102 : std::stoi(hostPort.substr(colon + 1));

    // Attempt a non-blocking TCP connect (best-effort; ignore failure but warn).
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd != -1) {
        sockaddr_in addr{}; addr.sin_family = AF_INET; addr.sin_port = htons(port);
        addr.sin_addr.s_addr = INADDR_ANY; // Skip DNS for stub; real impl would resolve host.
        if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == -1) {
            std::cerr << "[OverteClient] Warning: unable to reach Overte domain (stub)." << std::endl;
        } else {
            std::cout << "[OverteClient] (Stub) TCP connect succeeded to " << host << ':' << port << std::endl;
        }
        ::close(fd);
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
    // TODO: Use Overte networking layer (NodeList) to connect to AvatarMixer.
    m_avatarMixer = true;
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
