#pragma once
#include "Event.hpp"
#include <glm/vec3.hpp>
struct SprintEvent : public CancelableEvent
{
	bool Appliedflags;// Controls movement vector (X, Y, Z)

	explicit SprintEvent(bool flags)
		: Appliedflags(flags) {
	}
};
