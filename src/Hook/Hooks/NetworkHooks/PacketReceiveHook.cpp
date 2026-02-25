//
// Created by vastrakai on 7/2/2024.
//

#include "PacketReceiveHook.hpp"

#include <magic_enum.hpp>
#include <SDK/Minecraft/Network/MinecraftPackets.hpp>
#include <Features/Events/PacketInEvent.hpp>
#include <SDK/Minecraft/ClientInstance.hpp>
#include <SDK/Minecraft/GameSession.hpp>
#include <SDK/Minecraft/MinecraftSim.hpp>
#include <Utils/MiscUtils/ColorUtils.hpp>
#include <omp.h>

std::unordered_map<PacketID, Detour*> PacketReceiveHook::mDetours;
std::unordered_map<uintptr_t, Detour*> PacketReceiveHook::mDetoursByHandler;
std::vector<std::unique_ptr<Detour>> PacketReceiveHook::mOwnedDetours;

void PacketReceiveHook::onPacketSend(void* _this, void* networkIdentifier, void* netEventCallback, std::shared_ptr<Packet> packet)
{
    if (!_this || !packet) return;

    auto* dispatcher = reinterpret_cast<PacketHandlerDispatcher*>(_this);
    if (!dispatcher) return;

    Detour* detour = nullptr;
    const uintptr_t handler = dispatcher->getPacketHandler();
    if (auto it = mDetoursByHandler.find(handler); it != mDetoursByHandler.end())
        detour = it->second;
    else if (auto it = mDetours.find(packet->getId()); it != mDetours.end())
        detour = it->second;

    if (!detour) return;

    auto ofunc = detour->getOriginal<&PacketReceiveHook::onPacketSend>();
    if (!ofunc) return;

    NetworkIdentifier.store(networkIdentifier, std::memory_order_relaxed);

    auto* clientInstance = ClientInstance::get();
    if (!clientInstance || !clientInstance->getLocalPlayer())
    {
        ofunc(_this, networkIdentifier, netEventCallback, packet);
        return;
    }

    auto* sim = clientInstance->getMinecraftSim();
    auto* gameSession = sim ? sim->getGameSession() : nullptr;
    auto* actualEventCallback = gameSession ? gameSession->getEventCallback() : nullptr;
    if (actualEventCallback != netEventCallback) {
        ofunc(_this, networkIdentifier, netEventCallback, packet);
        return;
    }

    if (!gFeatureManager || !gFeatureManager->mDispatcher)
    {
        ofunc(_this, networkIdentifier, netEventCallback, packet);
        return;
    }

    try
    {
        auto holder = nes::make_holder<PacketInEvent>(packet, networkIdentifier, netEventCallback);
        gFeatureManager->mDispatcher->trigger(holder);
        if (holder->isCancelled()) return;
    }
    catch (const std::exception& e)
    {
        spdlog::error("PacketReceiveHook::onPacketSend exception for packet {}: {}", static_cast<int>(packet->getId()), e.what());
    }
    catch (...)
    {
        spdlog::error("PacketReceiveHook::onPacketSend unknown exception for packet {}", static_cast<int>(packet->getId()));
    }

    ofunc(_this, networkIdentifier, netEventCallback, packet);
}

void PacketReceiveHook::handlePacket(std::shared_ptr<Packet> packet)
{
    auto* networkIdentifier = NetworkIdentifier.load(std::memory_order_relaxed);
    auto* clientInstance = ClientInstance::get();
    auto* sim = clientInstance ? clientInstance->getMinecraftSim() : nullptr;
    auto* gameSession = sim ? sim->getGameSession() : nullptr;
    auto* eventCallback = gameSession ? gameSession->getEventCallback() : nullptr;

    if (!packet || !packet->mDispatcher || !networkIdentifier || !eventCallback) return;
    onPacketSend(packet->mDispatcher, networkIdentifier, eventCallback, packet);
}

void PacketReceiveHook::init()
{
    static bool called = false;
    if (called) return;
    called = true;

    auto packetIds = magic_enum::enum_values<PacketID>();

    uint64_t start = NOW;
    spdlog::info("Hooking {} packets", packetIds.size());

    mDetours.clear();
    mDetoursByHandler.clear();
    mOwnedDetours.clear();

    for (int i = 0; i < 0x136; i++) { // Fuck magic enum
        auto id = static_cast<PacketID>(i);
        auto packet = MinecraftPackets::createPacket(static_cast<PacketID>(i));
        if (!packet || !packet->mDispatcher) continue;

        auto packetFunc = packet->mDispatcher->getPacketHandler();
        if (!packetFunc) {
            spdlog::warn("Failed to hook packet: {} (0x{:X})", magic_enum::enum_name<PacketID>(id), i);
            continue;
        }

        if (auto existing = mDetoursByHandler.find(packetFunc); existing != mDetoursByHandler.end())
        {
            mDetours[id] = existing->second;
            continue;
        }

        auto detour = std::make_unique<Detour>(
            "PacketHandlerDispatcherInstance<" + std::string(magic_enum::enum_name<PacketID>(id)) + "Packet,0>::handle",
            reinterpret_cast<void*>(packetFunc),
            reinterpret_cast<void*>(&onPacketSend),
            true
        );

        if (!detour->mOriginalFunc)
        {
            spdlog::warn("Failed to create packet detour for id {} (0x{:X})", magic_enum::enum_name<PacketID>(id), i);
            continue;
        }

        Detour* rawDetour = detour.get();
        mOwnedDetours.emplace_back(std::move(detour));
        mDetoursByHandler[packetFunc] = rawDetour;
        mDetours[id] = rawDetour;
    };

    uint64_t timeTaken = NOW - start;

    spdlog::info("Successfully hooked {} packet handlers ({} packet ids) in {}ms", mOwnedDetours.size(), mDetours.size(), timeTaken);
}
