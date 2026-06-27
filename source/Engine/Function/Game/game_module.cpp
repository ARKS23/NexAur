#include "pch.h"
#include "game_module.h"

#include "Core/Log/log_system.h"
#include "Core/Module/builtin_module_names.h"
#include "Core/Module/engine_module.h"
#include "Function/Input/input_action_system.h"
#include "Function/Scene/scene_service.h"

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
                (void)tick_context;
                if (!m_is_enabled || !m_scene_service || !m_input_actions || !m_scene_service->getActiveScene()) {
                    return;
                }

                // Future gameplay systems will run here before RuntimeModule extracts render data.
            }

            void renderUI(const TickContext& tick_context) override {
                (void)tick_context;
                if (!m_is_enabled) {
                    return;
                }

                // Runtime HUD can be submitted here once PR-G8 introduces game state UI.
            }

            void shutdown(ModuleContext& context) override {
                (void)context;
                m_input_actions.reset();
                m_scene_service.reset();
                NX_CORE_INFO("Game module shut down.");
            }

        private:
            bool m_is_enabled = true;
            std::shared_ptr<SceneService> m_scene_service;
            std::shared_ptr<InputActionService> m_input_actions;
        };
    } // namespace

    std::unique_ptr<EngineModule> createGameModule() {
        return std::make_unique<GameModule>();
    }
} // namespace NexAur
