#pragma once
#include <Features/Modules/Setting.hpp>
#include <Features/FeatureManager.hpp>
#include <Features/Events/BaseTickEvent.hpp>
#include <Features/Events/PacketOutEvent.hpp>
#include <Features/Events/ElytraGlideEvent.hpp>
#include <Features/Events/KeyEvent.hpp>
#include <SDK/Minecraft/ClientInstance.hpp>
#include <SDK/Minecraft/MinecraftSim.hpp>
#include <SDK/Minecraft/Actor/Actor.hpp>
#include <SDK/Minecraft/Actor/Components/StateVectorComponent.hpp>
#include <SDK/Minecraft/Actor/Components/ActorRotationComponent.hpp>
#include <SDK/Minecraft/Network/Packets/PlayerAuthInputPacket.hpp>
#include <SDK/Minecraft/Network/Packets/SetActorMotionPacket.hpp>
#include <SDK/Minecraft/Network/Packets/Packet.hpp>

class PlayerAuthInputPacket;

// Module class for Elytra flight (Birdi)
class Birdi : public ModuleBase<Birdi> {
public:
    NumberSetting SpeedX = NumberSetting(
        "Horizontal Speed",
        "Horizontal Elytra flight speed",
        5.6f, 0.f, 20.f, 0.1f
    );
    NumberSetting SpeedY = NumberSetting(
        "Vertical Speed",
        "Vertical Elytra flight speed",
        5.6f, 0.f, 20.f, 0.1f
    );
    NumberSetting heightThreshold = NumberSetting(
        "Height Threshold",
        "AABB height threshold for Elytra activation",
        1.8f, 0.f, 3.f, 0.05f
    );
    BoolSetting useAcceleration = BoolSetting{
        "Acceleration", "Enable gradual flight acceleration", false
    };
    NumberSetting accelerationFactor = NumberSetting(
        "Acceleration Factor",
        "Acceleration increment (%) per tick",
        10.f, 1.f, 100.f, 1.f
    );

    // NEW: Minimum fall distance required before auto-start gliding.
    //      Adjust as needed.
    NumberSetting fallDistanceThreshold = NumberSetting(
        "Fall Distance Threshold",
        "Minimum fall distance to trigger Elytra flight",
        0.25f, 0.f, 10.f, 0.05f
    );

    Birdi()
        : ModuleBase("Birdi", "Allows true Elytra flight", ModuleCategory::Movement, 0, false)
    {
        addSettings(
            &SpeedX,
            &SpeedY,
            &heightThreshold,
            &useAcceleration,
            &accelerationFactor,
            &fallDistanceThreshold
        );
        mNames = {
            {Lowercase,       "Birdi"},
            {LowercaseSpaced, "Birdi"},
            {Normal,          "Birdi"},
            {NormalSpaced,    "Birdi"}
        };
    }

    // Internal state flags
    bool  isSpacePressed = false;
    bool  haveElytraInArmorSlot = false;
    bool  glidingActive = false;
    float accelProgress = 0.0f;
    bool accelerating = false; // Track if acceleration phase is active
    float accelerationTimer = 0.0f; // Timer for acceleration delay
    void onEnable() override;
    void onDisable() override;
    void onPacketOutEvent(class PacketOutEvent& event) const;
    void OnGlideEvents(class ElytraGlideEvent& event);
    void OnKeyEvents(class KeyEvent& event);
    void onBaseTickEvent(class BaseTickEvent& event);
};
