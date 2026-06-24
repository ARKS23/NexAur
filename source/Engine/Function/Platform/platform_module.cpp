#include "pch.h"
#include "platform_module.h"

#include "Core/Log/log_system.h"
#include "Core/Module/builtin_module_names.h"
#include "Core/Module/engine_module.h"
#include "Function/Input/input_system_glfw.h"
#include "Function/Platform/platform_services.h"
#include "Function/Renderer/window_system.h"

namespace NexAur {
    namespace {
        WindowGraphicsAPI resolveDefaultWindowGraphicsAPI() {
            return WindowGraphicsAPI::Vulkan;
        }

        class WindowSystemService final : public WindowService {
        public:
            explicit WindowSystemService(std::shared_ptr<WindowSystem> window_system)
                : m_window_system(std::move(window_system)) {}

            void* getNativeWindow() const override {
                return m_window_system ? m_window_system->getNativeWindow() : nullptr;
            }

            WindowGraphicsAPI getGraphicsAPI() const override {
                return m_window_system ? m_window_system->getGraphicsAPI() : WindowGraphicsAPI::Vulkan;
            }

            std::vector<const char*> getRequiredVulkanInstanceExtensions() const override {
                return m_window_system ? m_window_system->getRequiredVulkanInstanceExtensions() : std::vector<const char*>{};
            }

            std::pair<uint32_t, uint32_t> getSize() const override {
                if (!m_window_system) {
                    return { 0, 0 };
                }

                auto [width, height] = m_window_system->getWindowSize();
                return { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
            }

            void setEventCallback(const EventCallbackFn& callback) override {
                if (m_window_system) {
                    m_window_system->setEventCallback(callback);
                }
            }

            void setTitle(const char* title) override {
                if (m_window_system) {
                    m_window_system->setTitle(title);
                }
            }

            void present() override {
                if (m_window_system) {
                    m_window_system->update();
                }
            }

            void pollEvents() override {
                if (m_window_system) {
                    m_window_system->pollEvents();
                }
            }

        private:
            std::shared_ptr<WindowSystem> m_window_system;
        };

        class PlatformModule final : public EngineModule {
        public:
            ModuleInfo getInfo() const override {
                return { BuiltinModuleNames::Platform, ModuleStage::Platform, { BuiltinModuleNames::Core } };
            }

            void initialize(ModuleContext& context) override {
                // PlatformModule 拥有窗口和输入实现，对外只暴露 WindowService/InputService。
                m_window_system = std::make_shared<WindowSystem>();
                WindowSpecification specification;
                specification.graphics_api = resolveDefaultWindowGraphicsAPI();
                m_window_system->init(specification);
                NX_CORE_INFO("Platform window graphics API: {}", toString(specification.graphics_api));

                m_window_service = std::make_shared<WindowSystemService>(m_window_system);
                m_input_system = std::make_shared<InputSystemGLFW>(m_window_system->getNativeWindow());
                m_input_service = std::static_pointer_cast<InputService>(m_input_system);
                m_input_system->update();

                context.registry.registerService<WindowService>(m_window_service);
                context.registry.registerService<WindowSystem>(m_window_system);
                context.registry.registerService<InputService>(m_input_service);
                context.registry.registerService<InputSystem>(m_input_system);

                NX_CORE_INFO("Platform module initialized.");
            }

            void shutdown(ModuleContext& context) override {
                context.registry.resetService<InputService>();
                context.registry.resetService<InputSystem>();
                m_input_service.reset();
                m_input_system.reset();

                if (m_window_system) {
                    m_window_system->shutdown();
                }

                context.registry.resetService<WindowService>();
                context.registry.resetService<WindowSystem>();
                m_window_service.reset();
                m_window_system.reset();
            }

            void tick(const TickContext& tick_context) override {
                (void)tick_context;
                if (m_input_system) {
                    m_input_system->update();
                }
            }

        private:
            std::shared_ptr<WindowSystem> m_window_system;
            std::shared_ptr<WindowService> m_window_service;
            std::shared_ptr<InputSystem> m_input_system;
            std::shared_ptr<InputService> m_input_service;
        };
    } // namespace

    std::unique_ptr<EngineModule> createPlatformModule() {
        return std::make_unique<PlatformModule>();
    }
} // namespace NexAur
