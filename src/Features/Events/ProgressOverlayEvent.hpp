#pragma once

#include "Event.hpp"

#include <string>

class ProgressOverlayEvent : public Event
{
public:
    enum class Action
    {
        Open,
        Update,
        Close
    };

    Action mAction = Action::Update;
    std::string mOwnerId;
    std::string mTitle;
    std::string mStatus;
    float mProgress = 0.f;

    ProgressOverlayEvent(
        const Action action,
        std::string ownerId,
        std::string title = {},
        std::string status = {},
        const float progress = 0.f)
        : mAction(action),
          mOwnerId(std::move(ownerId)),
          mTitle(std::move(title)),
          mStatus(std::move(status)),
          mProgress(progress)
    {
    }
};
