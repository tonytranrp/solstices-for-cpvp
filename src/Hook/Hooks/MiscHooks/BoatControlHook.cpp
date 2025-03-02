#include "BoatControlHook.hpp"
#include <Features/Events/BoatControlEvent.hpp> // Path depends on your project structure
#include <Features/FeatureManager.hpp>
#include <glm/vec3.hpp>
// Initialize static
std::unique_ptr<Detour> BoatControlHook::mDetour = nullptr;

void BoatControlHook::Control(
    __int64 self,
    int* BoatID,
    __int64 BoatState,
    __int64 BoatStateSeccond,
    __int64 VelocityPtr,
    __int64 MovementValues,
    unsigned int* ECS,
    __int64 HighLevelECS
)
{
    // 1) Retrieve original function pointer from the Detour
    auto original = mDetour->getOriginal<&BoatControlHook::Control>();

    /*
        The underlying decompiled snippet suggests that offsets 24 and 32
        from VelocityPtr might correspond to X and Z velocity.
        For a 3D velocity, you’d figure out the Y offset if you want
        vertical movement control. This is hypothetical – adjust to match
        your actual memory structure.
    */
    float xVel = *reinterpret_cast<float*>(VelocityPtr + 24);
    float zVel = *reinterpret_cast<float*>(VelocityPtr + 32);

    // If there's a Y component somewhere else, you'd read that too.
    // For example, if it's at (VelocityPtr + 28):
    // float yVel = *reinterpret_cast<float*>(VelocityPtr + 28);

    // 2) Make a glm::vec3 so modules can conveniently manipulate velocity
    glm::vec3 velocity{ xVel, 0.f, zVel };

    // 3) Construct and dispatch the event
    auto boatEvent = nes::make_holder<BoatControlEvent>(velocity);
    gFeatureManager->mDispatcher->trigger(boatEvent);

    // 4) If the event wasn’t cancelled, update the velocity from the event
    if (!boatEvent->isCancelled()) {
        // Overwrite velocity with the possibly modified vector
        velocity = boatEvent->mVelocity - boatEvent->mVelocity;
    }

    // 5) Write velocity back to memory. If you want Y as well, do likewise.
    *reinterpret_cast<float*>(VelocityPtr + 24) = 0.0f;
    *reinterpret_cast<float*>(VelocityPtr + 32) = 0.0f;

    // If there's a Y offset in memory, you'd do:
    // *reinterpret_cast<float*>(VelocityPtr + 28) = velocity.y;

    // 6) Finally, call the original function
    //    so that the game continues its normal logic
    original(self, BoatID, BoatState, BoatStateSeccond, VelocityPtr, MovementValues, ECS, HighLevelECS);
}

void BoatControlHook::init()
{
    mDetour = std::make_unique<Detour>(
        "Boat::control::system",
        reinterpret_cast<void*>(SigManager::Boat_control_system),
        &BoatControlHook::Control
    );
}
