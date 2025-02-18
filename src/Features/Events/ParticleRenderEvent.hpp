#pragma once

#include "Event.hpp"

struct ParticleRenderEvent : public CancelableEvent {
    ParticleRenderEvent() = default;
};
