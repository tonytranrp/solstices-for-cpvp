#include "Birdi.hpp"
#include <SDK/Minecraft/Inventory/PlayerInventory.hpp>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <string>
//#include "ChatUtils.hpp"  // Ensure this is included for ChatUtils usage

void Birdi::onEnable() {
    isSpacePressed = false;
    haveElytraInArmorSlot = false;
    glidingActive = false;
    accelProgress = 0.0f;
    accelerating = false; // Ensure acceleration logic starts fresh
    accelerationTimer = 0.0f;

    gFeatureManager->mDispatcher->listen<PacketOutEvent, &Birdi::onPacketOutEvent>(this);
    gFeatureManager->mDispatcher->listen<ElytraGlideEvent, &Birdi::OnGlideEvents>(this);
    gFeatureManager->mDispatcher->listen<KeyEvent, &Birdi::OnKeyEvents>(this);
    gFeatureManager->mDispatcher->listen<BaseTickEvent, &Birdi::onBaseTickEvent>(this);

    // Log enabling
    ChatUtils::displayClientMessage("Birdi module enabled");
    spdlog::debug("[Fly] Birdi enabled");

    // Check if chest slot has an Elytra (warn if missing)
    auto player = ClientInstance::get()->getLocalPlayer();
    if (player) {
        if (ItemStack* chestStack = player->getArmorContainer()->getItem(1)) {
            bool hasElytra = false;
            if (chestStack->mItem && *chestStack->mItem) {
                std::string itemName = (*chestStack->mItem)->mName;
                std::transform(itemName.begin(), itemName.end(), itemName.begin(), ::tolower);
                if (itemName.find("elytra") != std::string::npos) {
                    hasElytra = true;
                }
            }
            if (!hasElytra) {
                ChatUtils::displayClientMessage("No Elytra in chest slot - functionality limited");
                spdlog::debug("[Fly] No Elytra found in chest slot on enable");
            }
        }
    }
}

void Birdi::onDisable() {
    isSpacePressed = false;
    haveElytraInArmorSlot = false;
    glidingActive = false;
    accelProgress = 0.0f;
    accelerating = false;
    accelerationTimer = 0.0f;

    gFeatureManager->mDispatcher->deafen<PacketOutEvent, &Birdi::onPacketOutEvent>(this);
    gFeatureManager->mDispatcher->deafen<ElytraGlideEvent, &Birdi::OnGlideEvents>(this);
    gFeatureManager->mDispatcher->deafen<KeyEvent, &Birdi::OnKeyEvents>(this);
    gFeatureManager->mDispatcher->deafen<BaseTickEvent, &Birdi::onBaseTickEvent>(this);

    // Log disabling
    ChatUtils::displayClientMessage("Birdi module disabled");
    spdlog::debug("[Fly] Birdi disabled");
}

void Birdi::OnKeyEvents(KeyEvent& event) {
    auto player = ClientInstance::get()->getLocalPlayer();
    if (!player) return;

    int  key = event.mKey;
    bool pressed = event.mPressed;

    // Handle SPACE for ascending
    if (key == VK_SPACE) {
        if (pressed) {
            if (haveElytraInArmorSlot && glidingActive && !player->isOnGround()) {
                isSpacePressed = true;
                event.setCancelled(true); // Prevent default space behavior

                ChatUtils::displayClientMessage("Space pressed -> ascending (Elytra flight)");
                spdlog::debug("[Fly] Space pressed -> ascending (Elytra flight)");
            }
        }
        else {
            isSpacePressed = false;
            event.setCancelled(false);

            ChatUtils::displayClientMessage("Space released -> stop ascending");
            spdlog::debug("[Fly] Space released -> stop ascending");
        }
    }

    // Handle SHIFT (Sneak) for descending
    else if (key == VK_SHIFT) {
        if (pressed) {
            if (haveElytraInArmorSlot && glidingActive && !player->isOnGround()) {
                isShiftPressed = true;  // New variable to track Shift state
                event.setCancelled(true); // Prevent sneaking action

                ChatUtils::displayClientMessage("Shift pressed -> descending (Elytra flight)");
                spdlog::debug("[Fly] Shift pressed -> descending (Elytra flight)");
            }
        }
        else {
            isShiftPressed = false;
            event.setCancelled(false);

            ChatUtils::displayClientMessage("Shift released -> stop descending");
            spdlog::debug("[Fly] Shift released -> stop descending");
        }
    }

    else {
        event.setCancelled(false); // Don't interfere with other keys
    }
}
void Birdi::OnGlideEvents(ElytraGlideEvent& event) {
    auto player = ClientInstance::get()->getLocalPlayer();
    if (!player) return;

    glm::vec3 motion(0.0f);

    // Movement input tracking
    bool isMoving = Keyboard::isUsingMoveKeys(true);

    // Improved acceleration logic with smoother transitions
    float targetSpeed = (SpeedX.mValue / 10.0f);
    
    if (useAcceleration.mValue) {
        if (isMoving) {
            // Gradual acceleration when moving
            if (!accelerating) {
                accelerating = true;
                accelerationTimer = 0.0f;
            }
            
            // Smooth acceleration curve
            if (accelerationTimer < accelerationFactor.mValue) {
                float progress = accelerationTimer / accelerationFactor.mValue;
                // Apply easing function for smoother acceleration
                float easedProgress = progress * progress * (3.0f - 2.0f * progress);
                targetSpeed *= easedProgress;
                accelerationTimer += 1.0f;
            }
        } else {
            // Gradual deceleration when not moving
            if (accelerating) {
                accelerating = false;
                accelerationTimer = accelerationFactor.mValue;
            }
            
            // Smooth deceleration
            if (accelerationTimer > 0.0f) {
                float progress = accelerationTimer / accelerationFactor.mValue;
                // Apply easing function for smoother deceleration
                float easedProgress = progress * progress * (3.0f - 2.0f * progress);
                targetSpeed *= easedProgress;
                accelerationTimer = std::max(0.0f, accelerationTimer - 1.0f);
            } else {
                targetSpeed = 0.0f;
            }
        }
    }

    if (isMoving || accelerationTimer > 0.0f) {
        glm::vec2 calc = MathUtils::getMotion(player->getActorRotationComponent()->mYaw, targetSpeed);
        motion.x = calc.x;
        motion.z = calc.y;
    }

    // Vertical control with consistent speed
    if (isSpacePressed) {
        motion.y += SpeedY.mValue / 10.0f;
    } else if (isShiftPressed) {
        motion.y -= SpeedY.mValue / 10.0f;
    }

    event.mVelocity = motion;

    ChatUtils::displayClientMessage("[Fly] Setting Elytra glide velocity: {:.3f}, {:.3f}, {:.3f}", motion.x, motion.y, motion.z);
    spdlog::debug("[Fly] Setting Elytra glide velocity: {:.3f}, {:.3f}, {:.3f}", motion.x, motion.y, motion.z);
}

void Birdi::onBaseTickEvent(BaseTickEvent& event) {
    auto player = ClientInstance::get()->getLocalPlayer();
    if (!player) return;

    bool wasGliding = glidingActive;

    // Check for Elytra in chest slot
    haveElytraInArmorSlot = false;
    if (ItemStack* chestStack = player->getArmorContainer()->getItem(1)) {
        if (chestStack->mItem && *chestStack->mItem) {
            std::string itemName = (*chestStack->mItem)->mName;
            std::transform(itemName.begin(), itemName.end(), itemName.begin(), ::tolower);
            if (itemName.find("elytra") != std::string::npos) {
                haveElytraInArmorSlot = true;
            }
        }
    }

    // Decide if we are gliding based on AABB height
    float height = player->getAABBShapeComponent()->mHeight;
    glidingActive = (height < heightThreshold.mValue);

    // Entering glide
    if (!wasGliding && glidingActive) {
        ChatUtils::displayClientMessage("Elytra flight engaged");
        spdlog::debug("[Fly] Gliding started (AABB height {:.2f})", height);
        accelProgress = 0.0f; // reset
    }
    // Exiting glide
    if (wasGliding && !glidingActive) {
        ChatUtils::displayClientMessage("Elytra flight disengaged");
        spdlog::debug("[Fly] Gliding ended (AABB height {:.2f})", height);
        isSpacePressed = false;
    }
}
void Birdi::onPacketOutEvent(PacketOutEvent& event) const {
    if (event.mPacket->getId() == PacketID::PlayerAuthInput) {
        auto player = ClientInstance::get()->getLocalPlayer();
        if (!player) return;

        auto paip = event.getPacket<PlayerAuthInputPacket>();
        float fallDist = player->getFallDistance();

        // Check if Elytra is equipped, player is in mid-air, and has fallen enough
        if (haveElytraInArmorSlot) {
            if (!player->isOnGround() && fallDist >= fallDistanceThreshold.mValue) {
                paip->mInputData |= AuthInputAction::START_GLIDING;
                //paip->mInputData |= AuthInputAction::START_FLYING;
                //paip->mInputData &= ~AuthInputAction::STOP_GLIDING;
                paip->mInputData &= ~AuthInputAction::STOP_FLYING;
                ChatUtils::displayClientMessage("[Fly] START_GLIDING (fallDist={:.2f}, threshold={:.2f})", fallDist, fallDistanceThreshold.mValue);
                spdlog::debug("[Fly] START_GLIDING (fallDist={:.2f}, threshold={:.2f})", fallDist, fallDistanceThreshold.mValue);
            }
            else {
                // Remove gliding if conditions aren’t met
                paip->mInputData &= ~AuthInputAction::START_GLIDING;
            }

            // Handle rotation adjustments
            if (glidingActive) {
                float oldYaw = paip->mRot.y;
                float oldPitch = paip->mRot.x;

                // Get movement input
                auto moveVec = player->getMoveInputComponent()->mMoveVector;

                // Determine new yaw based on movement
                float newYaw = oldYaw;
                bool movingHorizontally = false; // Track if only X/Z movement

                // Yaw adjustments (turning)
                if (moveVec.y > 0 && moveVec.x == 0) {  // Moving FORWARD (W)
                    newYaw = oldYaw;
                    movingHorizontally = true;
                }
                else if (moveVec.y < 0 && moveVec.x == 0) {  // Moving BACKWARD (S)
                    newYaw = oldYaw + 180.0f;
                    movingHorizontally = true;
                }
                else if (moveVec.x < 0 && moveVec.y == 0) {  // Moving LEFT (A)
                    newYaw = oldYaw + 90.0f;
                    movingHorizontally = true;
                }
                else if (moveVec.x > 0 && moveVec.y == 0) {  // Moving RIGHT (D)
                    newYaw = oldYaw - 90.0f;
                    movingHorizontally = true;
                }
                else if (moveVec.y > 0 && moveVec.x < 0) {  // Moving DIAGONALLY FORWARD + LEFT (WA)
                    newYaw = oldYaw + 45.0f;
                    movingHorizontally = true;
                }
                else if (moveVec.y > 0 && moveVec.x > 0) {  // Moving DIAGONALLY FORWARD + RIGHT (WD)
                    newYaw = oldYaw - 45.0f;
                    movingHorizontally = true;
                }
                else if (moveVec.y < 0 && moveVec.x < 0) {  // Moving DIAGONALLY BACKWARD + LEFT (SA) (FIXED)
                    newYaw = oldYaw + 135.0f;
                    movingHorizontally = true;
                }
                else if (moveVec.y < 0 && moveVec.x > 0) {  // Moving DIAGONALLY BACKWARD + RIGHT (SD) (FIXED)
                    newYaw = oldYaw - 135.0f;
                    movingHorizontally = true;
                }

                // Wrap yaw between -180 and 180 degrees
                if (newYaw > 180.0f) newYaw -= 360.0f;
                if (newYaw < -180.0f) newYaw += 360.0f;

                // Adjust pitch (only change when going up/down)
                float newPitch = oldPitch;
                if (isSpacePressed) {
                    newPitch = -45.0f;  // Looking UP
                }
                else if (isShiftPressed) {
                    newPitch = 45.0f;  // Looking DOWN
                }
                else if (movingHorizontally) {
                    newPitch = 0.0f;  // Keep pitch level when moving straight
                }

                // Apply final rotation
                paip->mRot.x = newPitch;
                paip->mRot.y = newYaw;
                paip->mYHeadRot = newYaw;
            }
        }
        else {
            // No Elytra => No Gliding
            paip->mInputData &= ~AuthInputAction::START_GLIDING;
        }
    }
}



