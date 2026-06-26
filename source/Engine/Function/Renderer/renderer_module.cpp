#include "pch.h"
#include "renderer_module.h"

#include "Core/Log/log_system.h"
#include "Core/Module/builtin_module_names.h"
#include "Core/Module/engine_module.h"
#include "Function/Platform/platform_services.h"
#include "Function/Renderer/renderer_debug_service.h"
#include "Function/Renderer/renderer_service.h"
#include "Function/Renderer/data/render_context.h"
#include "Function/Platform/window_graphics_api.h"
#include "Function/Renderer/Vulkan/vulkan_renderer_system.h"

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
                std::shared_ptr<WindowService> window_service = context.registry.getService<WindowService>();
                NX_CORE_ASSERT(window_service, "RendererModule requires WindowService.");
                NX_CORE_ASSERT(window_service->getGraphicsAPI() == WindowGraphicsAPI::Vulkan, "RendererModule requires a Vulkan window.");

                m_vulkan_renderer_system = std::make_shared<VulkanRendererSystem>();
                NX_CORE_ASSERT(m_vulkan_renderer_system->init(*window_service), "Failed to initialize VulkanRendererSystem.");
                m_renderer_service = std::static_pointer_cast<RendererService>(m_vulkan_renderer_system);
                m_renderer_debug_service = std::static_pointer_cast<RendererDebugService>(m_vulkan_renderer_system);
                NX_CORE_INFO("Renderer module selected Vulkan backend.");

                if (m_renderer_service) {
                    context.registry.registerService<RendererService>(m_renderer_service);
                }
                if (m_renderer_debug_service) {
                    context.registry.registerService<RendererDebugService>(m_renderer_debug_service);
                }

                NX_CORE_INFO("Renderer module initialized.");
            }

            void shutdown(ModuleContext& context) override {
                if (m_vulkan_renderer_system) {
                    m_vulkan_renderer_system->shutdown();
                }

                context.registry.resetService<RendererService>();
                context.registry.resetService<RendererDebugService>();
                m_renderer_debug_service.reset();
                m_renderer_service.reset();
                m_vulkan_renderer_system.reset();
            }

            void onEvent(Event& event) override {
                if (m_vulkan_renderer_system) {
                    m_vulkan_renderer_system->onEvent(event);
                }
            }

        private:
            std::shared_ptr<RendererService> m_renderer_service;
            std::shared_ptr<RendererDebugService> m_renderer_debug_service;
            std::shared_ptr<VulkanRendererSystem> m_vulkan_renderer_system;
        };
    } // namespace

    std::unique_ptr<EngineModule> createRenderContextModule() {
        return std::make_unique<RenderContextModule>();
    }

    std::unique_ptr<EngineModule> createRendererModule() {
        return std::make_unique<RendererModule>();
    }
} // namespace NexAur
