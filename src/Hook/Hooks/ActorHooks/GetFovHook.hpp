#pragma once

#include <Features/Events/FOVEvent.hpp>
#include <Hook/Hook.hpp>
#include <memory>
#include <bit>
#include <glm/vec3.hpp>
#include <Features/Modules/Movement/Birdi.hpp>
#include <SDK/Minecraft/Rendering/LevelRenderer.hpp>

/*
* auto casted = *std::bit_cast<func_t*>(&ofunc);
										   // Apply the modified color from event

										   // Call the original function with the modified color
										   auto result = casted(first, a, applyEffects);

										   static auto& dispatcher = Prax::get<nes::event_dispatcher>();
										   auto event = nes::make_holder<FOVEvents>(result);
										   dispatcher.trigger(event);

										   result = event->FieldOfView;
										   return result;
*/
class GetFovHook : public Hook {
public:
    GetFovHook() : Hook() {
        mName = "LevelRendererPlayer::GetFov";
    }

    // Detour for the render function
    static std::unique_ptr<Detour> mRenderDetour;

    // Hooked function
    static float render(LevelRendererPlayer* first, float a, bool applyEffects) {

        auto ofunc = mRenderDetour->getOriginal<&GetFovHook::render>();
       
        auto result = ofunc(first, a, applyEffects);
        auto event = nes::make_holder<FOVEvents>(result);
        gFeatureManager->mDispatcher->trigger(event);
        result = event->FOV;
        return result;
    }

    void init() override {
        mRenderDetour = std::make_unique<Detour>(
            "LevelRendererPlayer::GetFov",
            reinterpret_cast<void*>(SigManager::LevelRendererPlayer_GetFov),
            &GetFovHook::render
        );
    }
};

// Initialize static variable
std::unique_ptr<Detour> GetFovHook::mRenderDetour;