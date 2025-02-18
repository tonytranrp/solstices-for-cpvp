#pragma once
#include <string>
#include <Utils/MemUtils.hpp>


//
// Created by vastrakai on 6/25/2024.
//

class GuiData {
public:
    // unlikely to change
    CLASS_FIELD(glm::vec2, mResolution, 0x30);
    CLASS_FIELD(glm::vec2, mResolutionRounded, 0x38);
    CLASS_FIELD(glm::vec2, mResolutionScaled, 0x40);

    CLASS_FIELD(float, mGuiScale, 0x4C);
    CLASS_FIELD(float, mScalingMultiplier, 0x50);

    void displayClientMessageQueued(const std::string& msg);
    void displayClientMessage(const std::string& msg);
};