#pragma once

#include "Core/Base.h"

namespace NexAur {
    class SceneV2;

    enum class GameState {
        MainMenu,
        Playing,
        Paused,
        GameOver,
        Victory
    };

    struct GameStateSnapshot {
        GameState state = GameState::Playing;
        int score = 0;
        int collectible_count = 0;
        int remaining_collectibles = 0;
        int player_health = 0;
        int player_max_health = 0;
        bool has_player = false;
        bool has_gameplay_scene = false;
    };

    class NEXAUR_API GameStateModel final {
    public:
        void resetForScene(SceneV2& scene);
        void togglePause();
        void addCollected(int count, int score);
        void updateRules(SceneV2& scene);

        bool isGameplayActive() const { return m_snapshot.state == GameState::Playing; }
        const GameStateSnapshot& getSnapshot() const { return m_snapshot; }

    private:
        void refreshSceneSnapshot(SceneV2& scene);

    private:
        GameStateSnapshot m_snapshot;
    };
} // namespace NexAur
