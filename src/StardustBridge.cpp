// StardustBridge.cpp
#include "StardustBridge.hpp"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <thread>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <vector>
#include <algorithm>
#include <cstring>
#include <dlfcn.h>

using namespace std::chrono_literals;

static std::vector<std::string> candidateSocketPaths() {
    std::vector<std::string> out;

    if (const char* envSock = std::getenv("STARDUSTXR_SOCKET")) out.emplace_back(envSock);
    if (const char* envSock2 = std::getenv("STARDUST_SOCKET")) out.emplace_back(envSock2);
    if (const char* envAbs = std::getenv("STARDUSTXR_ABSTRACT")) {
        // If provided without @, add @ prefix for abstract namespace
        std::string v = envAbs;
        if (v.empty() || v[0] != '@') v = '@' + v;
        out.emplace_back(v);
    }

    std::string xdg;
    if (const char* env = std::getenv("XDG_RUNTIME_DIR")) xdg = env;
    if (!xdg.empty()) {
        out.emplace_back(xdg + "/stardust.sock");
        out.emplace_back(xdg + "/stardustxr.sock");
        out.emplace_back(xdg + "/stardust/stardust.sock");
        out.emplace_back(xdg + "/stardustxr/stardust.sock");
    }

    // /run/user/<uid>/...
    char uidPath[128];
    std::snprintf(uidPath, sizeof(uidPath), "/run/user/%d", (int)getuid());
    std::string runUser(uidPath);
    out.emplace_back(runUser + "/stardust.sock");
    out.emplace_back(runUser + "/stardustxr.sock");
    out.emplace_back(runUser + "/stardust/stardust.sock");
    out.emplace_back(runUser + "/stardustxr/stardust.sock");

    out.emplace_back("/tmp/stardustxr.sock");

    // Common abstract names to try as a fallback
    out.emplace_back("@stardust");
    out.emplace_back("@stardustxr");
    out.emplace_back("@stardust/stardust");
    out.emplace_back("@stardustxr/stardust");
    return out;
}

std::string StardustBridge::defaultSocketPath() {
    auto c = candidateSocketPaths();
    return c.empty() ? std::string{} : c.front();
}

bool StardustBridge::connect(const std::string& socketPath) {
    // Prefer Rust bridge if available.
    if (loadBridge()) {
        const char* appId = "org.stardustxr.starworld";
        int rc = m_fnStart ? m_fnStart(appId) : -1;
        if (rc == 0) {
            m_connected = true;
            std::cout << "[StardustBridge] Connected via Rust bridge (C-ABI)." << std::endl;
            m_overteRoot = createNode("OverteWorld");
            // Set root node to type 0 (Unknown) with zero dimensions so it doesn't render
            setNodeEntityType(*m_overteRoot, 0); 
            setNodeDimensions(*m_overteRoot, glm::vec3(0.0f, 0.0f, 0.0f));
            return true;
        } else {
            std::cerr << "[StardustBridge] Rust bridge present but start() failed (rc=" << rc << ")" << std::endl;
        }
    }

    std::vector<std::string> paths;
    if (!socketPath.empty()) paths.push_back(socketPath);
    auto candidates = candidateSocketPaths();
    paths.insert(paths.end(), candidates.begin(), candidates.end());

    // Deduplicate while preserving order
    std::vector<std::string> unique;
    for (auto& p : paths) {
        if (!p.empty() && std::find(unique.begin(), unique.end(), p) == unique.end()) unique.push_back(p);
    }

    for (const auto& p : unique) {
        // Try to connect regardless of fs existence—server may create on first accept.
        bool isAbstract = !p.empty() && p[0] == '@';
        int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd == -1) {
            continue;
        }

        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        if (isAbstract) {
            // Linux abstract namespace: first byte of sun_path is NUL, name in the rest.
            // p begins with '@' per our convention; skip it when copying.
            std::memset(addr.sun_path, 0, sizeof(addr.sun_path));
            std::snprintf(addr.sun_path + 1, sizeof(addr.sun_path) - 1, "%s", p.c_str() + 1);
        } else {
            std::snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", p.c_str());
        }

        if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == -1) {
            ::close(fd);
            continue;
        }

        // Make non-blocking after successful connect
        int flags = ::fcntl(fd, F_GETFL, 0);
        if (flags != -1) ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);

        m_socketFd = fd;
        m_socketPath = p;
        m_connected = true;
    std::cout << "[StardustBridge] Connected to compositor at " << (isAbstract ? ("abstract:" + p.substr(1)) : p) << std::endl;

        m_overteRoot = createNode("OverteWorld");
        // Set root node to type 0 (Unknown) with zero dimensions so it doesn't render
        setNodeEntityType(*m_overteRoot, 0);
        setNodeDimensions(*m_overteRoot, glm::vec3(0.0f, 0.0f, 0.0f));
        return true;
    }

    std::cerr << "[StardustBridge] Could not connect to StardustXR. Tried:" << std::endl;
    for (auto& p : unique) std::cerr << "  - " << p << std::endl;
    std::cerr << "Hint: set STARDUSTXR_SOCKET to a filesystem path, or STARDUSTXR_ABSTRACT to an abstract name (e.g. export STARDUSTXR_ABSTRACT=stardustxr). Leading '@' denotes abstract." << std::endl;
    return false;
}

StardustBridge::NodeId StardustBridge::createNode(const std::string& name,
                                                  const glm::mat4& transform,
                                                  std::optional<NodeId> parent) {
    NodeId id = m_nextId++;
    m_nodes.emplace(id, Node{ name, parent, transform });
    // Forward to Rust bridge if available.
    if (m_fnCreateNode) {
        float m[16];
        // GLM mat4 is column-major; pass as 16 floats as-is
        std::memcpy(m, &transform[0][0], sizeof(m));
        std::uint64_t rid = m_fnCreateNode(name.c_str(), m);
        (void)rid; // Could map to NodeId if Rust returns its own id
    }
    return id;
}

bool StardustBridge::updateNodeTransform(NodeId id, const glm::mat4& transform) {
    auto it = m_nodes.find(id);
    if (it == m_nodes.end()) return false;
    it->second.transform = transform;
    if (m_fnUpdateNode) {
        float m[16];
        std::memcpy(m, &transform[0][0], sizeof(m));
        (void)m_fnUpdateNode(id, m);
    }
    return true;
}

bool StardustBridge::removeNode(NodeId id) {
    auto it = m_nodes.find(id);
    if (it == m_nodes.end()) return false;
    m_nodes.erase(it);
    if (m_fnRemoveNode) {
        (void)m_fnRemoveNode(id);
    }
    return true;
}

bool StardustBridge::setNodeModel(NodeId id, const std::string& modelUrl) {
    auto it = m_nodes.find(id);
    if (it == m_nodes.end()) return false;
    if (m_fnSetModel) {
        return m_fnSetModel(id, modelUrl.c_str()) == 0;
    }
    return true;
}

bool StardustBridge::setNodeTexture(NodeId id, const std::string& textureUrl) {
    auto it = m_nodes.find(id);
    if (it == m_nodes.end()) return false;
    if (m_fnSetTexture) {
        return m_fnSetTexture(id, textureUrl.c_str()) == 0;
    }
    return true;
}

bool StardustBridge::setNodeColor(NodeId id, const glm::vec3& color, float alpha) {
    auto it = m_nodes.find(id);
    if (it == m_nodes.end()) return false;
    if (m_fnSetColor) {
        return m_fnSetColor(id, color.r, color.g, color.b, alpha) == 0;
    } else {
        std::cerr << "[StardustBridge] Warning: setNodeColor called but m_fnSetColor is null" << std::endl;
    }
    return true;
}

bool StardustBridge::setNodeDimensions(NodeId id, const glm::vec3& dimensions) {
    auto it = m_nodes.find(id);
    if (it == m_nodes.end()) return false;
    if (m_fnSetDimensions) {
        return m_fnSetDimensions(id, dimensions.x, dimensions.y, dimensions.z) == 0;
    }
    return true;
}

bool StardustBridge::setNodeEntityType(NodeId id, uint8_t entityType) {
    auto it = m_nodes.find(id);
    if (it == m_nodes.end()) return false;
    if (m_fnSetEntityType) {
        return m_fnSetEntityType(id, entityType) == 0;
    }
    return true;
}

void StardustBridge::poll() {
    if (!m_connected) return;

    if (m_fnPoll) {
        int rc = m_fnPoll();
        if (rc < 0) {
            std::cerr << "[StardustBridge] Bridge reported disconnected; shutting down." << std::endl;
            m_running = false;
            m_connected = false;
            return;
        }
    }

    // Detect disconnect: a non-blocking read of 0 or error indicating closed.
    if (m_socketFd < 0) return;
    char buf;
    ssize_t n = ::recv(m_socketFd, &buf, 1, MSG_PEEK);
    if (n == 0) {
        std::cerr << "[StardustBridge] Compositor socket closed" << std::endl;
        m_connected = false;
        m_running = false; // Request shutdown
        return;
    } else if (n == -1 && (errno == ECONNRESET || errno == ENOTCONN)) {
        std::cerr << "[StardustBridge] Compositor connection reset" << std::endl;
        m_connected = false;
        m_running = false;
        return;
    } else if (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        // No data pending; connection still alive.
    }

    // TODO: poll actual StardustXR event queue & input devices.
    // Simulate input for now: small circular joystick motion over time.
    static auto start = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    float t = std::chrono::duration<float>(now - start).count();
    m_joystick = { std::sin(t * 0.5f), std::cos(t * 0.5f) };

    // Head pose remains identity; in real implementation populate from HMD tracking.
    m_headPose = glm::mat4(1.0f);
}

void StardustBridge::close() {
    if (m_fnShutdown) m_fnShutdown();
    if (m_socketFd >= 0) {
        ::close(m_socketFd);
        m_socketFd = -1;
    }
    m_connected = false;
}

// Ensure socket is closed on destruction
StardustBridge::~StardustBridge() { close(); }

bool StardustBridge::loadBridge() {
    if (m_bridgeHandle) return true;

    const char* overridePath = std::getenv("STARWORLD_BRIDGE_PATH");
    std::vector<std::string> candidates;
    if (overridePath) {
        candidates.emplace_back(std::string(overridePath));
    }
    // Likely local dev output
    candidates.emplace_back("./bridge/target/debug/libstardust_bridge.so");
    candidates.emplace_back("libstardust_bridge.so");

    for (const auto& path : candidates) {
        void* h = ::dlopen(path.c_str(), RTLD_LAZY | RTLD_LOCAL);
        if (!h) continue;
        auto req = [&](const char* sym){ return ::dlsym(h, sym); };
        m_fnStart = reinterpret_cast<fn_start_t>(req("sdxr_start"));
        if (!m_fnStart) m_fnStart = reinterpret_cast<fn_start_t>(req("_sdxr_start"));
        m_fnPoll = reinterpret_cast<fn_poll_t>(req("sdxr_poll"));
        m_fnShutdown = reinterpret_cast<fn_shutdown_t>(req("sdxr_shutdown"));
        m_fnCreateNode = reinterpret_cast<fn_create_node_t>(req("sdxr_create_node"));
        m_fnUpdateNode = reinterpret_cast<fn_update_node_t>(req("sdxr_update_node"));
        m_fnRemoveNode = reinterpret_cast<fn_remove_node_t>(req("sdxr_remove_node"));
        m_fnSetModel = reinterpret_cast<fn_set_model_t>(req("sdxr_set_node_model"));
        m_fnSetTexture = reinterpret_cast<fn_set_texture_t>(req("sdxr_set_node_texture"));
        m_fnSetColor = reinterpret_cast<fn_set_color_t>(req("sdxr_set_node_color"));
        m_fnSetDimensions = reinterpret_cast<fn_set_dimensions_t>(req("sdxr_set_node_dimensions"));
        m_fnSetEntityType = reinterpret_cast<fn_set_entity_type_t>(req("sdxr_set_node_entity_type"));
        if (m_fnStart && m_fnPoll && m_fnCreateNode && m_fnUpdateNode) {
            m_bridgeHandle = h;
            std::cout << "[StardustBridge] Loaded Rust bridge: " << path << std::endl;
            return true;
        }
        ::dlclose(h);
    }
    return false;
}

// Legacy snippet removed after implementing new connect signature.
