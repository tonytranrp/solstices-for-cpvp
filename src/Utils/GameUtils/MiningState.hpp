// MiningState.hpp
#pragma once

// Forward declarations
class PacketMine;

// Mining state enums that can be shared between PacketMine and QueueManager
namespace MiningStates {
    // Mining state for PacketMine module
    enum class PacketMineState {
        Idle,      // Not mining
        Waiting,   // Waiting (e.g. waiting for delay or progress accumulation)
        Rotating,  // In the process of rotating toward the target
        Breaking   // Actively breaking the block
    };

    // Queue state for mining operations
    namespace Queue {
        enum class Mining {
            Idle,       // Not mining
            Waiting,    // Waiting for delay or progress accumulation
            Rotating,   // Rotating toward target
            Breaking,   // Actively breaking the block
            Completed   // Mining operation completed
        };
    }

    // Conversion functions
    Queue::Mining convertPacketMineState(PacketMineState state);
    PacketMineState convertToPacketMineState(Queue::Mining state);
};