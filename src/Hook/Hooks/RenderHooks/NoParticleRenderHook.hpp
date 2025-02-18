#pragma once
//
// Created by vastrakai on 7/8/2024.
//

#include <Hook/Hook.hpp>
#include <Hook/HookManager.hpp>


class ActorRenderDispatcher;
class BaseActorRenderContext;
class Actor;


class NoParticleRenderHook : public Hook {
public:
    NoParticleRenderHook() : Hook() {
        mName = "LevelRenderer::renderParticles";
    }

    static void renderParticles(void* a1, void* a2);
    void init() override;

    // 🔹 Only declare `mDetour`, don't define it here!
    static std::unique_ptr<Detour> mDetour;
};






