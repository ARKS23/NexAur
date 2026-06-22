#include "pch.h"
#include "global_context.h"

#include "Editor/editor_layer.h"
#include "Editor/editor_services.h"
#include "Core/Module/module_manager.h"
#include "Core/Log/log_system.h"
#include "Function/File/file_system.h"
#include "Function/Input/input_system_glfw.h"
#include "Function/Platform/platform_services.h"
#include "Function/Renderer/RHI/renderer_system.h"
#include "Function/Renderer/RHI/renderer_service.h"
#include "Function/Renderer/data/render_context.h"
#include "Function/Renderer/window_system.h"
#include "Function/Resource/asset_manager.h"
#include "Function/Scene/scene_service.h"
#include "Function/Scene/scene_manager.h"
#include "Function/UI/ui_system.h"

#ifndef ENGINE_ROOT_DIR
#define ENGINE_ROOT_DIR "."
#endif

namespace NexAur {
    RunTimeGlobalContext g_runtime_global_context;

    namespace {
        // 当前阶段使用静态模块名声明依赖关系。后续如果拆成独立 module 文件，
        // 这些名字应当继续保持稳定，因为依赖拓扑排序依赖它们。
        constexpr const char* kCoreModule = "Core";
        constexpr const char* kFileSystemModule = "FileSystem";
        constexpr const char* kPlatformModule = "Platform";
        constexpr const char* kResourceModule = "Resource";
        constexpr const char* kRenderContextModule = "RenderContext";
        constexpr const char* kRendererModule = "Renderer";
        constexpr const char* kRuntimeModule = "Runtime";
        constexpr const char* kUIModule = "UI";
        constexpr const char* kEditorModule = "Editor";

        // Core 最先启动，提供日志等基础能力。
        class CoreEngineModule final : public EngineModule {
        public:
            ModuleInfo getInfo() const override {
                return { kCoreModule, ModuleStage::Core, {} };
            }

            void initialize(ModuleContext& context) override {
                (void)context;
                LogSystem::init();
                NX_CORE_INFO("Core module initialized.");
            }
        };

        class FileSystemModule final : public EngineModule {
        public:
            explicit FileSystemModule(RunTimeGlobalContext& owner) : m_owner(owner) {}

            ModuleInfo getInfo() const override {
                return { kFileSystemModule, ModuleStage::Core, { kCoreModule } };
            }

            void initialize(ModuleContext& context) override {
                m_file_system = std::make_shared<FileSystem>(ENGINE_ROOT_DIR);
                context.registry.registerService<FileSystem>(m_file_system);
                // 兼容桥：旧代码仍可能从 g_runtime_global_context 取 FileSystem。
                m_owner.m_file_system = m_file_system;
                NX_CORE_INFO("FileSystem module initialized.");
            }

            void shutdown(ModuleContext& context) override {
                if (m_owner.m_file_system == m_file_system) {
                    m_owner.m_file_system.reset();
                }
                context.registry.resetService<FileSystem>();
                m_file_system.reset();
            }

        private:
            RunTimeGlobalContext& m_owner;
            std::shared_ptr<FileSystem> m_file_system;
        };

        class WindowSystemService final : public WindowService {
        public:
            explicit WindowSystemService(std::shared_ptr<WindowSystem> window_system)
                : m_window_system(std::move(window_system)) {}

            void* getNativeWindow() const override {
                return m_window_system ? m_window_system->getNativeWindow() : nullptr;
            }

            std::pair<uint32_t, uint32_t> getSize() const override {
                if (!m_window_system) {
                    return { 0, 0 };
                }

                auto [width, height] = m_window_system->getWindowSize();
                return { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
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
            explicit PlatformModule(RunTimeGlobalContext& owner) : m_owner(owner) {}

            ModuleInfo getInfo() const override {
                return { kPlatformModule, ModuleStage::Platform, { kCoreModule } };
            }

            void initialize(ModuleContext& context) override {
                // PlatformModule 拥有窗口和输入实现，对外只暴露 WindowService/InputService。
                m_window_system = std::make_shared<WindowSystem>();
                WindowSpecification specification;
                m_window_system->init(specification);

                m_window_service = std::make_shared<WindowSystemService>(m_window_system);
                m_input_system = std::make_shared<InputSystemGLFW>(m_window_system->getNativeWindow());
                m_input_service = std::static_pointer_cast<InputService>(m_input_system);
                m_input_system->update();

                context.registry.registerService<WindowService>(m_window_service);
                context.registry.registerService<WindowSystem>(m_window_system);
                context.registry.registerService<InputService>(m_input_service);
                context.registry.registerService<InputSystem>(m_input_system);
                // 兼容桥：旧路径暂时保留，后续新代码优先从 ModuleRegistry 取服务。
                m_owner.m_window_system = m_window_system;
                m_owner.m_input_system = m_input_system;

                NX_CORE_INFO("Platform module initialized.");
            }

            void shutdown(ModuleContext& context) override {
                if (m_owner.m_input_system == m_input_system) {
                    m_owner.m_input_system.reset();
                }
                context.registry.resetService<InputService>();
                context.registry.resetService<InputSystem>();
                m_input_service.reset();
                m_input_system.reset();

                if (m_window_system) {
                    m_window_system->shutdown();
                }

                if (m_owner.m_window_system == m_window_system) {
                    m_owner.m_window_system.reset();
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
            RunTimeGlobalContext& m_owner;
            std::shared_ptr<WindowSystem> m_window_system;
            std::shared_ptr<WindowService> m_window_service;
            std::shared_ptr<InputSystem> m_input_system;
            std::shared_ptr<InputService> m_input_service;
        };

        class ResourceModule final : public EngineModule {
        public:
            ModuleInfo getInfo() const override {
                return { kResourceModule, ModuleStage::Resource, { kCoreModule, kFileSystemModule } };
            }

            void initialize(ModuleContext& context) override {
                // AssetManager 目前仍是单例，这里用不拥有删除权的 shared_ptr 作为过渡服务视图。
                AssetManager::getInstance().init();
                m_asset_manager = std::shared_ptr<AssetManager>(&AssetManager::getInstance(), [](AssetManager*) {});
                context.registry.registerService<AssetManager>(m_asset_manager);
            }

            void shutdown(ModuleContext& context) override {
                context.registry.resetService<AssetManager>();
                m_asset_manager.reset();
                AssetManager::getInstance().shutdown();
            }

        private:
            std::shared_ptr<AssetManager> m_asset_manager;
        };

        class RenderContextModule final : public EngineModule {
        public:
            explicit RenderContextModule(RunTimeGlobalContext& owner) : m_owner(owner) {}

            ModuleInfo getInfo() const override {
                return { kRenderContextModule, ModuleStage::Renderer, { kCoreModule } };
            }

            void initialize(ModuleContext& context) override {
                // RenderContext 管理双缓冲 RenderDataPacket，连接 Scene 数据提取和 Renderer 消费。
                m_render_context = std::make_shared<RenderContext>();
                context.registry.registerService<RenderContext>(m_render_context);
                m_owner.m_render_context = m_render_context;
            }

            void shutdown(ModuleContext& context) override {
                if (m_owner.m_render_context == m_render_context) {
                    m_owner.m_render_context.reset();
                }
                context.registry.resetService<RenderContext>();
                m_render_context.reset();
            }

        private:
            RunTimeGlobalContext& m_owner;
            std::shared_ptr<RenderContext> m_render_context;
        };

        class RendererModule final : public EngineModule {
        public:
            explicit RendererModule(RunTimeGlobalContext& owner) : m_owner(owner) {}

            ModuleInfo getInfo() const override {
                return { kRendererModule, ModuleStage::Renderer, { kPlatformModule, kResourceModule } };
            }

            void initialize(ModuleContext& context) override {
                // RendererSystem 是当前 OpenGL 实现；外部模块优先依赖 RendererService。
                m_renderer_system = std::make_shared<RendererSystem>();
                m_renderer_system->init();

                context.registry.registerService<RendererService>(std::static_pointer_cast<RendererService>(m_renderer_system));
                context.registry.registerService<RendererSystem>(m_renderer_system);
                m_owner.m_renderer_system = m_renderer_system;

                NX_CORE_INFO("Renderer module initialized.");
            }

            void shutdown(ModuleContext& context) override {
                if (m_renderer_system) {
                    m_renderer_system->shutdown();
                }

                if (m_owner.m_renderer_system == m_renderer_system) {
                    m_owner.m_renderer_system.reset();
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
            RunTimeGlobalContext& m_owner;
            std::shared_ptr<RendererSystem> m_renderer_system;
        };

        class RuntimeModule final : public EngineModule {
        public:
            explicit RuntimeModule(RunTimeGlobalContext& owner) : m_owner(owner) {}

            ModuleInfo getInfo() const override {
                return { kRuntimeModule, ModuleStage::Runtime, { kResourceModule, kRendererModule } };
            }

            void initialize(ModuleContext& context) override {
                // RuntimeModule 拥有 SceneManager，对外暴露 SceneService。
                m_scene_manager = std::make_shared<SceneManager>();
                m_scene_manager->init();

                context.registry.registerService<SceneService>(std::static_pointer_cast<SceneService>(m_scene_manager));
                context.registry.registerService<SceneManager>(m_scene_manager);
                m_owner.m_scene_manager = m_scene_manager;

                NX_CORE_INFO("Runtime module initialized.");
            }

            void shutdown(ModuleContext& context) override {
                if (m_scene_manager) {
                    m_scene_manager->shutdown();
                }

                if (m_owner.m_scene_manager == m_scene_manager) {
                    m_owner.m_scene_manager.reset();
                }
                context.registry.resetService<SceneService>();
                context.registry.resetService<SceneManager>();
                m_scene_manager.reset();
            }

        private:
            RunTimeGlobalContext& m_owner;
            std::shared_ptr<SceneManager> m_scene_manager;
        };

        class UIModule final : public EngineModule {
        public:
            explicit UIModule(RunTimeGlobalContext& owner) : m_owner(owner) {}

            ModuleInfo getInfo() const override {
                return { kUIModule, ModuleStage::UI, { kPlatformModule, kRendererModule } };
            }

            void initialize(ModuleContext& context) override {
                // UI 需要在事件路由中查询 ViewportService，因此保存 registry 指针。
                // 这里仍是模块顶层使用，不把 registry 继续传入深层业务对象。
                m_registry = &context.registry;
                m_ui_system = std::make_shared<UISystem>();
                m_ui_system->init();

                context.registry.registerService<UIService>(std::static_pointer_cast<UIService>(m_ui_system));
                context.registry.registerService<UISystem>(m_ui_system);
                m_owner.m_ui_system = m_ui_system;

                NX_CORE_INFO("UI module initialized.");
            }

            void onEvent(Event& event) override {
                if (!m_ui_system) {
                    return;
                }

                m_ui_system->onEvent(event);
                // ImGui 捕获输入时，在这里截断后续 Editor/Runtime 输入响应。
                if (shouldBlockInputForUICapture(event)) {
                    event.handled = true;
                }
            }

            void shutdown(ModuleContext& context) override {
                if (m_ui_system) {
                    m_ui_system->shutdown();
                }

                if (m_owner.m_ui_system == m_ui_system) {
                    m_owner.m_ui_system.reset();
                }
                context.registry.resetService<UIService>();
                context.registry.resetService<UISystem>();
                m_ui_system.reset();
                m_registry = nullptr;
            }

        private:
            bool shouldBlockInputForUICapture(Event& event) const {
                const bool keyboard_event = event.isInCategory(EventCategoryKeyboard);
                const bool mouse_event =
                    event.isInCategory(EventCategoryMouse) ||
                    event.isInCategory(EventCategoryMouseButton);

                if (keyboard_event && (m_ui_system->wantsCaptureKeyboard() || m_ui_system->wantsTextInput())) {
                    return true;
                }

                if (mouse_event && m_ui_system->wantsCaptureMouse()) {
                    std::shared_ptr<ViewportService> viewport_service =
                        m_registry ? m_registry->getService<ViewportService>() : nullptr;
                    // 鼠标在 viewport 上时允许编辑器视口继续处理 picking/滚轮等交互。
                    const bool mouse_over_viewport = viewport_service && viewport_service->isViewportHovered();
                    return !mouse_over_viewport;
                }

                return false;
            }

        private:
            RunTimeGlobalContext& m_owner;
            ModuleRegistry* m_registry = nullptr;
            std::shared_ptr<UISystem> m_ui_system;
        };

        class EditorModule final : public EngineModule {
        public:
            ModuleInfo getInfo() const override {
                return { kEditorModule, ModuleStage::Editor, { kUIModule, kRuntimeModule, kRendererModule, kPlatformModule } };
            }

            void initialize(ModuleContext& context) override {
                // EditorContext 是编辑器层的小上下文，只保存编辑器实际需要的窄服务。
                // 这样 Panel/EditorLayer 不需要直接访问 RunTimeGlobalContext。
                m_context = std::make_shared<EditorContext>();
                m_context->scene_service = context.registry.getService<SceneService>();
                m_context->renderer_system = context.registry.getService<RendererSystem>();
                m_context->renderer_service = context.registry.getService<RendererService>();
                m_context->input_service = context.registry.getService<InputService>();
                m_context->render_context = context.registry.getService<RenderContext>();
                m_context->ui_service = context.registry.getService<UIService>();
                m_context->active_scene = m_context->scene_service ? m_context->scene_service->getActiveScene() : nullptr;

                m_editor_layer = std::make_shared<EditorLayer>(m_context);
                // SelectionService 由 EditorLayer 实现，Panel 通过服务写选择状态。
                m_context->selection_service = std::static_pointer_cast<SelectionService>(m_editor_layer);

                context.registry.registerService<EditorService>(std::static_pointer_cast<EditorService>(m_editor_layer));
                context.registry.registerService<SelectionService>(std::static_pointer_cast<SelectionService>(m_editor_layer));
                context.registry.registerService<ViewportService>(std::static_pointer_cast<ViewportService>(m_editor_layer));

                NX_CORE_INFO("Editor module initialized.");
            }

            void postUpdate(const TickContext& tick_context) override {
                // 放在 postUpdate 是为了让 Scene 先提取渲染数据，
                // 然后 EditorCamera 再覆盖 viewport 预览相机。
                if (m_editor_layer && m_editor_layer->isEnabled()) {
                    m_editor_layer->update(tick_context.delta_time);
                }
            }

            void renderUI(const TickContext& tick_context) override {
                (void)tick_context;
                if (m_editor_layer && m_editor_layer->isEnabled()) {
                    m_editor_layer->renderUI();
                }
            }

            void onEvent(Event& event) override {
                if (m_editor_layer && m_editor_layer->isEnabled()) {
                    m_editor_layer->onEvent(event);
                }
            }

            void shutdown(ModuleContext& context) override {
                context.registry.resetService<ViewportService>();
                context.registry.resetService<SelectionService>();
                context.registry.resetService<EditorService>();
                m_editor_layer.reset();
                m_context.reset();
            }

        private:
            std::shared_ptr<EditorContext> m_context;
            std::shared_ptr<EditorLayer> m_editor_layer;
        };
    } // namespace

    RunTimeGlobalContext::RunTimeGlobalContext() = default;

    RunTimeGlobalContext::~RunTimeGlobalContext() = default;

    void RunTimeGlobalContext::startSystems() {
        if (m_module_manager) {
            return;
        }

        clearCompatibilityServices();

        // RunTimeGlobalContext 现在只负责组合模块和保留旧 public 指针兼容视图。
        // 新模块代码不要继续把它当成万能 Service Locator。
        m_module_manager = std::make_unique<ModuleManager>();
        m_module_manager->registerModule(std::make_unique<CoreEngineModule>());
        m_module_manager->registerModule(std::make_unique<FileSystemModule>(*this));
        m_module_manager->registerModule(std::make_unique<PlatformModule>(*this));
        m_module_manager->registerModule(std::make_unique<ResourceModule>());
        m_module_manager->registerModule(std::make_unique<RenderContextModule>(*this));
        m_module_manager->registerModule(std::make_unique<RendererModule>(*this));
        m_module_manager->registerModule(std::make_unique<RuntimeModule>(*this));
        m_module_manager->registerModule(std::make_unique<UIModule>(*this));
        m_module_manager->registerModule(std::make_unique<EditorModule>());
        m_module_manager->initializeModules();
    }

    void RunTimeGlobalContext::shutdownSystems() {
        if (m_module_manager) {
            // 具体关闭顺序交给 ModuleManager 根据初始化顺序反向执行。
            m_module_manager->shutdownModules();
            m_module_manager.reset();
        }

        clearCompatibilityServices();
    }

    ModuleManager* RunTimeGlobalContext::getModuleManager() {
        return m_module_manager.get();
    }

    const ModuleManager* RunTimeGlobalContext::getModuleManager() const {
        return m_module_manager.get();
    }

    ModuleRegistry* RunTimeGlobalContext::getModuleRegistry() {
        return m_module_manager ? &m_module_manager->getRegistry() : nullptr;
    }

    const ModuleRegistry* RunTimeGlobalContext::getModuleRegistry() const {
        return m_module_manager ? &m_module_manager->getRegistry() : nullptr;
    }

    void RunTimeGlobalContext::clearCompatibilityServices() {
        m_ui_system.reset();
        m_scene_manager.reset();
        m_renderer_system.reset();
        m_render_context.reset();
        m_input_system.reset();
        m_window_system.reset();
        m_file_system.reset();
    }
} // namespace NexAur
