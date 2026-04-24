#pragma once
#include "PlayerState.h"

// --- 2. State Machine System ---
class StateMachine
{
public:
    ~StateMachine() {
        if (currentState) {
            delete currentState;
            currentState = nullptr;
        }
    }

    void Initialize(PlayerState* startState, Player* player) {
        currentState = startState;
        currentState->Enter(player);
    }

    void ChangeState(Player* player, PlayerState* newState) {
        if (currentState) {
            currentState->Exit(player);
            delete currentState; // Hapus state lama
        }
        currentState = newState;
        currentState->Enter(player);
    }

    void Update(Player* player, float elapsedTime) {
        if (currentState) {
            currentState->Update(player, elapsedTime);
        }
    }

    // Helper untuk cek tipe state (opsional, berguna untuk debug)
    // bool IsState(const std::string& stateName); 

private:
    PlayerState* currentState = nullptr;
};