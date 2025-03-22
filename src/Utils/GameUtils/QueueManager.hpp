// QueueManager.hpp
#pragma once
#include "MiningState.hpp"
#include <unordered_map>
#include <string>
#include <queue>
#include <functional>
#include <memory>
#include <typeindex>
#include <typeinfo>

// Forward declarations
class PacketMine;

// QueueState enum - centralized state definitions for all modules
namespace QueueState {
    // General states that can be used across modules
    enum class General {
        Idle,
        Waiting,
        Processing,
        Completed,
        Error
    };

    // Mining-related states are now defined in MiningState.hpp
    using Mining = MiningStates::Queue::Mining;

    // Combat-related states (for future implementation)
    enum class Combat {
        Idle,
        Targeting,
        Attacking,
        Defending,
        Retreating
    };

    // Movement-related states (for future implementation)
    enum class Movement {
        Idle,
        Walking,
        Running,
        Jumping,
        Flying,
        Swimming
    };
}

// QueueManager class - manages module states and queues
class QueueManager {
private:
    // Singleton instance
    static QueueManager* sInstance;

    // Private constructor for singleton pattern
    QueueManager() = default;

    // State storage for different modules and state types
    template<typename StateEnum>
    struct StateStorage {
        std::unordered_map<void*, StateEnum> states;
    };

    // Map to store different state types
    std::unordered_map<std::type_index, void*> mStateStorages;

    // Get or create state storage for a specific state enum type
    template<typename StateEnum>
    StateStorage<StateEnum>* getStateStorage() {
        auto typeIndex = std::type_index(typeid(StateEnum));
        auto it = mStateStorages.find(typeIndex);
        
        if (it == mStateStorages.end()) {
            auto storage = new StateStorage<StateEnum>();
            mStateStorages[typeIndex] = storage;
            return storage;
        }
        
        return static_cast<StateStorage<StateEnum>*>(it->second);
    }

    // Queue storage for different modules and state types
    template<typename StateEnum>
    struct QueueStorage {
        std::unordered_map<void*, std::queue<StateEnum>> queues;
    };

    // Map to store different queue types
    std::unordered_map<std::type_index, void*> mQueueStorages;

    // Get or create queue storage for a specific state enum type
    template<typename StateEnum>
    QueueStorage<StateEnum>* getQueueStorage() {
        auto typeIndex = std::type_index(typeid(StateEnum));
        auto it = mQueueStorages.find(typeIndex);
        
        if (it == mQueueStorages.end()) {
            auto storage = new QueueStorage<StateEnum>();
            mQueueStorages[typeIndex] = storage;
            return storage;
        }
        
        return static_cast<QueueStorage<StateEnum>*>(it->second);
    }

public:
    // Delete copy constructor and assignment operator
    QueueManager(const QueueManager&) = delete;
    QueueManager& operator=(const QueueManager&) = delete;

    // Get singleton instance
    static QueueManager* get() {
        if (!sInstance) {
            sInstance = new QueueManager();
        }
        return sInstance;
    }

    // Clean up resources
    ~QueueManager() {
        for (auto& [type, storage] : mStateStorages) {
            delete storage;
        }
        for (auto& [type, storage] : mQueueStorages) {
            delete storage;
        }
    }

    // Set state for a module
    template<typename StateEnum>
    void setState(void* modulePtr, StateEnum state) {
        auto storage = getStateStorage<StateEnum>();
        storage->states[modulePtr] = state;
    }

    // Get state for a module
    template<typename StateEnum>
    StateEnum getState(void* modulePtr, StateEnum defaultState = static_cast<StateEnum>(0)) {
        auto storage = getStateStorage<StateEnum>();
        auto it = storage->states.find(modulePtr);
        
        if (it == storage->states.end()) {
            return defaultState;
        }
        
        return it->second;
    }
    
    // Get state for a module (const version)
    template<typename StateEnum>
    StateEnum getState(const void* modulePtr, StateEnum defaultState = static_cast<StateEnum>(0)) {
        // Use const_cast to reuse the non-const implementation
        // This is safe because we're only reading from the map, not modifying it
        return getState<StateEnum>(const_cast<void*>(modulePtr), defaultState);
    }

    // Check if module is in a specific state
    template<typename StateEnum>
    bool isInState(void* modulePtr, StateEnum state) {
        return getState<StateEnum>(modulePtr) == state;
    }
    
    // Check if module is in a specific state (const version)
    template<typename StateEnum>
    bool isInState(const void* modulePtr, StateEnum state) {
        return getState<StateEnum>(modulePtr) == state;
    }

    // Add state to queue for a module
    template<typename StateEnum>
    void queueState(void* modulePtr, StateEnum state) {
        auto storage = getQueueStorage<StateEnum>();
        storage->queues[modulePtr].push(state);
    }

    // Get next state from queue for a module
    template<typename StateEnum>
    bool getNextQueuedState(void* modulePtr, StateEnum& outState) {
        auto storage = getQueueStorage<StateEnum>();
        auto& queue = storage->queues[modulePtr];
        
        if (queue.empty()) {
            return false;
        }
        
        outState = queue.front();
        queue.pop();
        return true;
    }

    // Clear queue for a module
    template<typename StateEnum>
    void clearQueue(void* modulePtr) {
        auto storage = getQueueStorage<StateEnum>();
        auto it = storage->queues.find(modulePtr);
        
        if (it != storage->queues.end()) {
            std::queue<StateEnum> empty;
            std::swap(it->second, empty);
        }
    }

    // Utility functions for PacketMine module
    
    // Convert MiningStates::PacketMineState to QueueState::Mining
    static QueueState::Mining convertPacketMineState(MiningStates::PacketMineState state) {
        return MiningStates::convertPacketMineState(state);
    }
    
    // Convert QueueState::Mining to MiningStates::PacketMineState
    static MiningStates::PacketMineState convertToPacketMineState(QueueState::Mining state) {
        return MiningStates::convertToPacketMineState(state);
    }
};

// Static instance is initialized in QueueManager.cpp