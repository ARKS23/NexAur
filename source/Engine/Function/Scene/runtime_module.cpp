#include "pch.h"
#include "runtime_module.h"

#include "Core/Log/log_system.h"
#include "Core/Module/builtin_module_names.h"
#include "Core/Module/engine_module.h"
#include "Function/Scene/scene_manager.h"
#include "Function/Scene/scene_service.h"

namespace NexAur {
    namespace {
        class RuntimeModule final : public EngineModule {
        public:
            ModuleInfo getInfo() const override {
                return {
                    BuiltinModuleNames::Runtime,
                    ModuleStage::Runtime,
                    { BuiltinModuleNames::Resource, BuiltinModuleNames::Renderer }
                };
            }

            void initialize(ModuleContext& context) override {
                // RuntimeModule 拥有 SceneManager，对外暴露 SceneService。
                m_scene_manager = std::make_shared<SceneManager>();
                m_scene_manager->init();

                context.registry.registerService<SceneService>(std::static_pointer_cast<SceneService>(m_scene_manager));
                context.registry.registerService<SceneManager>(m_scene_manager);

                NX_CORE_INFO("Runtime module initialized.");
            }

            void shutdown(ModuleContext& context) override {
                if (m_scene_manager) {
                    m_scene_manager->shutdown();
                }

                context.registry.resetService<SceneService>();
                context.registry.resetService<SceneManager>();
                m_scene_manager.reset();
            }

        private:
            std::shared_ptr<SceneManager> m_scene_manager;
        };
    } // namespace

    std::unique_ptr<EngineModule> createRuntimeModule() {
        return std::make_unique<RuntimeModule>();
    }
} // namespace NexAur
