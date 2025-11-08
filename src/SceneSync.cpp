#include "SceneSync.Hpp"

#include "OverteClient.hpp"
#include "StardustBridge.hpp"

#include <glm/gtc/matrix_transform.hpp>

std::unordered_map<std::uint64_t, std::uint64_t> SceneSync::s_entityNodeMap;

void SceneSync::update(StardustBridge& stardust, OverteClient& overte) {
	// Pull only the entities that changed since the last call.
	auto updated = overte.consumeUpdatedEntities();
	for (const auto& e : updated) {
		auto it = s_entityNodeMap.find(e.id);
		if (it == s_entityNodeMap.end()) {
			// Create a Stardust node the first time we see this entity.
			auto nodeId = stardust.createNode(e.name, e.transform);
			s_entityNodeMap.emplace(e.id, nodeId);
		} else {
			// Update existing node's transform.
			stardust.updateNodeTransform(it->second, e.transform);
		}
	}
}

