#pragma once
//
// Created by vastrakai on 11/10/2024.
//
#include <Hook/Hook.hpp>

class NametagRenderHook : public Hook
{
public:
    NametagRenderHook() : Hook()
    {
        mName = "Unknown::renderNametag";
    }
/*_QWORD *__fastcall sub_140A41910(
        __int64 a1,
        _QWORD *a2,
        __int64 a3,
        __int64 a4,
        _BYTE *a5,
        void *a6,
        __int64 a7,
        char a8,
        int a9,
        __int64 a10)*/
    static std::unique_ptr<Detour> mRenderDetour;

    static void* render(void* a1, void* a2, void* a3, void* a4, class Actor* actor, void* a6, glm::vec3* pos, bool unknown, float deltaThing, mce::Color* color);
    void init() override;
};
/*#include <Features/Modules/Misc/PacketMine.hpp>

class GameModeStopDestroyHook : public Hook {
public:
    GameModeStopDestroyHook() : Hook() {
        mName = "GameMode::stopDestroyBlock";
    }

    static std::unique_ptr<Detour> mDetour;
    static void stopDestroyBlock(GameMode* _this, const glm::ivec3& position);
    void init() override;
};
std::unique_ptr<Detour> GameModeStopDestroyHook::mDetour;

void GameModeStopDestroyHook::stopDestroyBlock(GameMode* _this, const glm::ivec3& position) {
    auto original = mDetour->getOriginal<&GameModeStopDestroyHook::stopDestroyBlock>();

    // Get PacketMine module
    auto packetMine = gFeatureManager->mModuleManager->getModule<PacketMine>();

    // If PacketMine is enabled, don't pass to original
    if (packetMine && packetMine->mEnabled) {
        return;
    }

    original(_this, position);
}

void GameModeStopDestroyHook::init() {
    uintptr_t** vtable = (uintptr_t**)SigManager::Gamemode_vtable;
    if (!vtable) return;

    // Using index 4 for stopDestroyBlock from GameMode vtable
    mDetour = std::make_unique<Detour>("stopDestroyBlock", (void*)vtable[4], &GameModeStopDestroyHook::stopDestroyBlock);
    mDetour->enable();
}















class GameModeStartDestroyHook : public Hook {
public:
    GameModeStartDestroyHook() : Hook() {
        mName = "GameMode::startDestroyBlock";
    }

    static std::unique_ptr<Detour> mDetour;
    static bool startDestroyBlock(GameMode* _this, glm::ivec3& position, int blockSide, bool& isDestroyedOut);
    void init() override;
};
std::unique_ptr<Detour> GameModeStartDestroyHook::mDetour;

bool GameModeStartDestroyHook::startDestroyBlock(GameMode* _this, glm::ivec3& position, int blockSide, bool& isDestroyedOut) {
    auto original = mDetour->getOriginal<&GameModeStartDestroyHook::startDestroyBlock>();

    // Get PacketMine module
    auto packetMine = gFeatureManager->mModuleManager->getModule<PacketMine>();

    // Call original first to get result
   
   bool result  = original(_this, position, blockSide, isDestroyedOut);
    // If successful and module enabled, handle it
    if (result && packetMine && packetMine->mEnabled) {
        packetMine->setTargetBlock(position, blockSide);
        return false;
    }
    return result;
}

void GameModeStartDestroyHook::init() {
    uintptr_t** vtable = (uintptr_t**)SigManager::Gamemode_vtable;
    if (!vtable) return;

    // Using index 1 for startDestroyBlock from GameMode vtable
    mDetour = std::make_unique<Detour>("startDestroyBlock", (void*)vtable[1], &GameModeStartDestroyHook::startDestroyBlock);
    mDetour->enable();
}*/