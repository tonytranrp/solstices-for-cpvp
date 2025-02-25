#pragma once

#include <Features/Events/ElytraGlideEvent.hpp>
#include <Hook/Hook.hpp>
#include <memory>
#include <bit>
#include <glm/vec3.hpp>

class ActorGlideHook : public Hook {
public:
    ActorGlideHook() : Hook() {
        mName = "ActorGlide::system";
    }

    // Detour for the render function
    static std::unique_ptr<Detour> mRenderDetour;

    // The hooked render function.
    // The function signature should match the original function's calling convention.
    /*
    * __int64 a1,
        __int64 a2,
        float *a3,
        _QWORD *a4,
        unsigned int *a5,
        _DWORD *a6,
        float *a7
    */
    static void render(__int64 a1,
        __int64 a2,
        float* a3,
        unsigned long* a4,
        unsigned int* a5,
        unsigned long* a6,
        float* a7) {
        // Retrieve the original function pointer
        auto ofunc = mRenderDetour->getOriginal<&ActorGlideHook::render>();
        

        // Preserve Movement Components from the movement array (a6 indices 6,7,8 store velocity)
        glm::vec3 velocity = { a7[6], a7[7], a7[8] };

        // Create and trigger the glide event allowing external modification of the velocity
        auto event = nes::make_holder<ElytraGlideEvent>(velocity);
        gFeatureManager->mDispatcher->trigger(event);

        // If the event was not cancelled, update velocity with the modified value
        if (!event->isCancelled()) {
            velocity = event->mVelocity;
        }

        // Call the original function
        ofunc(a1, a2, a3, a4, a5, a6,a7);

        // Override the movement values with the (potentially modified) velocity
        a7[6] = velocity.x;
        a7[7] = velocity.y;
        a7[8] = velocity.z;
    }

    void init() override {
        mRenderDetour = std::make_unique<Detour>(
            "ActorGlide::system",
            reinterpret_cast<void*>(SigManager::ActorGlide_system),
            &ActorGlideHook::render
        );
    }
};

std::unique_ptr<Detour> ActorGlideHook::mRenderDetour;
