#pragma once

#include <Features/Modules/Module.hpp>
#include <vector>
#include <string>
#include <glm/glm.hpp>

// AutoAnchor module: if no respawn anchor is above the player, switch to the anchor slot and place one.
// If a respawn anchor is present above the player's head, switch to glowstone and place one glowstone block (to charge/explode it),
// then switch back to the respawn anchor slot.
class AutoAnchor : public ModuleBase<AutoAnchor> {
public:
    struct PlacePosition {
        BlockPos pos;
        PlacePosition(const BlockPos& p) : pos(p) {}
    };

    AutoAnchor();

    void onEnable() override;
    void onDisable() override;
    void onBaseTickEvent(class BaseTickEvent& event);

private:
    // Settings
    NumberSetting mPlaceDelay = NumberSetting("PlaceDelay", "Delay before placing anchor (ms)", 500, 0, 2000, 10);
    NumberSetting mGlowDelay = NumberSetting("GlowDelay", "Delay before placing glowstone (ms)", 300, 0, 2000, 10);
    NumberSetting mPlaceRange = NumberSetting("PlaceRange", "Placement range (blocks)", 5.0f, 1.0f, 10.0f, 0.1f);

    // Candidate placement positions (here we use only one: directly above the player's head)
    std::vector<PlacePosition> mPossiblePlacements;

    // Timing variables (NOW must return current time in ms)
    uint64_t mLastPlace = 0;
    uint64_t mLastGlow = 0;

    // Inventory switching state (hotbar slots 0–8)
    int mAnchorSlot = -1;   // Slot for respawn anchor (detected by checking if mName contains "respawn_anchor")
    int mGlowSlot = -1;     // Slot for glowstone (detected by checking if mName contains "glowstone")
    int mPrevSlot = -1;     // Previously selected slot
    bool mHasSwitched = false;

    // Helper functions
    bool canPlaceAnchor(const BlockPos& pos);
    std::vector<PlacePosition> findPlacePositions();
    void placeAnchor(const BlockPos& pos);
    void placeGlowstone(const BlockPos& pos);

    // Inventory switching functions (real switching)
    bool switchToAnchor();
    bool switchToGlowstone();
    void switchBack();
};
