#include "pch.h"
#include "renderer_module.h"

#include "Core/Log/log_system.h"
#include "Core/Module/builtin_module_names.h"
#include "Core/Module/engine_module.h"
#include "Function/Renderer/RHI/renderer_service.h"
#include "Function/Renderer/RHI/renderer_system.h"
#include "Function/Renderer/data/render_context.h"

namespace NexAur {
    namespace {
        class RenderContextModule final : public EngineModule {
        public:
            ModuleInfo getInfo() const override {
                return { BuiltinModuleNames::RenderContext, ModuleStage::Renderer, { BuiltinModuleNames::Core } };
            }

            void initialize(ModuleContext& context) override {
                // RenderContext 管理双缓冲 RenderDataPacket，连接 Scene 数据提取和 Renderer 消费。
                m_render_context = std::make_shared<RenderContext>();
                context.registry.registerService<RenderContext>(m_render_context);
            }

            void shutdown(ModuleContext& context) override {
                context.registry.resetService<RenderContext>();
                m_render_context.reset();
            }

        private:
            std::shared_ptr<RenderContext> m_render_context;
        };

        class RendererModule final : public EngineModule {
        public:
            ModuleInfo getInfo() const override {
                return {
                    BuiltinModuleNames::Renderer,
                    ModuleStage::Renderer,
                    { BuiltinModuleNames::Platform, BuiltinModuleNames::Resource }
                };
            }

            void initialize(ModuleContext& context) override {
                // RendererSystem 是当前 OpenGL 实现；外部模块优先依赖 RendererService。
                m_renderer_system = std::make_shared<RendererSystem>();
                m_renderer_system->init();

                context.registry.registerService<RendererService>(std::static_pointer_cast<RendererService>(m_renderer_system));
                context.registry.registerService<RendererSystem>(m_renderer_system);

                NX_CORE_INFO("Renderer module initialized.");
            }

            void shutdown(ModuleContext& context) override {
                if (m_renderer_system) {
                    m_renderer_system->shutdown();
                }

                context.registry.resetService<RendererService>();
                context.registry.resetService<RendererSystem>();
                m_renderer_system.reset();
            }

            void onEvent(Event& event) override {
                if (m_renderer_system) {
                    m_renderer_system->onEvent(event);
                }
            }

        private:
            std::shared_ptr<RendererSystem> m_renderer_system;
        };
    } // namespace

    std::unique_ptr<EngineModule> createRenderContextModule() {
        return std::make_unique<RenderContextModule>();
    }

    std::unique_ptr<EngineModule> createRendererModule() {
        return std::make_unique<RendererModule>();
    }
} // namespace NexAur
