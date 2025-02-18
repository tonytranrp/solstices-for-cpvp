#pragma once
//
// Created by vastrakai on 11/14/2024.
//
#include <Hook/Hooks/RenderHooks/ActorRenderDispatcherHook.hpp>
#include <Features/Events/ParticleRenderEvent.hpp>
class NoRender : public ModuleBase<NoRender> {
public:
    BoolSetting mNoBlockOverlay = BoolSetting("No Block Overlay", "Disables the block overlay from being stuck inside a block", true);
    BoolSetting mNoFire = BoolSetting("No Fire", "Disables the fire overlay", true);
   // BoolSetting mNoParticles = BoolSetting("No Particles", "Disables particle rendering", true);
    NoRender() : ModuleBase("NoRender", "Disables certain rendering elements", ModuleCategory::Visual, 0, false) {
        addSetting(&mNoBlockOverlay);
        addSetting(&mNoFire);
       // addSetting(&mNoParticles);

        mNames = {
            {Lowercase, "norender"},
            {LowercaseSpaced, "no render"},
            {Normal, "NoRender"},
            {NormalSpaced, "No Render"}
        };
    }

    static void patchOverlay(bool);
    static void patchFireRender(bool);
   /* void OnParticleRenderEvent(class ParticleRenderEvent& event) {
        if (mNoParticles.mValue) {
            event.cancel();
        }
    }*/
    void onEnable() override;
    void onDisable() override;
};