//
// Created by vastrakai on 9/16/2024.
//
#pragma once
#include <vector>
#include <string>
#include "Packet.hpp"

enum class ActorLinkType : unsigned char {
    None = 0x0,
    Riding = 0x1,
    Passenger = 0x2,
};

struct ActorLink {
    ActorLinkType mType;               // this+0x0
    uint64_t      mA;                  // this+0x8
    uint64_t      mB;                  // this+0x10
    bool          mImmediate;          // this+0x18
    bool          mPassengerInitiated; // this+0x19
};

class AddActorPacket : public Packet {
public:
    std::vector<ActorLink> mLinks;
    glm::vec3              mPos;
    glm::vec3              mVelocity;
    glm::vec2              mRot;
    float                  mYHeadRotation;
    float                  mYBodyRotation;
    uint64_t               mEntityId;   // Unique ID
    uint64_t               mRuntimeId;  // Runtime ID
};
