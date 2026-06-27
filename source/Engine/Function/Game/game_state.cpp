#include "pch.h"
#include "game_state.h"

#include "Function/Scene/component.h"
#include "Function/Scene/gameplay_component.h"
#include "Function/Scene/scene_v2.h"

namespace NexAur {
    void GameStateModel::resetForScene(SceneV2& scene) {
        m_snapshot = GameStateSnapshot{};
        m_snapshot.state = GameState::Playing;
        refreshSceneSnapshot(scene);
    }

    void GameStateModel::togglePause() {
        if (!m_snapshot.has_gameplay_scene) {
            return;
        }

        if (m_snapshot.state == GameState::Playing) {
            m_snapshot.state = GameState::Paused;
        } else if (m_snapshot.state == GameState::Paused) {
            m_snapshot.state = GameState::Playing;
        }
    }

    void GameStateModel::addCollected(int count, int score) {
        if (count <= 0 && score <= 0) {
            return;
        }

        m_snapshot.score += std::max(0, score);
        m_snapshot.collectible_count = std::max(m_snapshot.collectible_count, m_snapshot.remaining_collectibles + count);
    }

    void GameStateModel::updateRules(SceneV2& scene) {
        const GameState previous_state = m_snapshot.state;
        refreshSceneSnapshot(scene);

        if (previous_state == GameState::Paused || previous_state == GameState::GameOver || previous_state == GameState::Victory) {
            m_snapshot.state = previous_state;
            return;
        }

        if (m_snapshot.has_player && m_snapshot.player_max_health > 0 && m_snapshot.player_health <= 0) {
            m_snapshot.state = GameState::GameOver;
            return;
        }

        if (m_snapshot.collectible_count > 0 && m_snapshot.remaining_collectibles == 0) {
            m_snapshot.state = GameState::Victory;
            return;
        }

        m_snapshot.state = GameState::Playing;
    }

    void GameStateModel::refreshSceneSnapshot(SceneV2& scene) {
        entt::registry& registry = scene.getRegistry();

        int current_collectibles = 0;
        auto collectible_view = registry.view<CollectibleComponent>();
        for (entt::entity entity : collectible_view) {
            (void)entity;
            current_collectibles++;
        }

        m_snapshot.remaining_collectibles = current_collectibles;
        m_snapshot.collectible_count = std::max(m_snapshot.collectible_count, current_collectibles);

        m_snapshot.has_player = false;
        m_snapshot.player_health = 0;
        m_snapshot.player_max_health = 0;

        auto player_view = registry.view<PlayerComponent>();
        for (entt::entity entity : player_view) {
            m_snapshot.has_player = true;
            if (const auto* health = registry.try_get<HealthComponent>(entity)) {
                m_snapshot.player_health = health->current;
                m_snapshot.player_max_health = health->max;
            }
            break;
        }

        m_snapshot.has_gameplay_scene = m_snapshot.has_player || m_snapshot.collectible_count > 0;
    }
} // namespace NexAur
