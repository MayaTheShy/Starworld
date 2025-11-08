#include "InputHandler.hpp"

#include "OverteClient.hpp"
#include "StardustBridge.hpp"

#include <algorithm>
#include <glm/glm.hpp>

void InputHandler::update(float /*dt*/) {
	auto js = m_stardust.joystick();
	// Apply radial dead zone.
	float mag = glm::length(js);
	if (mag < m_deadZone) {
		js = {0.0f, 0.0f};
	} else if (mag > 1.0f) {
		js /= mag; // clamp
	}

	glm::vec3 vel{js.x * m_moveSpeed, 0.0f, js.y * m_moveSpeed};
	m_overte.sendMovementInput(vel);
}

