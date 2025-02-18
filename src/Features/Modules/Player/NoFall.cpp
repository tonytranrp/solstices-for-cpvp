//
// Created by vastrakai on 8/30/2024.
//

#include "NoFall.hpp"
#include <Features/FeatureManager.hpp>
#include <Features/Events/BaseTickEvent.hpp>
#include <Features/Events/BobHurtEvent.hpp>
#include <Features/Events/BoneRenderEvent.hpp>
#include <Features/Events/PacketInEvent.hpp>
#include <Features/Events/PacketOutEvent.hpp>
#include <Features/Events/RenderEvent.hpp>
#include <Features/Modules/Misc/Friends.hpp>
#include <SDK/Minecraft/ClientInstance.hpp>
#include <SDK/Minecraft/Options.hpp>
#include <Utils/GameUtils/ActorUtils.hpp>
#include <SDK/Minecraft/Actor/Actor.hpp>
#include <SDK/Minecraft/Actor/GameMode.hpp>
#include <SDK/Minecraft/World/Level.hpp>
#include <SDK/Minecraft/World/HitResult.hpp>
#include <SDK/Minecraft/Inventory/PlayerInventory.hpp>
#include <SDK/Minecraft/Network/LoopbackPacketSender.hpp>
#include <SDK/Minecraft/Network/Packets/MovePlayerPacket.hpp>
#include <SDK/Minecraft/Network/Packets/PlayerAuthInputPacket.hpp>
#include <SDK/Minecraft/Network/Packets/RemoveActorPacket.hpp>
#include <SDK/Minecraft/Rendering/GuiData.hpp>
#include <Features/Events/PacketOutEvent.hpp>
#include <SDK/Minecraft/Actor/Components/FlagComponent.hpp>
#include <SDK/Minecraft/Network/Packets/MovePlayerPacket.hpp>
#include <SDK/Minecraft/Network/Packets/PlayerAuthInputPacket.hpp>

void NoFall::onEnable()
{
    gFeatureManager->mDispatcher->listen<PacketOutEvent, &NoFall::onPacketOutEvent>(this);
}

void NoFall::onDisable()
{
    gFeatureManager->mDispatcher->deafen<PacketOutEvent, &NoFall::onPacketOutEvent>(this);
}

void NoFall::onPacketOutEvent(PacketOutEvent& event)
{
    if (event.mPacket->getId() == PacketID::PlayerAuthInput)
    {
        auto player = ClientInstance::get()->getLocalPlayer();
        if (!player) return;

        auto paip = event.getPacket<PlayerAuthInputPacket>();
        if (mMode.mValue == Mode::Sentinel) {
            static bool alt = false;
            alt = !alt;
            if (alt) return;
            paip->mPosDelta.y = -0.0784000015258789f;
        }
        else if (mMode.mValue == Mode::BDS) {
            paip->mInputData |= AuthInputAction::START_GLIDING;
            paip->mInputData &= ~AuthInputAction::STOP_GLIDING;
            paip->mPosDelta = { 0, 0, 0 };
        } else if (mMode.mValue == Mode::Lifeboat) {
            if (!player->getFlag<SetMovingFlagRequestComponent>()) return;

            static bool alt = false;
            alt = !alt;

            static float lastY = 0;
            if (alt) lastY = paip->mPos.y;
            else
            {
                paip->mPos.y = lastY + 0.1f;
                paip->mPosDelta = { 0, 0, 0 };
                paip->removeMovingInput();
            }
        }
    }

    if (event.mPacket->getId() == PacketID::MovePlayer)
    {
        auto mp = event.getPacket<MovePlayerPacket>();
        if (mMode.mValue == Mode::OnGround) mp->mOnGround = true;
    }
}
