//
// Created by alteik on 11/11/2024.
//

#include "Fov.hpp"

void Fov::onEnable()
{
    gFeatureManager->mDispatcher->listen<FOVEvents, &Fov::OnFovEvent>(this);
}

void Fov::onDisable()
{
    gFeatureManager->mDispatcher->deafen<FOVEvents, &Fov::OnFovEvent>(this);
}

void Fov::OnFovEvent(FOVEvents& event)
{
    event.FOV = mFov.mValue;
}
