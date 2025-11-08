// OverteClient.hpp
#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

struct OverteEntity {
	std::uint64_t id{0};
	std::string name;
	glm::mat4 transform{1.0f};
};

// Lightweight stub over Overte networking layer. Designed to be replaced by
// real Overte SDK calls while keeping the app testable.
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

private:
	std::string m_domainUrl;
	bool m_connected{false};
	bool m_avatarMixer{false};
	bool m_entityServer{false};
	bool m_audioMixer{false};

	// Very small in-process world state for testing
	std::unordered_map<std::uint64_t, OverteEntity> m_entities;
	std::vector<std::uint64_t> m_updateQueue; // ids of entities updated since last consume
	std::uint64_t m_nextEntityId{1};
};

