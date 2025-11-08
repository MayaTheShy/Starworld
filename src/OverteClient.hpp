// OverteClient.hpp
#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

// Networking types needed for member declarations (sockaddr_storage, socklen_t)
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

struct OverteEntity {
	std::uint64_t id{0};
	std::string name;
	glm::mat4 transform{1.0f};
};

// Lightweight client for Overte mixers/entities. Designed to follow Overte's
// standards. For now includes a minimal parser scaffold; simulation can be
// optionally enabled via STARWORLD_SIMULATE=1.
class OverteClient {
public:
	explicit OverteClient(std::string domainUrl)
		: m_domainUrl(std::move(domainUrl)) {}

	// High-level connect that brings up key mixers.
	bool connect();

	// Mixer-specific stubs.
	bool connectAvatarMixer();
	bool connectEntityServer();
	bool connectAudioMixer();

	// Pump network I/O. Non-blocking.
	void poll();

	// Movement/controls
	void sendMovementInput(const glm::vec3& linearVelocity); // m/s in domain frame

	// Entity accessors
	const std::unordered_map<std::uint64_t, OverteEntity>& entities() const { return m_entities; }
	std::vector<OverteEntity> consumeUpdatedEntities();
	std::vector<std::uint64_t> consumeDeletedEntities();

private:
	void parseNetworkPackets(); // standards-aligned parsing (scaffold)

	std::string m_domainUrl;
	std::string m_host{"127.0.0.1"};
	int m_port{40102};
	bool m_connected{false};
	bool m_avatarMixer{false};
	bool m_entityServer{false};
	bool m_audioMixer{false};
	bool m_useSimulation{false};

	// Very small in-process world state for testing
	std::unordered_map<std::uint64_t, OverteEntity> m_entities;
	std::vector<std::uint64_t> m_updateQueue; // ids of entities updated since last consume
	std::vector<std::uint64_t> m_deleteQueue; // ids of entities to delete
	std::uint64_t m_nextEntityId{1};

	// Networking
	int m_udpFd{-1};
	bool m_udpReady{false};
	struct sockaddr_storage m_udpAddr{};
	socklen_t m_udpAddrLen{0};
};

