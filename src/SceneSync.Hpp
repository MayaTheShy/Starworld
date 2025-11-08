// SceneSync.hpp (note: file kept as provided name)
#pragma once

#include <cstdint>
#include <unordered_map>

#include <glm/glm.hpp>

class StardustBridge;
class OverteClient;

// Synchronizes Overte entities into the Stardust subscene.
class SceneSync {
public:
	static void update(StardustBridge& stardust, OverteClient& overte);

private:
	// Map Overte entity id -> Stardust node id
	static std::unordered_map<std::uint64_t, std::uint64_t> s_entityNodeMap;
};

