//
// Created by vastrakai on 7/2/2024.
//

#include "PacketReceiveHook.hpp"

#include <atomic>
#include <limits>
#include <mutex>
#include <unordered_set>

#include <magic_enum.hpp>
#include <SDK/Minecraft/Network/MinecraftPackets.hpp>
#include <Features/Events/PacketInEvent.hpp>
#include <SDK/Minecraft/ClientInstance.hpp>
#include <SDK/Minecraft/GameSession.hpp>
#include <SDK/Minecraft/MinecraftSim.hpp>
#include <Utils/MemUtils.hpp>

std::unordered_map<PacketID, Detour*> PacketReceiveHook::mDetours;
std::unordered_map<uintptr_t, Detour*> PacketReceiveHook::mDetoursByHandler;
std::vector<std::unique_ptr<Detour>> PacketReceiveHook::mOwnedDetours;

namespace {
    template <typename T>
    [[nodiscard]] bool isNullOrSentinel(const T* ptr) noexcept
    {
        if (ptr == nullptr) {
            return true;
        }

        return reinterpret_cast<uintptr_t>(ptr) == std::numeric_limits<uintptr_t>::max();
    }

    using PacketHandlerFn = decltype(&PacketReceiveHook::onPacketSend);

    std::unordered_set<uintptr_t> gFaultyPacketHandlers;
    std::mutex gFaultyPacketHandlersMutex;

    bool tryGetHandlerAddress(void* dispatcher, uintptr_t& outHandlerAddress)
    {
        outHandlerAddress = 0;
        if (isNullOrSentinel(dispatcher))
        {
            return false;
        }

        const bool readHandler = TryCallWrapper([&]() {
            outHandlerAddress = MemUtils::GetVTableFunction(dispatcher, 1);
        });
        return readHandler && outHandlerAddress != 0;
    }

    [[nodiscard]] bool isKnownFaultyHandler(const uintptr_t handlerAddress)
    {
        std::scoped_lock lock(gFaultyPacketHandlersMutex);
        return gFaultyPacketHandlers.contains(handlerAddress);
    }

    void markFaultyHandler(const uintptr_t handlerAddress)
    {
        if (handlerAddress == 0)
        {
            return;
        }

        std::scoped_lock lock(gFaultyPacketHandlersMutex);
        gFaultyPacketHandlers.insert(handlerAddress);
    }

    PacketHandlerFn resolveOriginalHandler(void* dispatcher, uintptr_t& outHandlerAddress)
    {
        outHandlerAddress = 0;
        if (!tryGetHandlerAddress(dispatcher, outHandlerAddress))
        {
            return nullptr;
        }

        if (const auto it = PacketReceiveHook::mDetoursByHandler.find(outHandlerAddress); it != PacketReceiveHook::mDetoursByHandler.end() && it->second != nullptr)
        {
            return it->second->getOriginal<&PacketReceiveHook::onPacketSend>();
        }

        return nullptr;
    }

    [[nodiscard]] bool shouldDispatchPacketEvent(void* netEventCallback)
    {
        if (netEventCallback == nullptr)
        {
            return false;
        }

        void* actualEventCallback = nullptr;
        const bool resolvedCallback = TryCallWrapper([&]() {
            auto* clientInstance = ClientInstance::get();
            if (isNullOrSentinel(clientInstance))
            {
                return;
            }

            auto* sim = clientInstance->getMinecraftSim();
            if (isNullOrSentinel(sim))
            {
                return;
            }

            auto* gameSession = sim->getGameSession();
            if (isNullOrSentinel(gameSession))
            {
                return;
            }

            actualEventCallback = gameSession->getEventCallback();
        });

        if (!resolvedCallback || actualEventCallback == nullptr)
        {
            return false;
        }

        return actualEventCallback == netEventCallback;
    }
}

void* PacketReceiveHook::onPacketSend(void* _this, void* networkIdentifier, void* netEventCallback, std::shared_ptr<Packet> packet)
{
    uintptr_t handlerAddress = 0;
    auto ofunc = resolveOriginalHandler(_this, handlerAddress);
    if (ofunc == nullptr)
    {
        static std::atomic_uint32_t missingOriginalLogCount = 0;
        const uint32_t count = missingOriginalLogCount.fetch_add(1, std::memory_order_relaxed);
        if (count < 8)
        {
            spdlog::error("[PacketReceiveHook] Missing original packet handler trampoline. Dropping packet hook callback to prevent crash.");
        }
        return nullptr;
    }

    if (isKnownFaultyHandler(handlerAddress))
    {
        return nullptr;
    }

    if (networkIdentifier != nullptr)
    {
        NetworkIdentifier = networkIdentifier;
    }

    bool cancelEvent = false;
    if (packet != nullptr && !isNullOrSentinel(packet.get()) && shouldDispatchPacketEvent(netEventCallback))
    {
        if (gFeatureManager != nullptr && gFeatureManager->mDispatcher != nullptr)
        {
            const bool triggered = TryCallWrapper([&]() {
                auto holder = nes::make_holder<PacketInEvent>(packet, networkIdentifier, netEventCallback);
                gFeatureManager->mDispatcher->trigger(holder);
                cancelEvent = holder->isCancelled();
            });

            if (!triggered)
            {
                spdlog::warn("[PacketReceiveHook] Exception while dispatching PacketInEvent. Falling back to original packet flow.");
            }
        }
    }

    if (cancelEvent)
    {
        return nullptr;
    }

    if (packet == nullptr || isNullOrSentinel(packet.get()))
    {
        return nullptr;
    }

    if (isNullOrSentinel(packet->mDispatcher))
    {
        return nullptr;
    }

    void* result = nullptr;
    const bool calledOriginal = TryCallWrapper([&]() {
        result = ofunc(_this, networkIdentifier, netEventCallback, packet);
    });
    if (!calledOriginal)
    {
        static std::atomic_uint32_t originalExceptionLogCount = 0;
        const uint32_t count = originalExceptionLogCount.fetch_add(1, std::memory_order_relaxed);
        if (count < 8)
        {
            spdlog::error("[PacketReceiveHook] Exception while calling original packet handler. Returning nullptr to avoid crash.");
        }
        markFaultyHandler(handlerAddress);
        return nullptr;
    }

    return result;
}

void PacketReceiveHook::handlePacket(std::shared_ptr<Packet> packet)
{
    if (NetworkIdentifier == nullptr || packet == nullptr || isNullOrSentinel(packet.get()) || isNullOrSentinel(packet->mDispatcher))
    {
        return;
    }

    void* eventCallback = nullptr;
    const bool resolvedCallback = TryCallWrapper([&]() {
        auto* clientInstance = ClientInstance::get();
        if (isNullOrSentinel(clientInstance))
        {
            return;
        }

        auto* sim = clientInstance->getMinecraftSim();
        if (isNullOrSentinel(sim))
        {
            return;
        }

        auto* gameSession = sim->getGameSession();
        if (isNullOrSentinel(gameSession))
        {
            return;
        }

        eventCallback = gameSession->getEventCallback();
    });

    if (!resolvedCallback || eventCallback == nullptr)
    {
        return;
    }

    onPacketSend(packet->mDispatcher, NetworkIdentifier, eventCallback, packet);
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
        if (!packet || isNullOrSentinel(packet.get()) || isNullOrSentinel(packet->mDispatcher))
        {
            continue;
        }

        uintptr_t packetFunc = 0;
        const bool readPacketFunc = TryCallWrapper([&]() {
            packetFunc = packet->mDispatcher->getPacketHandler();
        });
        if (!readPacketFunc || packetFunc == 0)
        {
            spdlog::warn("Failed to resolve packet handler for {} (0x{:X})", magic_enum::enum_name<PacketID>(id), i);
            continue;
        }

        if (const auto existing = mDetoursByHandler.find(packetFunc); existing != mDetoursByHandler.end() && existing->second != nullptr)
        {
            mDetours[id] = existing->second;
            continue;
        }

        auto detour = std::make_unique<Detour>(
            "PacketHandlerDispatcherInstance<" + std::string(magic_enum::enum_name<PacketID>(id)) + "Packet,0>::handle",
            reinterpret_cast<void*>(packetFunc),
            &onPacketSend,
            true);
        if (detour == nullptr || detour->mOriginalFunc == nullptr)
        {
            spdlog::warn("Failed to hook packet handler for {} (0x{:X})", magic_enum::enum_name<PacketID>(id), i);
            continue;
        }

        Detour* detourPtr = detour.get();
        mOwnedDetours.emplace_back(std::move(detour));
        mDetoursByHandler[packetFunc] = detourPtr;
        mDetours[id] = detourPtr;
    };

    uint64_t timeTaken = NOW - start;

    spdlog::info(
        "Successfully hooked {} unique packet handlers ({} packet ids) in {}ms",
        mOwnedDetours.size(),
        mDetours.size(),
        timeTaken);
}

void PacketReceiveHook::shutdown()
{
    for (auto& detour : mOwnedDetours)
    {
        if (detour)
        {
            detour->restore();
        }
    }

    mDetours.clear();
    mDetoursByHandler.clear();
    mOwnedDetours.clear();
    NetworkIdentifier = nullptr;
}
