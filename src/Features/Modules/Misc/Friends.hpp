#pragma once
//
// Created by vastrakai on 7/12/2024.
//

#include <Features/Modules/Module.hpp>
#include <SDK/Minecraft/Actor/Actor.hpp>

inline class Friends* gFriendManager = nullptr;

class Friends : public ModuleBase<Friends> {
public:
    Friends() : ModuleBase<Friends>("Friends", "Add yo homies with .friend!", ModuleCategory::Misc, 0, false)
    {
        mNames = {
            {Lowercase, "friends"},
            {LowercaseSpaced, "friends"},
            {Normal, "Friends"},
            {NormalSpaced, "Friends"},
        };

        gFriendManager = this;
    }

    static std::vector<std::string> mFriends;

    void onInit() override;
    static bool isFriend(const std::string& name);
    static bool isFriend(Actor* actor);
    static void addFriend(const std::string& name);
    static void addFriend(Actor* actor);
    static void removeFriend(const std::string& name);
    static void removeFriend(Actor* actor);
    static void clearFriends();
};