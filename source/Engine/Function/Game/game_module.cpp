#include "pch.h"
#include "game_module.h"

#include "Core/Log/log_system.h"
#include "Core/Module/builtin_module_names.h"
#include "Core/Module/engine_module.h"
#include "Function/Game/game_state.h"
#include "Function/Game/gameplay_systems.h"
#include "Function/Game/runtime_camera_controller_system.h"
#include "Function/Game/runtime_hud.h"
#include "Function/Input/input_action_system.h"
#include "Function/Physics/trigger_overlap_system.h"
#include "Function/Scene/scene_service.h"
#include "Function/Scene/scene_v2.h"

namespace NexAur {
    namespace {
        class GameModule final : public EngineModule {
        public:
            ModuleInfo getInfo() const override {
                return {
                    BuiltinModuleNames::Game,
                    ModuleStage::Game,
                    { BuiltinModuleNames::Runtime, BuiltinModuleNames::InputAction }
                };
            }

            void initialize(ModuleContext& context) override {
                m_scene_service = context.registry.getService<SceneService>();
                m_input_actions = context.registry.getService<InputActionService>();
                NX_CORE_INFO("Game module initialized.");
            }

            void tick(const TickContext& tick_context) override {
                if (!m_is_enabled || !m_scene_service || !m_input_actions) {
                    return;
                }

                std::shared_ptr<SceneV2> active_scene = m_scene_service->getActiveScene();
                if (!active_scene) {
                    m_tracked_scene.reset();
                    return;
                }

                if (active_scene != m_tracked_scene) {
                    m_tracked_scene = active_scene;
                    m_game_state.resetForScene(*active_scene);
                }

                SceneV2& scene = *active_scene;
                if (m_input_actions->wasPressed(DefaultInputActions::Pause)) {
                    m_game_state.togglePause();
                }

                m_game_state.updateRules(scene);
                if (!m_game_state.isGameplayActive()) {
                    m_runtime_camera_controller_system.update(scene, tick_context.delta_time);
                    return;
                }

                m_player_control_system.update(scene, *m_input_actions, tick_context.delta_time);
                m_enemy_system.update(scene, tick_context.delta_time);
                m_movement_system.update(scene, tick_context.delta_time);
                m_trigger_overlap_system.update(scene);
                const CollectibleFrameResult collectible_result =
                    m_collectible_system.update(scene, m_trigger_overlap_system.getFrame());
                m_game_state.addCollected(collectible_result.collected_count, collectible_result.collected_score);
                m_game_state.updateRules(scene);
                m_runtime_camera_controller_system.update(scene, tick_context.delta_time);

                if (m_game_state.isGameplayActive()) {
                    m_lifetime_system.update(scene, tick_context.delta_time);
                    m_health_system.update(scene);
                }
            }

            void renderUI(const TickContext& tick_context) override {
                (void)tick_context;
                if (!m_is_enabled) {
                    return;
                }

                m_runtime_hud.draw(m_game_state.getSnapshot());
            }

            void shutdown(ModuleContext& context) override {
                (void)context;
                m_tracked_scene.reset();
                m_input_actions.reset();
                m_scene_service.reset();
                NX_CORE_INFO("Game module shut down.");
            }

        private:
            bool m_is_enabled = true;
            std::shared_ptr<SceneService> m_scene_service;
            std::shared_ptr<InputActionService> m_input_actions;
            std::shared_ptr<SceneV2> m_tracked_scene;
            GameStateModel m_game_state;
            PlayerControlSystem m_player_control_system;
            EnemySystem m_enemy_system;
            MovementSystem m_movement_system;
            TriggerOverlapSystem m_trigger_overlap_system;
            CollectibleSystem m_collectible_system;
            LifetimeSystem m_lifetime_system;
            HealthSystem m_health_system;
            RuntimeCameraControllerSystem m_runtime_camera_controller_system;
            RuntimeHud m_runtime_hud;
        };
    } // namespace

    std::unique_ptr<EngineModule> createGameModule() {
        return std::make_unique<GameModule>();
    }
} // namespace NexAur
