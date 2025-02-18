#include "NoParticleRenderHook.hpp"

#include <Features/Events/ParticleRenderEvent.hpp>
#include <SDK/Minecraft/ClientInstance.hpp>
#include <Hook/Detour.hpp>

// ✅ Correctly define `mDetour` (Only in .cpp)
std::unique_ptr<Detour> NoParticleRenderHook::mDetour;

void NoParticleRenderHook::renderParticles(void* a1, void* a2) {
    auto oFunc = mDetour->getOriginal<&renderParticles>();

    // Fire the event
    auto event = nes::make_holder<ParticleRenderEvent>();
    gFeatureManager->mDispatcher->trigger(event);

    // If canceled, skip rendering
    if (event->isCancelled()) return;

    return oFunc(a1, a2);
}

void NoParticleRenderHook::init() {
    mDetour = std::make_unique<Detour>("LevelRenderer::renderParticles",
        reinterpret_cast<void*>(SigManager::LevelRenderer_renderParticles),
        &NoParticleRenderHook::renderParticles);
}
