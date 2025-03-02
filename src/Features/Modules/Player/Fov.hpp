#pragma once

//
// Created by alteik on 11/11/2024.
//
#include <SDK/SigManager.hpp>
#include <Utils/Buffer.hpp>
#include <Utils/MemUtils.hpp>
#include <Features/Events/FOVEvent.hpp>
class Fov : public ModuleBase<Fov> {
public:

    NumberSetting mFov = NumberSetting("Speed", "speed u eating with", 110, 60, 180, 1);

    Fov() : ModuleBase("Fov", "Field of View(custom)", ModuleCategory::Player, 0, false)
    {
        addSetting(&mFov);

        mNames = {
            {Lowercase, "Fov"},
              {LowercaseSpaced, "fov"},
              {Normal, "Fov"},
              {NormalSpaced, "F O V"}
        };
    };
    void onEnable() override;
    void onDisable() override;
    void OnFovEvent(class FOVEvents& event);
};

