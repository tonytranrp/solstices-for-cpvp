//void __cdecl SprintTriggerSystem::setSprinting(class 
// StrictEntityContext const & __ptr64,
// class EntityModifier<struct AttributeRequestComponent> & __ptr64,
// struct ActorDataFlagComponent & __ptr64,
// struct ActorDataDirtyFlagsComponent & __ptr64,
// BOOL)
#pragma once

#include <Features/Events/SprintEvent.hpp>
#include <Hook/Hook.hpp>
#include <memory>
#include <bit>
#include <glm/vec3.hpp>
#include <SDK/Minecraft/Actor/Components/ActorDataFlagComponent.hpp>
#include <Features/Modules/Movement/Sprint.hpp>
class StrictEntityContext;
class ActorSetSprintingSystem : public Hook {
public:
    ActorSetSprintingSystem() : Hook() {
        mName = "ActorSprint::system";
    }

    // Detour for the render function
    static std::unique_ptr<Detour> mRenderDetour;

    // Hooked function
    static char render(__int64 a1, __int64 a2, __int64* a3, unsigned int* a4, __int16 a5) {//a5 = 1

        // Retrieve the original function pointer
        auto ofunc = mRenderDetour->getOriginal<&ActorSetSprintingSystem::render>();
        // Check if the module is enabled before modifying anything
        auto* birdiModule = gFeatureManager->mModuleManager->getModule<Sprint>();
        a5 = 1;
        // Call the original function
        return ofunc(a1, a2, a3, a4, a5);
    }

    void init() override {
        mRenderDetour = std::make_unique<Detour>(
            "ActorSprint::system",
            reinterpret_cast<void*>(SigManager::SetSprintingSystem),
            &ActorSetSprintingSystem::render
        );
    }
};

// Initialize static variable
std::unique_ptr<Detour> ActorSetSprintingSystem::mRenderDetour;