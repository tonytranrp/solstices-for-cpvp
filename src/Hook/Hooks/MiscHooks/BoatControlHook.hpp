#pragma once

#include <Hook/Hook.hpp>
#include <Utils/Structs.hpp>
#include <memory>

class BoatControlHook : public Hook
{
public:
    BoatControlHook() : Hook() {
        mName = "Boat::control::system";
    }

    static std::unique_ptr<Detour> mDetour;

    // Matching the function signature you discovered
    static void Control(
        __int64 self,
        int* BoatID,
        __int64 BoatState,
        __int64 BoatStateSeccond,
        __int64 VelocityPtr,
        __int64 MovementValues,
        unsigned int* ECS,
        __int64 HighLevelECS
    );

    void init() override;
};
