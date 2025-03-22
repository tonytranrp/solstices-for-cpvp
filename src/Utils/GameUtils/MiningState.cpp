// MiningState.cpp
#include "MiningState.hpp"

namespace MiningStates {
    // Convert PacketMineState to Queue::Mining
    Queue::Mining convertPacketMineState(PacketMineState state) {
        switch (state) {
        case PacketMineState::Idle:
            return Queue::Mining::Idle;
        case PacketMineState::Waiting:
            return Queue::Mining::Waiting;
        case PacketMineState::Rotating:
            return Queue::Mining::Rotating;
        case PacketMineState::Breaking:
            return Queue::Mining::Breaking;
        default:
            return Queue::Mining::Idle;
        }
    }

    // Convert Queue::Mining to PacketMineState
    PacketMineState convertToPacketMineState(Queue::Mining state) {
        switch (state) {
        case Queue::Mining::Idle:
            return PacketMineState::Idle;
        case Queue::Mining::Waiting:
            return PacketMineState::Waiting;
        case Queue::Mining::Rotating:
            return PacketMineState::Rotating;
        case Queue::Mining::Breaking:
            return PacketMineState::Breaking;
        case Queue::Mining::Completed:
            return PacketMineState::Idle; // Map Completed to Idle for PacketMine
        default:
            return PacketMineState::Idle;
        }
    }
}