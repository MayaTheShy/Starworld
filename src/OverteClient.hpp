// OverteClient.hpp
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

// Networking types needed for member declarations (sockaddr_storage, socklen_t)
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

// Forward declarations
class OverteAuth;

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

// Assignment client information from DomainList
struct AssignmentClient {
	uint8_t type;           // 0=EntityServer, 1=AudioMixer, 2=AvatarMixer, etc.
	std::array<uint8_t, 16> uuid;
	sockaddr_storage address{};
	socklen_t addressLen{0};
	uint16_t port{0};
};

// Lightweight client for Overte mixers/entities. Designed to follow Overte's
// standards. For now includes a minimal parser scaffold; simulation can be
// optionally enabled via STARWORLD_SIMULATE=1.
class OverteClient {
public:
	explicit OverteClient(std::string domainUrl);
	~OverteClient();

	// Authentication
	bool login(const std::string& username, const std::string& password,
	           const std::string& metaverseUrl = "https://mv.overte.org");
	bool isAuthenticated() const;
	void setAuth(OverteAuth* auth); // Set metaverse authentication
	
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
	
	// Entity creation
	void createEntity(const std::string& name, EntityType type, const glm::vec3& position, 
	                  const glm::vec3& dimensions, const glm::vec3& color);

private:
	void parseNetworkPackets(); // standards-aligned parsing (scaffold)
	void parseEntityPacket(const char* data, size_t len);
	void parseDomainPacket(const char* data, size_t len);
	void handleDomainListReply(const char* data, size_t len);
	void handleDomainConnectionDenied(const char* data, size_t len);
	void handleICEPing(const char* data, size_t len);
	void handlePing(const char* payload, size_t len);
	void sendDomainListRequest();
	void sendDomainConnectRequest();
	void sendEntityQuery();
	void sendPing(int fd, const sockaddr_storage& addr, socklen_t addrLen);
	void sendACK(uint32_t sequenceNumber);
	
	// Avatar Mixer protocol
	void sendAvatarIdentity();
	void sendAvatarData();
	void sendAvatarQuery();
	void handleAvatarMixerPacket(const char* data, size_t len, uint8_t packetType);

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
	
	// Authentication (non-owning pointer to auth object from main)
	OverteAuth* m_auth{nullptr};

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
	
	// Assignment clients from DomainList
	std::vector<AssignmentClient> m_assignmentClients;
	sockaddr_storage m_entityServerAddr{};
	socklen_t m_entityServerAddrLen{0};
	uint16_t m_entityServerPort{0};
	
	// Avatar Mixer connection
	sockaddr_storage m_avatarMixerAddr{};
	socklen_t m_avatarMixerAddrLen{0};
	uint16_t m_avatarMixerPort{0};
	bool m_avatarMixerConnected{false};
	
	// Avatar state
	glm::vec3 m_avatarPosition{0.0f, 0.0f, 0.0f};
	glm::quat m_avatarOrientation{1.0f, 0.0f, 0.0f, 0.0f};  // Identity quaternion
	std::uint16_t m_avatarDataSequence{0};
	std::uint16_t m_avatarIdentitySequence{0};
	bool m_identitySent{false};
	
	// EntityServer connection
	int m_entityFd{-1};
	bool m_entityServerReady{false};
	sockaddr_storage m_entityAddr{};
	socklen_t m_entityAddrLen{0};
	std::vector<char> m_entityBuffer; // accumulate partial packets
};

