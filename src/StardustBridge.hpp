// StardustBridge.hpp
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <functional>

#include <glm/glm.hpp>

// A lightweight bridge to the StardustXR compositor.
// Assumes a C API is available at runtime; this implementation provides a
// minimal in-process fallback so the app remains testable without the shared lib.
class StardustBridge {
public:
	using NodeId = std::uint64_t;

	// Connect to the StardustXR compositor via IPC.
	// Returns true on success. If socketPath is empty, uses defaultSocketPath().
	bool connect(const std::string& socketPath = {});

	// Create a 3D node with an initial transform. Optionally parent it.
	NodeId createNode(const std::string& name,
					  const glm::mat4& transform = glm::mat4(1.0f),
					  std::optional<NodeId> parent = std::nullopt);

	// Update a node's transform. Returns false if the node doesn't exist.
	bool updateNodeTransform(NodeId id, const glm::mat4& transform);

	// Remove a node. Returns false if the node doesn't exist.
	bool removeNode(NodeId id);

	// Poll compositor events and input. Non-blocking.
	void poll();

	// Lifecycle helpers for the main loop.
	bool running() const { return m_running; }
	void requestQuit() { m_running = false; }

	// Input snapshot (polled each frame via poll()).
	glm::vec2 joystick() const { return m_joystick; }   // x,y in [-1, 1]
	glm::mat4 headPose() const { return m_headPose; }   // world-from-head

	// Utility: compute default IPC socket path (first-best guess).
	static std::string defaultSocketPath();

	~StardustBridge();
	// Explicit cleanup
	void close();

private:
	struct Node {
		std::string name;
		std::optional<NodeId> parent;
		glm::mat4 transform{1.0f};
	};

	// Fallback in-process scene representation for testing without the runtime.
	std::unordered_map<NodeId, Node> m_nodes;
	NodeId m_nextId{1};

	// Connection and state
	bool m_connected{false};
	bool m_running{true};
	std::string m_socketPath;
	int m_socketFd{-1};

	// Input state
	glm::vec2 m_joystick{0.0f, 0.0f};
	glm::mat4 m_headPose{1.0f};

	// Optional root for the Overte world subscene
	std::optional<NodeId> m_overteRoot;

	// Dynamic Rust bridge (dlopen) function pointers
	void* m_bridgeHandle{nullptr};
	using fn_start_t = int(*)(const char*);
	using fn_poll_t = int(*)();
	using fn_shutdown_t = void(*)();
	using fn_create_node_t = std::uint64_t(*)(const char*, const float*);
	using fn_update_node_t = int(*)(std::uint64_t, const float*);
	using fn_remove_node_t = int(*)(std::uint64_t);
	fn_start_t m_fnStart{nullptr};
	fn_poll_t m_fnPoll{nullptr};
	fn_shutdown_t m_fnShutdown{nullptr};
	fn_create_node_t m_fnCreateNode{nullptr};
	fn_update_node_t m_fnUpdateNode{nullptr};
	fn_remove_node_t m_fnRemoveNode{nullptr};

	bool loadBridge();
};

