// BoatControlEvent.hpp

#pragma once
#include "Event.hpp"
#include <glm/vec3.hpp>

struct BoatControlEvent : public CancelableEvent {
    glm::vec3 mVelocity; // The boat's movement vector (X, Y, Z).

    explicit BoatControlEvent(const glm::vec3& velocity)
        : mVelocity(velocity) {
    }
};
