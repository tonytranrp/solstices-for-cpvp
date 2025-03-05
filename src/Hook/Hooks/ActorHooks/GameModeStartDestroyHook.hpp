// GameModeStartDestroyHook.hpp
#pragma once
#include <Hook/Hook.hpp>
class GameModeStartDestroyHook : public Hook {
public:
    GameModeStartDestroyHook() : Hook() {
        mName = "GameMode::startDestroyBlock";
    }

    static std::unique_ptr<Detour> mDetour;
    static bool startDestroyBlock(GameMode* _this, glm::ivec3& position, int blockSide, bool& isDestroyedOut) {
        auto original = mDetour->getOriginal<&GameModeStartDestroyHook::startDestroyBlock>();

        // Get PacketMine module
        auto packetMine = gFeatureManager->mModuleManager->getModule<PacketMine>();

        // Call original first to get result

        bool result = original(_this, position, blockSide, isDestroyedOut);
        // If successful and module enabled, handle it
        if (result && packetMine && packetMine->mEnabled) {
            packetMine->setTargetBlock(position, blockSide);
            return false;
        }
        return result;
    }
    void init() {
        uintptr_t** vtable = (uintptr_t**)SigManager::Gamemode_vtable;
        if (!vtable) return;

        // Using index 1 for startDestroyBlock from GameMode vtable
        mDetour = std::make_unique<Detour>("startDestroyBlock", (void*)vtable[1], &GameModeStartDestroyHook::startDestroyBlock);
        mDetour->enable();
    };
};
std::unique_ptr<Detour> GameModeStartDestroyHook::mDetour;