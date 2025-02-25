#pragma once
#include "Event.hpp"
#include <glm/vec3.hpp>
struct ElytraGlideEvent : public CancelableEvent 
{
	glm::vec3 mVelocity;// Controls movement vector (X, Y, Z)

	explicit ElytraGlideEvent(const glm::vec3& velocity)
		: mVelocity(velocity) {
	}
};
