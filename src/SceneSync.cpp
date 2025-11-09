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
			
			// Set visual properties for the newly created node
			stardust.setNodeEntityType(nodeId, static_cast<uint8_t>(e.type));
			stardust.setNodeColor(nodeId, e.color, e.alpha);
			stardust.setNodeDimensions(nodeId, e.dimensions);
			
			if (!e.modelUrl.empty()) {
				stardust.setNodeModel(nodeId, e.modelUrl);
			}
			if (!e.textureUrl.empty()) {
				stardust.setNodeTexture(nodeId, e.textureUrl);
			}
		} else {
			// Update existing node's transform and visual properties
			stardust.updateNodeTransform(it->second, e.transform);
			stardust.setNodeEntityType(it->second, static_cast<uint8_t>(e.type));
			stardust.setNodeColor(it->second, e.color, e.alpha);
			stardust.setNodeDimensions(it->second, e.dimensions);
			
			if (!e.modelUrl.empty()) {
				stardust.setNodeModel(it->second, e.modelUrl);
			}
			if (!e.textureUrl.empty()) {
				stardust.setNodeTexture(it->second, e.textureUrl);
			}
		}
	}

	// Process deletions after updates to avoid create-then-delete thrash.
	auto deleted = overte.consumeDeletedEntities();
	for (auto entId : deleted) {
		auto it = s_entityNodeMap.find(entId);
		if (it != s_entityNodeMap.end()) {
			stardust.removeNode(it->second);
			s_entityNodeMap.erase(it);
		}
	}
}

