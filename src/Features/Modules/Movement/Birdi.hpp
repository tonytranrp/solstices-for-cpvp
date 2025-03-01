//
// Created by vastrakai on 6/30/2024.
//
#pragma once
#include <Features/Modules/Setting.hpp>
#include <Features/FeatureManager.hpp>
#include <Features/Events/BaseTickEvent.hpp>
#include <Features/Events/PacketInEvent.hpp>
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

//
// Created by vastrakai on 6/30/2024.
//

#include <Features/Modules/Setting.hpp>

class Birdi : public ModuleBase<Birdi> {
public:

    NumberSetting SpeedX = NumberSetting("Friction", "The amount of friction to apply", 5.6f, 0.f, 20.f, 0.1f);
    NumberSetting SpeedY = NumberSetting("Friction", "The amount of friction to apply", 5.6f, 0.f, 20.f, 0.1f);


    Birdi() : ModuleBase("Birdi", "Allows you to Birdi", ModuleCategory::Movement, 0, false) {
        addSettings(
            &SpeedX,
            &SpeedY
        );

        mNames = {
            {Lowercase, "Birdi"},
            {LowercaseSpaced, "Birdi"},
            {Normal, "Birdi"},
            {NormalSpaced, "Birdi"}
        };

       // gFeatureManager->mDispatcher->listen<PacketInEvent, &Birdi::onPacketInEvent>(this);

    }
    static bool isjumping;
    bool haveelytrainarmorslor = false;
    bool goodaabb = false;
    void onEnable() override;
    void onDisable() override;
    void onPacketOutEvent(class PacketOutEvent& event) const;
    void OnGlideEvents(class ElytraGlideEvent& event);
    void OnKeyEvents(class KeyEvent& event);
    void onBaseTickEvent(class BaseTickEvent& event);
};
