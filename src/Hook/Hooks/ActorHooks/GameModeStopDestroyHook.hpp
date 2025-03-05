// GameModeStopDestroyHook.hpp
#pragma once
#include <Hook/Hook.hpp>
#include <Features/Modules/Misc/PacketMine.hpp>

class GameModeStopDestroyHook : public Hook {
public:
    GameModeStopDestroyHook() : Hook() {
        mName = "GameMode::stopDestroyBlock";
    }

    static std::unique_ptr<Detour> mDetour;
    static void stopDestroyBlock(GameMode* _this, const glm::ivec3& position) {
        auto original = mDetour->getOriginal<&GameModeStopDestroyHook::stopDestroyBlock>();

        // Get PacketMine module
        auto packetMine = gFeatureManager->mModuleManager->getModule<PacketMine>();

        // If PacketMine is enabled and we are NOT forcing a break, cancel the stopDestroyBlock call.
        if (packetMine && packetMine->mEnabled && !packetMine->shouldForceBreak()) {
            return;
        }

        original(_this, position);
    }
    void init() {
        uintptr_t** vtable = (uintptr_t**)SigManager::Gamemode_vtable;
        if (!vtable) return;

        // Using index 4 for stopDestroyBlock from GameMode vtable
        mDetour = std::make_unique<Detour>("stopDestroyBlock", (void*)vtable[4], &GameModeStopDestroyHook::stopDestroyBlock);
        mDetour->enable();
    }
};
std::unique_ptr<Detour> GameModeStopDestroyHook::mDetour;
