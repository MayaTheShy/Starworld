// InputHandler.hpp
#pragma once

class StardustBridge;
class OverteClient;

// Reads input from Stardust and forwards movement to Overte.
class InputHandler {
public:
	InputHandler(StardustBridge& stardust, OverteClient& overte)
		: m_stardust(stardust), m_overte(overte) {}

	// dt in seconds
	void update(float dt);

private:
	StardustBridge& m_stardust;
	OverteClient& m_overte;
	float m_moveSpeed{1.5f}; // meters per second at full deflection
	float m_deadZone{0.15f};
};

