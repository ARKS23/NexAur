#include "pch.h"
#include "runtime_module.h"

#include "Core/Log/log_system.h"
#include "Core/Module/builtin_module_names.h"
#include "Core/Module/engine_module.h"
#include "Function/Renderer/data/render_context.h"
#include "Function/Scene/scene_manager.h"
#include "Function/Scene/scene_service.h"
#include "Function/Scene/scene_v2.h"

namespace NexAur {
    namespace {
        class RuntimeModule final : public EngineModule {
        public:
            ModuleInfo getInfo() const override {
                return {
                    BuiltinModuleNames::Runtime,
                    ModuleStage::Runtime,
                    { BuiltinModuleNames::Resource, BuiltinModuleNames::RenderContext }
                };
            }

            void initialize(ModuleContext& context) override {
                // RuntimeModule 拥有 SceneManager，对外暴露 SceneService。
                m_scene_manager = std::make_shared<SceneManager>();
                m_scene_manager->init();
                m_render_context = context.registry.getService<RenderContext>();

                context.registry.registerService<SceneService>(std::static_pointer_cast<SceneService>(m_scene_manager));
                context.registry.registerService<SceneManager>(m_scene_manager);

                NX_CORE_INFO("Runtime module initialized.");
            }

            void tick(const TickContext& tick_context) override {
                if (m_scene_manager) {
                    m_scene_manager->tick(tick_context.delta_time);
                }
            }

            void postUpdate(const TickContext& tick_context) override {
                (void)tick_context;
                extractActiveScene();
            }

            void shutdown(ModuleContext& context) override {
                if (m_scene_manager) {
                    m_scene_manager->shutdown();
                }

                context.registry.resetService<SceneService>();
                context.registry.resetService<SceneManager>();
                m_render_context.reset();
                m_scene_manager.reset();
            }

        private:
            void extractActiveScene() {
                if (!m_scene_manager || !m_render_context) {
                    return;
                }

                std::shared_ptr<SceneV2> active_scene = m_scene_manager->getActiveScene();
                if (!active_scene) {
                    return;
                }

                RenderDataPacket& write_packet = m_render_context->getWriteData();
                write_packet.debug_visualization_options = m_render_context->getDebugVisualizationOptions();
                active_scene->extractSceneData(&write_packet);
            }

            std::shared_ptr<SceneManager> m_scene_manager;
            std::shared_ptr<RenderContext> m_render_context;
        };
    } // namespace

    std::unique_ptr<EngineModule> createRuntimeModule() {
        return std::make_unique<RuntimeModule>();
    }
} // namespace NexAur
