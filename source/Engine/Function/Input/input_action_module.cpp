#include "pch.h"
#include "input_action_module.h"

#include "Core/Log/log_system.h"
#include "Core/Module/builtin_module_names.h"
#include "Core/Module/engine_module.h"
#include "Function/Input/input_action_system.h"
#include "Function/Platform/platform_services.h"

namespace NexAur {
    namespace {
        class InputActionModule final : public EngineModule {
        public:
            ModuleInfo getInfo() const override {
                return {
                    BuiltinModuleNames::InputAction,
                    ModuleStage::Platform,
                    { BuiltinModuleNames::Platform }
                };
            }

            void initialize(ModuleContext& context) override {
                m_input_service = context.registry.getService<InputService>();
                m_input_actions = std::make_shared<InputActionSystem>(m_input_service);
                m_input_actions->configureDefaultBindings();

                context.registry.registerService<InputActionService>(
                    std::static_pointer_cast<InputActionService>(m_input_actions));
                context.registry.registerService<InputActionSystem>(m_input_actions);

                NX_CORE_INFO("InputAction module initialized.");
            }

            void tick(const TickContext& tick_context) override {
                (void)tick_context;
                if (m_input_actions) {
                    m_input_actions->update();
                }
            }

            void shutdown(ModuleContext& context) override {
                context.registry.resetService<InputActionService>();
                context.registry.resetService<InputActionSystem>();
                m_input_actions.reset();
                m_input_service.reset();
                NX_CORE_INFO("InputAction module shut down.");
            }

        private:
            std::shared_ptr<InputService> m_input_service;
            std::shared_ptr<InputActionSystem> m_input_actions;
        };
    } // namespace

    std::unique_ptr<EngineModule> createInputActionModule() {
        return std::make_unique<InputActionModule>();
    }
} // namespace NexAur
