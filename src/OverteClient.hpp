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

// Overte entity types (matching Overte EntityTypes.h)
enum class EntityType {
	Unknown,
	Box,
	Sphere,
	Model,
	Shape,
	Light,
	Text,
	Zone,
	Web,
	ParticleEffect,
	Line,
	PolyLine,
	Grid,
	Gizmo,
	Material
};

struct OverteEntity {
	std::uint64_t id{0};
	std::string name;
	glm::mat4 transform{1.0f};
	
	// Visual properties
	EntityType type{EntityType::Box};
	std::string modelUrl;      // For Model type entities
	std::string textureUrl;    // Texture/material URL
	glm::vec3 color{1.0f, 1.0f, 1.0f};  // RGB color (0-1 range)
	glm::vec3 dimensions{0.1f, 0.1f, 0.1f};  // Size/scale in meters
	float alpha{1.0f};         // Transparency (0-1)
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
	void parseEntityPacket(const char* data, size_t len);
	void parseDomainPacket(const char* data, size_t len);
	void handleDomainListReply(const char* data, size_t len);
	void handleDomainConnectionDenied(const char* data, size_t len);
	void sendDomainListRequest();
	void sendDomainConnectRequest();
	void sendEntityQuery();
	void sendPing(int fd, const sockaddr_storage& addr, socklen_t addrLen);

	std::string m_domainUrl;
	std::string m_host{"127.0.0.1"};
	int m_port{40102};
	bool m_connected{false};
	bool m_avatarMixer{false};
	bool m_entityServer{false};
	bool m_audioMixer{false};
	bool m_useSimulation{false};
	bool m_domainConnected{false};
	std::string m_sessionUUID; // Our client session UUID
	std::string m_username;     // Domain account username (for future signature-based auth)
	std::uint32_t m_sequenceNumber{0};  // Packet sequence number for NLPacket protocol
	std::uint16_t m_localID{0};         // Local ID assigned by domain server

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
	
	// EntityServer connection
	int m_entityFd{-1};
	bool m_entityServerReady{false};
	sockaddr_storage m_entityAddr{};
	socklen_t m_entityAddrLen{0};
	std::vector<char> m_entityBuffer; // accumulate partial packets
};

