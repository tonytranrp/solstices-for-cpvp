#pragma once

#include <Features/Events/ElytraGlideEvent.hpp>
#include <Hook/Hook.hpp>
#include <memory>
#include <bit>
#include <glm/vec3.hpp>
#include <Features/Modules/Movement/Birdi.hpp>

class ActorGlideHook : public Hook {
public:
    ActorGlideHook() : Hook() {
        mName = "ActorGlide::system";
    }

    // Detour for the render function
    static std::unique_ptr<Detour> mRenderDetour;

    // Hooked function
    static void render(__int64 a1,
        __int64 a2,
        float* a3,
        unsigned long* a4,
        unsigned int* a5,
        unsigned long* a6,
        float* a7) {

        // Retrieve the original function pointer
        auto ofunc = mRenderDetour->getOriginal<&ActorGlideHook::render>();

        // Preserve original movement values
        glm::vec3 velocity = { a7[6], a7[7], a7[8] };

        // Check if the module is enabled before modifying anything
        auto* birdiModule = gFeatureManager->mModuleManager->getModule<Birdi>();
        if (birdiModule && birdiModule->mEnabled) {

            // Create and trigger the glide event
            auto event = nes::make_holder<ElytraGlideEvent>(velocity);
            gFeatureManager->mDispatcher->trigger(event);

            // If the event is not cancelled, apply the new velocity
            if (!event->isCancelled()) {
                velocity = event->mVelocity;
            }
        }

        // Call the original function
        ofunc(a1, a2, a3, a4, a5, a6, a7);

        // Only override movement values if the module is enabled
        if (birdiModule && birdiModule->mEnabled) {
            a7[6] = velocity.x;
            a7[7] = velocity.y;
            a7[8] = velocity.z;
        }
    }

    void init() override {
        mRenderDetour = std::make_unique<Detour>(
            "ActorGlide::system",
            reinterpret_cast<void*>(SigManager::ActorGlide_system),
            &ActorGlideHook::render
        );
    }
};

// Initialize static variable
std::unique_ptr<Detour> ActorGlideHook::mRenderDetour;