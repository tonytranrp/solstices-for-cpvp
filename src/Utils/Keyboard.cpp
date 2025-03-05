//
// Created by vastrakai on 6/29/2024.
//

#include "Keyboard.hpp"

#include <SDK/Minecraft/ClientInstance.hpp>
#include <SDK/Minecraft/KeyboardMouseSettings.hpp>
#include <SDK/Minecraft/Actor/Actor.hpp>
#include <SDK/Minecraft/Actor/Components/MoveInputComponent.hpp>

#include "StringUtils.hpp"

int Keyboard::getKeyId(const std::string& str)
{
    return mKeyMap[StringUtils::toLower(str)];
}

bool Keyboard::isUsingMoveKeys(bool includeSpaceShift)
{
    auto player = ClientInstance::get()->getLocalPlayer();
    if (!player) return false;

    auto& keyboard = *ClientInstance::get()->getKeyboardSettings();

    auto moveInput = player->getMoveInputComponent();
    bool isMoving = moveInput->mForward || moveInput->mBackward || moveInput->mLeft || moveInput->mRight;
    if (includeSpaceShift)
        return isMoving || Keyboard::mPressedKeys[keyboard["key.jump"]] || Keyboard::mPressedKeys[keyboard["key.sneak"]];

    return isMoving;
}

bool Keyboard::isStrafing()
{
    auto player = ClientInstance::get()->getLocalPlayer();
    if (!player) return false;

    auto moveInput = player->getMoveInputComponent();
    return moveInput->mLeft || moveInput->mRight || moveInput->mBackward;
}
bool Keyboard::WantSimulate(bool wanted) {
    if (wanted) {
        WantedTosimulate = true;
        return true;
    }
    else { 
        WantedTosimulate = false; 
        return false;
    }
}
bool Keyboard::SimulateKey(int key,bool isdown) {
    // Simulate a key press event:
    // First, call with isDown = true, then call with isDown = false.
    if (key != 0) {
        KeyToSimulate = key;
        Isdown = true;
        return true;
    }
    else {
        KeyToSimulate = 0;
        Isdown = false;
        return false;
    }
}
bool Keyboard::IsKeyPress(int key) {
    // Check if the key exists and its value is true.
    auto it = mPressedKeys.find(key);
    return (it != mPressedKeys.end() && it->second);
}
// returns the char representation of the key
std::string Keyboard::getKey(int keyId)
{
    for (const auto& [key, value] : mKeyMap)
    {
        if (value == keyId)
        {
            return StringUtils::toUpper(key);
        }
    }

    return "unknown";
}
