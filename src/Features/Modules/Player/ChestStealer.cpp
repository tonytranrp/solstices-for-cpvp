//
// Created by vastrakai on 7/5/2024.
//

#include "ChestStealer.hpp"

#include <random>
#include <Features/FeatureManager.hpp>
#include <Features/Events/BaseTickEvent.hpp>
#include <Features/Events/ContainerScreenTickEvent.hpp>
#include <Features/Events/PacketInEvent.hpp>
#include <SDK/Minecraft/ClientInstance.hpp>
#include <SDK/Minecraft/Actor/Actor.hpp>
#include <Features/Events/PacketOutEvent.hpp>
#include <Features/Events/PacketInEvent.hpp>
#include <SDK/Minecraft/Inventory/ContainerManagerModel.hpp>
#include <SDK/Minecraft/Inventory/ContainerScreenController.hpp>
#include <SDK/Minecraft/Inventory/ItemStack.hpp>
#include <SDK/Minecraft/Inventory/PlayerInventory.hpp>
#include <SDK/Minecraft/Network/LoopbackPacketSender.hpp>
#include <SDK/Minecraft/Network/MinecraftPackets.hpp>
#include <SDK/Minecraft/Network/Packets/ContainerClosePacket.hpp>
#include <SDK/Minecraft/Network/Packets/InteractPacket.hpp>
#include <SDK/Minecraft/Network/Packets/PlayerActionPacket.hpp>

#include "InvManager.hpp"


void ChestStealer::onContainerScreenTickEvent(ContainerScreenTickEvent& event) const
{
    if (mMode.mValue != Mode::Normal) return;
    static uint64_t lastSteal = 0;
    auto csc = event.mController;
    auto player = ClientInstance::get()->getLocalPlayer();
    auto container = player->getContainerManagerModel();

    static bool isStealing = false;

    std::vector<int> itemz = {};
    for (int i = 0; i < 54; i++) {
        ItemStack* stack = container->getSlot(i);
        if (mIgnoreUseless.mValue && InvManager::isItemUseless(stack, -1)) continue;
        if (stack && stack->mItem) itemz.push_back(i);
    }

    if (itemz.empty())
    {
        if (lastSteal + 200 < NOW) {
            isStealing = false;
            csc->_tryExit();
        }
        return;
    }


    static uint64_t delay = getDelay();

    if (!isStealing) {
        NotifyUtils::notify("Stealing!", 1.0f + (static_cast<float>(delay) / 1000.0f * itemz.size()), Notification::Type::Info);
        isStealing = true;
    }

    if (lastSteal + delay < NOW) {
        for (const int i : itemz) {
            csc->handleAutoPlace("container_items", i);
            lastSteal = NOW;
            delay = getDelay(); // Randomize delay again
            break;
        }
    }
}


void sendCloseTransac(ContainerID id)
{
    auto packet = MinecraftPackets::createPacket<ContainerClosePacket>();
    packet->mContainerId = id;
    packet->mServerInitiatedClose = false;
    ClientInstance::get()->getPacketSender()->sendToServer(packet.get());
    spdlog::debug("Sent close transaction packet for container {}", magic_enum::enum_name(id));
}

void ChestStealer::reset()
{
    if (mMode.mValue == Mode::Silent)
        sendCloseTransac(mCurrentContainerId);

    mIsStealing = false;
    mIsChestOpen = false;
}

void ChestStealer::onEnable()
{
    gFeatureManager->mDispatcher->listen<ContainerScreenTickEvent, &ChestStealer::onContainerScreenTickEvent>(this);
    gFeatureManager->mDispatcher->listen<PacketInEvent, &ChestStealer::onPacketInEvent>(this);
    gFeatureManager->mDispatcher->listen<PacketOutEvent, &ChestStealer::onPacketOutEvent>(this);
    gFeatureManager->mDispatcher->listen<BaseTickEvent, &ChestStealer::onBaseTickEvent>(this);
    gFeatureManager->mDispatcher->listen<RenderEvent, &ChestStealer::onRenderEvent>(this);
}

void ChestStealer::onDisable()
{
    gFeatureManager->mDispatcher->deafen<ContainerScreenTickEvent, &ChestStealer::onContainerScreenTickEvent>(this);
    gFeatureManager->mDispatcher->deafen<PacketInEvent, &ChestStealer::onPacketInEvent>(this);
    gFeatureManager->mDispatcher->deafen<PacketOutEvent, &ChestStealer::onPacketOutEvent>(this);
    gFeatureManager->mDispatcher->deafen<BaseTickEvent, &ChestStealer::onBaseTickEvent>(this);
    gFeatureManager->mDispatcher->deafen<RenderEvent, &ChestStealer::onRenderEvent>(this);
}

int startingEmptySlot = -1;

int getFirstEmptySlot()
{
    auto player = ClientInstance::get()->getLocalPlayer();
    for (int i = 0; i < 36; i++) {
        if (startingEmptySlot != -1 && i < startingEmptySlot) continue;

        ItemStack* stack = player->getSupplies()->getContainer()->getItem(i);
        if (!stack->mItem)
        {
            startingEmptySlot = i + 1;
            return i;
        }
    }

    return -1;
}

// TODO: This will cause issues with high ping because the items aren't updated with setSlot.

void ChestStealer::takeItem(int slot, ItemStack& item)
{
    auto player = ClientInstance::get()->getLocalPlayer();

    int from = slot;
    int to = getFirstEmptySlot();
    ItemStack* item2 = player->getSupplies()->getContainer()->getItem(to);
    auto item1 = item;

    InventoryAction action1 = InventoryAction(from, &item1, item2);
    InventoryAction action2 = InventoryAction(to, item2, &item1);


    action1.mSource.mType = InventorySourceType::ContainerInventory;
    action2.mSource.mType = InventorySourceType::ContainerInventory;
    action1.mSource.mContainerId = static_cast<int>(ContainerID::Chest);
    action2.mSource.mContainerId = static_cast<int>(ContainerID::Inventory);

    auto pkt = MinecraftPackets::createPacket<InventoryTransactionPacket>();

    auto cit = std::make_unique<ComplexInventoryTransaction>();
    cit->data.addAction(action1);
    cit->data.addAction(action2);

    pkt->mTransaction = std::move(cit);

    ClientInstance::get()->getPacketSender()->sendToServer(pkt.get());
}

std::map<int, uint64_t> itemDelays;


void ChestStealer::onBaseTickEvent(BaseTickEvent& event)
{
    auto player = event.mActor;
#ifdef __PRIVATE_BUILD__

    if (!mIsChestOpen)
    {
        mIsStealing = false;
        return;
    }

    mIsStealing = true;

    int itemIndex = 0;
    std::map<int, ItemStack> items = {};

    spdlog::debug("Items to take: {}", mItemsToTake.size());
    for (ItemStack& item : mItemsToTake) {
        if (!item.mItem)
        {
            itemIndex++;
            continue;
        }

        if (mIgnoreUseless.mValue && InvManager::isItemUseless(&item, -1))
        {
            itemIndex++;
            continue;
        }
        items[itemIndex] = item;
        spdlog::debug("slot {} has an item", itemIndex);
        itemIndex++;
    }

    mRemainingItems = items.size();

    uint64_t predictedStealTime = (static_cast<uint64_t>(getDelay()) * items.size()) + 2000 + mLastOpen;

    if (predictedStealTime < NOW)
    {
        ChatUtils::displayClientMessage("§cStealing timed out...");
        reset();
        return;
    }

    if (mLastItemTaken + static_cast<uint64_t>(getDelay()) > NOW) return;

    //ChatUtils::displayClientMessage("Item count: " + std::to_string(items.size()));
    if (items.size() == 0)
    {
        reset();
        return;
    }

    for (auto& [slot, item] : items)
    {
        if (!item.mItem) continue;
        takeItem(slot, item);
        //ChatUtils::displayClientMessage("Took item from slot " + std::to_string(slot));
        items.erase(slot);
        if (doDelay()) return;
    }
#endif
}

bool ChestStealer::doDelay()
{
    if (mDelay.mValue != 0 && !mRandomizeDelay.mValue || mRandomizeDelay.mValue)
    {
        mLastItemTaken = NOW;
        return true;
    }

    return false;
}

void ChestStealer::onRenderEvent(class RenderEvent& event)
{
#ifdef __PRIVATE_BUILD__
    if (mMode.mValue != Mode::Silent) return;

    float immediateProgress = 0.0f;
    static float animProgress = 0.0f;

    int total = mTotalItems;
    int taken = mTotalItems - mRemainingItems;
    if (total == 0) return;

    // use mathutils::lerp
    immediateProgress = static_cast<float>(taken) / static_cast<float>(total);

    float lerp = ImGui::GetIO().DeltaTime * 10.0f;
    animProgress = mIsChestOpen && mIsStealing ? MathUtils::lerp(animProgress, immediateProgress, lerp) : MathUtils::lerp(animProgress, 0.0f, lerp);

    if (animProgress < 0.01f)
    {
        animProgress = 0.0f;
        return;
    }

    auto drawList = ImGui::GetBackgroundDrawList();


    AABB blockAabb = AABB(mHighlightedPos, glm::vec3(1, 1, 1));
    glm::vec3 progresSize = glm::vec3(animProgress, animProgress, animProgress);

    glm::vec3 centeredHighlight = mHighlightedPos;
    // Center the highlight depending on the progress
    centeredHighlight.x += 0.5f - (animProgress / 2.f);
    centeredHighlight.y += 0.5f - (animProgress / 2.f);
    centeredHighlight.z += 0.5f - (animProgress / 2.f);

    AABB blockProgressAabb = AABB(centeredHighlight, progresSize);

    std::vector<ImVec2> imPoints = MathUtils::getImBoxPoints(blockAabb);
    std::vector<ImVec2> imPoints2 = MathUtils::getImBoxPoints(blockProgressAabb);
    ImColor cColor = ColorUtils::getThemedColor(0);

    drawList->AddConvexPolyFilled(imPoints2.data(), imPoints2.size(), ImColor(cColor.Value.x, cColor.Value.y, cColor.Value.z, 0.4f));
    if (mIsChestOpen && mIsStealing) drawList->AddPolyline(imPoints.data(), imPoints.size(), cColor, 0, 2.0f);
    else drawList->AddPolyline(imPoints2.data(), imPoints2.size(), cColor, 0, 2.0f);

#endif
}

void ChestStealer::onPacketOutEvent(class PacketOutEvent& event)
{
#ifdef __PRIVATE_BUILD__
    /*if (event.mPacket->getId() == PacketID::InventoryTransaction)
    {
        auto itp = event.getPacket<InventoryTransactionPacket>();
        if (itp->mTransaction->type == ComplexInventoryTransaction::Type::ItemUseTransaction)
        {
            auto iut = reinterpret_cast<ItemUseInventoryTransaction*>(itp->mTransaction.get());
            if (iut->mActionType == ItemUseInventoryTransaction::ActionType::Use)
            {
                auto pos = glm::floor(iut->mClickPos);
                spdlog::debug("ItemUseInventoryTransaction::ActionType::Use at {}, {}, {}", pos.x, pos.y, pos.z);
                mLastPos = pos;
            }
        }


    }*/

    if (event.mPacket->getId() == PacketID::PlayerAction)
    {
        auto packet = event.getPacket<PlayerActionPacket>();
        if (packet->mAction == PlayerActionType::StartItemUseOn)
        {
            auto pos = packet->mPos;
            mLastPos = pos;
        }
    }

    if (event.mPacket->getId() == PacketID::Interact && mIsStealing && mMode.mValue == Mode::Silent)
    {
        auto packet = event.getPacket<InteractPacket>();
        if (packet->mAction == InteractPacket::Action::OpenInventory)
        {
            spdlog::warn("Cancelled InteractPacket::Action::OpenInventory because we are silently stealing!");
            event.cancel();
        }
    }
#endif
}

void ChestStealer::onPacketInEvent(PacketInEvent& event)
{
#ifdef __PRIVATE_BUILD__
    if (mMode.mValue != Mode::Silent) return;

    if (event.mPacket->getId() == PacketID::ContainerOpen)
    {
        auto cop = event.getPacket<ContainerOpenPacket>();
        if (cop->mContainerId != ContainerID::Chest) return;

        spdlog::debug("Opened chest with id {}", magic_enum::enum_name(cop->mContainerId));

        mCurrentContainerId = cop->mContainerId;
        mIsChestOpen = true;
        mLastOpen = NOW;
        mTotalDirty = true;
        mHighlightedPos = mLastPos;
        event.cancel();
    }

    if (event.mPacket->getId() == PacketID::ContainerClose)
    {
        auto cop = event.getPacket<ContainerClosePacket>();
        if (cop->mContainerId != ContainerID::Chest) return;

        spdlog::debug("Closed chest with id {}", magic_enum::enum_name(cop->mContainerId));

        mIsChestOpen = false;
        mIsStealing = false;
        event.cancel();
    }

    if (event.mPacket->getId() == PacketID::InventoryContent)
    {
        auto icp = event.getPacket<InventoryContentPacket>();
        if (icp->mInventoryId == mCurrentContainerId)
        {
            mItemsToTake.clear();
            for (auto& item : icp->mSlots)
            {
                mItemsToTake.push_back(ItemStack::fromDescriptor(item));
            }

            if (mTotalDirty)
            {
                mTotalDirty = false;

                int total = 0;
                for (auto& item : mItemsToTake)
                {
                    if (!item.mItem) continue;

                    total++;
                }

                mTotalItems = total;
            }

            event.cancel();
        } else if (icp->mInventoryId == ContainerID::Inventory)
        {
            startingEmptySlot = -1;
            spdlog::info("Server has updated our inventory");
        }

        spdlog::debug("InventoryContentPacket for container {} has {} item stacks", magic_enum::enum_name(icp->mInventoryId), icp->mSlots.size());
    }
#endif

}

uint64_t ChestStealer::getDelay() const
{
    if (mRandomizeDelay.mValue)
    {
        return static_cast<uint64_t>(MathUtils::random(mRandomizeMin.mValue, mRandomizeMax.mValue));
    }
    return static_cast<uint64_t>(mDelay.mValue);
}
