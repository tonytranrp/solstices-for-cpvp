//
// Created by jcazm on 7/27/2024.
//

#include "ItemPhysics.hpp"

#include <random>
#include <Features/Events/ItemRendererEvent.hpp>
#include <minhook/src/buffer.h>
#include <SDK/Minecraft/ClientInstance.hpp>
#include <SDK/Minecraft/Actor/Actor.hpp>
#include <SDK/Minecraft/Actor/ActorRenderData.hpp>
static std::unique_ptr<Detour> glm_rotateHook;
ItemPhysics* ItemPhysics::s_instance = nullptr;

static std::array<std::byte, 4> getRipRel(uintptr_t instructionAddress, uintptr_t targetAddress) {
    uintptr_t relAddress = targetAddress - (instructionAddress + 4); // 4 bytes for RIP-relative addressing
    std::array<std::byte, 4> relRipBytes{};

    for (size_t i = 0; i < 4; ++i) {
        relRipBytes[i] = static_cast<std::byte>((relAddress >> (i * 8)) & 0xFF);
    }

    return relRipBytes;
}
static void glm_rotate_wrapper(glm::mat4x4& mat, float angle, float x, float y, float z) {
    if (ItemPhysics::s_instance) {
        ItemPhysics::s_instance->glm_rotate(mat, angle, x, y, z);
    }
}

void ItemPhysics::glm_rotate(glm::mat4x4 &mat, float angle, float x, float y, float z) {
   // static auto thisMod = gFeatureManager->mModuleManager->getModule<ItemPhysics>();
    static auto rotateSig = SigManager::glm_rotate;
    using glm_rotate_t = void(__fastcall*)(glm::mat4x4&, float, float, float, float);
    static auto glm_rotate = reinterpret_cast<glm_rotate_t>(rotateSig);
    if (renderData == nullptr)
        return;

    auto curr = renderData->mActor;

    static float height = 0.5f;

    if (!actorData.contains(curr)) {
        std::random_device rd;
        std::mt19937 gen(rd());

        std::uniform_int_distribution<> dist(0, 1);
        std::uniform_int_distribution<> dist2(0, 359);

        const auto vec = glm::vec3(dist2(gen), dist2(gen), dist2(gen));
        const auto sign = glm::ivec3(dist(gen) * 2 - 1, dist(gen) * 2 - 1, dist(gen) * 2 - 1);
        bool onground = true;
        auto def = std::tuple{ onground ? 0.f : height, vec, sign };
        actorData.emplace(curr, def);
    }

    const float deltaTime = 1.f / static_cast<float>(ImGui::GetIO().Framerate);

    float& yMod = std::get<0>(actorData.at(curr));

    yMod -= height * deltaTime;

    if (yMod <= 0.f)
        yMod = 0.f;

    glm::vec3 pos = renderData->position;
    pos.y += yMod;

    auto& vec = std::get<1>(actorData.at(curr));
    const auto& sign = std::get<2>(actorData.at(curr));


    mat = translate(mat, {pos.x, pos.y, pos.z});

    glm_rotate(mat, vec.x, 1.f, 0.f, 0.f);
    glm_rotate(mat, vec.y, 0.f, 1.f, 0.f);
    glm_rotate(mat, vec.z, 0.f, 0.f, 1.f);
}

std::vector<unsigned char> gRtBytes = {0xE8};
DEFINE_PATCH_FUNC(patchPosAddr, SigManager::glm_rotateRef, gRtBytes);

DEFINE_NOP_PATCH_FUNC(patchTranslateRef, SigManager::SigManager::glm_translateRef, 0x5);
//DEFINE_NOP_PATCH_FUNC(patchTranslateRef2, SigManager::SigManager::glm_translateRef2, 0x5);

std::vector<unsigned char> gOgRtBytes = {};
void ItemPhysics::onEnable() {
    s_instance = this;  // Set the static instance pointer

    gFeatureManager->mDispatcher->listen<RenderEvent, &ItemPhysics::onRenderEvent>(this);
    gFeatureManager->mDispatcher->listen<ItemRendererEvent, &ItemPhysics::onItemRendererEvent>(this);

    static auto rotateAddr = reinterpret_cast<void*>(SigManager::glm_rotateRef);
    if (gOgRtBytes.empty())
        gOgRtBytes = MemUtils::readBytes(reinterpret_cast<uintptr_t>(rotateAddr), 10);

    static auto posAddr = SigManager::ItemPositionConst + 4;
    origPosRel = *reinterpret_cast<uint32_t*>(posAddr);

    if (glm_rotateHook == nullptr) {
        // Use the static wrapper function here.
        glm_rotateHook = std::make_unique<Detour>("glm_rotate", rotateAddr, glm_rotate_wrapper);
    }

    glm_rotateHook->enable();
    patchPosAddr(true);

    newPosRel = static_cast<float*>(AllocateBuffer(reinterpret_cast<void*>(posAddr)));
    *newPosRel = 0.f;

    const auto newRipRel = getRipRel(posAddr, reinterpret_cast<uintptr_t>(newPosRel));
    MemUtils::writeBytes(posAddr, newRipRel.data(), 4);

    patchTranslateRef(true);
}


void ItemPhysics::onDisable() {
    gFeatureManager->mDispatcher->deafen<RenderEvent, &ItemPhysics::onRenderEvent>(this);
    gFeatureManager->mDispatcher->deafen<ItemRendererEvent, &ItemPhysics::onItemRendererEvent>(this);
    static auto posAddr = SigManager::ItemPositionConst + 4;

    MemUtils::writeBytes(posAddr, &origPosRel, 4);
    patchTranslateRef(false);
    //patchTranslateRef2(false);
    FreeBuffer(newPosRel);

    MemUtils::writeBytes(SigManager::glm_rotateRef, gOgRtBytes);
    patchPosAddr(false);
    glm_rotateHook->restore();


    actorData.clear();
}

void ItemPhysics::onRenderEvent(RenderEvent &event) {
    static bool playerNull = ClientInstance::get()->getLocalPlayer() == nullptr;

    if (playerNull != (ClientInstance::get()->getLocalPlayer() == nullptr)) {
        playerNull = ClientInstance::get()->getLocalPlayer() == nullptr;

        if (playerNull) {
            actorData.clear();
            renderData = nullptr;
        }
    }
}

void ItemPhysics::onItemRendererEvent(ItemRendererEvent &event) {
    this->renderData = event.mActorRenderData;
}