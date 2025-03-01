#include "Birdi.hpp"
#include <SDK/Minecraft/Inventory/PlayerInventory.hpp>
bool Birdi::isjumping = false;
//sorry for skidding i didn't mean to:((((
/*
* static bool isjumping;
    bool haveelytrainarmorslor = false;
    bool goodaabb = false;
*/
void Birdi::onEnable()
{
    isjumping = false;
    haveelytrainarmorslor = false;
    goodaabb = false;
    isjumping = false;
    // gFeatureManager->mDispatcher->listen<BaseTickEvent, &Birdi::onBaseTickEvent>(this);
    gFeatureManager->mDispatcher->listen<PacketOutEvent, &Birdi::onPacketOutEvent>(this);
    gFeatureManager->mDispatcher->listen<ElytraGlideEvent, &Birdi::OnGlideEvents>(this);
    gFeatureManager->mDispatcher->listen<KeyEvent, &Birdi::OnKeyEvents>(this);
    gFeatureManager->mDispatcher->listen<BaseTickEvent, &Birdi::onBaseTickEvent>(this);
}

void Birdi::onDisable()
{
    isjumping = false;
    haveelytrainarmorslor = false;
    goodaabb = false;
    isjumping = false;
    // gFeatureManager->mDispatcher->deafen<BaseTickEvent, &Birdi::onBaseTickEvent>(this);
    gFeatureManager->mDispatcher->deafen<PacketOutEvent, &Birdi::onPacketOutEvent>(this);
    gFeatureManager->mDispatcher->deafen<ElytraGlideEvent, &Birdi::OnGlideEvents>(this);
    gFeatureManager->mDispatcher->deafen<KeyEvent, &Birdi::OnKeyEvents>(this);
    gFeatureManager->mDispatcher->deafen<BaseTickEvent, &Birdi::onBaseTickEvent>(this);
}
void Birdi::OnKeyEvents(KeyEvent& event) {
    auto player = ClientInstance::get()->getLocalPlayer();
    if (!player) return;
    bool isJumping = player->getMoveInputComponent()->mIsJumping;
    if (event.mKey == VK_SPACE && event.mPressed && haveelytrainarmorslor &&  goodaabb) {
        isjumping = true;
        event.setCancelled(true);
    }
    else { 
        isjumping = false;
        event.setCancelled(false);
    }


}
void Birdi::OnGlideEvents(ElytraGlideEvent& event) {
    auto player = ClientInstance::get()->getLocalPlayer();
    if (!player) return;
  
    bool isSneaking = player->getMoveInputComponent()->mIsSneakDown;

    glm::vec3 motion = glm::vec3(0, 0, 0);
    if (Keyboard::isUsingMoveKeys(true))
    {
        glm::vec2 calc = MathUtils::getMotion(player->getActorRotationComponent()->mYaw, SpeedX.mValue / 10);
        motion.x = calc.x;
        motion.z = calc.y;
        if (isjumping)
            motion.y += SpeedY.mValue / 10;
        else if (isSneaking)
            motion.y -= SpeedY.mValue / 10;
    }
    event.mVelocity = motion;




}
void Birdi::onBaseTickEvent(BaseTickEvent& event)
{
    auto localPlayer = ClientInstance::get()->getLocalPlayer();
    if (!localPlayer) return;
    auto mainInventory = localPlayer->getSupplies()->getContainer();
    if (!mainInventory)
        return;
    auto offhandContainer = localPlayer->getArmorContainer()->getItem(2);
    if (offhandContainer) {
        haveelytrainarmorslor = true;
    }
    if (localPlayer->getAABBShapeComponent()->mHeight < 1.8f) {
        goodaabb = true;
    }
   // ChatUtils::displayClientMessage("AABB Max {} {}" , localPlayer->getAABBShapeComponent()->mHeight, localPlayer->getAABB().mBounds->y);
    //ChatUtils::displayClientMessage("AABB Min {} {} {}", localPlayer->getAABB().mMin.x, localPlayer->getAABB().mMin.y, localPlayer->getAABB().mMin.z);
}


void Birdi::onPacketOutEvent(PacketOutEvent& event) const
{

    if (event.mPacket->getId() == PacketID::PlayerAuthInput)
    {
        auto player = ClientInstance::get()->getLocalPlayer();
        if (player == nullptr)
            return;

        auto packet = event.getPacket<PlayerAuthInputPacket>();
        packet->mInputData |= AuthInputAction::START_GLIDING;
        packet->mInputData &= ~AuthInputAction::STOP_GLIDING; // event started i guess
    }
}
