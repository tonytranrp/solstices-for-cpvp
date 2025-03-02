#pragma once
#include "Event.hpp"
#include <glm/vec3.hpp>
struct FOVEvents : public CancelableEvent
{
	float FOV;// Controls movement vector (X, Y, Z)

	explicit FOVEvents(float FOVs)
		: FOV(FOVs) {
	}
};
