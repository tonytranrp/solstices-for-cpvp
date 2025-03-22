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
        mWantedTosimulate = true;
        mSimulatedKeys.clear(); // Clear any previous simulated keys
        return true;
    } else {
        mWantedTosimulate = false;
        mKeyToSimulate = 0;
        mIsdown = false;
        mSimulatedKeys.clear();
        return false;
    }
}

bool Keyboard::SimulateKey(int key, bool isdown) {
    if (key != 0) {
        mKeyToSimulate = key;
        mIsdown = isdown;
        mSimulatedKeys.push_back(key);
        return true;
    }
    return false;
}

void Keyboard::CancelSimulation() {
    mWantedTosimulate = false;
    mKeyToSimulate = 0;
    mIsdown = false;
    mSimulatedKeys.clear();
}

bool Keyboard::IsSimulatingKey(int key) {
    return std::find(mSimulatedKeys.begin(), mSimulatedKeys.end(), key) != mSimulatedKeys.end();
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
