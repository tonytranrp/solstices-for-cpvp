//
// Created by vastrakai on 6/25/2024.
//

#include "HookManager.hpp"

#include <MinHook.h>

#include "Hooks/ActorHooks/ActorModelHook.hpp"
#include "Hooks/ActorHooks/GetFovHook.hpp"
#include "Hooks/ActorHooks/ActorGlideHook.hpp"
#include "Hooks/ActorHooks/AnimationHooks.hpp"
#include "Hooks/ActorHooks/NametagRenderHook.hpp"
#include "Hooks/ActorHooks/GameModeStopDestroyHook.hpp"
#include "Hooks/ActorHooks/GameModeStartDestroyHook.hpp"
#include "Hooks/ContainerHooks/ContainerScreenControllerHook.hpp"
#include "Hooks/MiscHooks/FireBlockChangedHook.hpp"
#include "Hooks/MiscHooks/KeyHook.hpp"
#include "Hooks/MiscHooks/BoatControlHook.hpp"
#include "Hooks/MiscHooks/MouseHook.hpp"
#include "Hooks/MiscHooks/PreGameHook.hpp"
#include "Hooks/NetworkHooks/ConnectionRequestHook.hpp"
#include "Hooks/NetworkHooks/PacketReceiveHook.hpp"
#include "Hooks/NetworkHooks/PacketSendHook.hpp"
#include "Hooks/NetworkHooks/RakPeerHooks.hpp"
#include "Hooks/RenderHooks/ActorRenderDispatcherHook.hpp"
#include "Hooks/RenderHooks/D3DHook.hpp"
#include "Hooks/RenderHooks/HoverTextRendererHook.hpp"
#include "Hooks/RenderHooks/ItemRendererHook.hpp"
#include "Hooks/RenderHooks/LookInputHook.hpp"
#include "Hooks/RenderHooks/NoParticleRenderHook.hpp"
#include "Hooks/RenderHooks/RenderItemInHandHook.hpp"
#include "Hooks/RenderHooks/SetupAndRenderHook.hpp"



#define ADD_HOOK(hook) hooks.emplace_back(std::make_shared<hook>())

void HookManager::init(bool initLp)
{
    if (initLp)
    {
        std::vector<std::shared_ptr<Hook>> hooks;
        ADD_HOOK(BaseTickHook);
        for (auto& hook : hooks)
        {
            hook->init();
            mHooks.emplace_back(hook);
        }
    }
    else
    {
        std::vector<std::shared_ptr<Hook>> hooks;
        ADD_HOOK(KeyHook);
        ADD_HOOK(SetupAndRenderHook);
        ADD_HOOK(D3DHook);
        ADD_HOOK(ContainerScreenControllerHook);
        ADD_HOOK(MouseHook);
        ADD_HOOK(ConnectionRequestHook);
        ADD_HOOK(PacketReceiveHook);
        ADD_HOOK(PacketSendHook);
        ADD_HOOK(RakPeerHooks);
      // ADD_HOOK(NoParticleRenderHook);
        ADD_HOOK(ActorRenderDispatcherHook);
        ADD_HOOK(AnimationHooks);
        ADD_HOOK(HoverTextRendererHook);
        ADD_HOOK(LookInputHook);
        ADD_HOOK(ActorModelHook);
        ADD_HOOK(ItemRendererHook);
        ADD_HOOK(FireBlockChangedHook);
        ADD_HOOK(RenderItemInHandHook);
        ADD_HOOK(PreGameHook);
        ADD_HOOK(NametagRenderHook);
        ADD_HOOK(ActorGlideHook);
        ADD_HOOK(GetFovHook);
       // ADD_HOOK(BoatControlHook); dont works idk why breh
       // ADD_HOOK(GameModeStartDestroyHook);  // Add these
       // ADD_HOOK(GameModeStopDestroyHook);   // two hooks


        for (auto& hook : hooks)
        {
            hook->init();
            mHooks.emplace_back(hook);
        }

        MH_EnableHook(MH_ALL_HOOKS);
    }
}

void HookManager::shutdown()
{
    for (auto& hook : mHooks)
    {
        hook->shutdown();
    }

    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();

    mHooks.clear();
}

