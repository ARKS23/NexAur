#include "pch.h"
#include "engine.h"

#include "Core/Events/window_event.h"
#include "Core/Module/module_manager.h"
#include "Editor/editor_services.h"
#include "Function/Global/global_context.h"
#include "Function/Platform/platform_services.h"
#include "Function/Renderer/RHI/renderer_service.h"
#include "Function/Renderer/data/render_context.h"
#include "Function/Scene/scene_service.h"
#include "Function/Scene/scene_v2.h"
#include "Function/UI/ui_system.h"

namespace NexAur {
    namespace {
        // Engine 位于顶层调度，可以在迁移期从 ModuleRegistry 取服务。
        // 深层系统不要复制这种访问方式，应该通过构造参数或自己的 Context 拿依赖。
        template<typename Service>
        std::shared_ptr<Service> getModuleService() {
            ModuleRegistry* registry = g_runtime_global_context.getModuleRegistry();
            return registry ? registry->getService<Service>() : nullptr;
        }

        template<typename Service>
        std::shared_ptr<Service> getRequiredModuleService() {
            std::shared_ptr<Service> service = getModuleService<Service>();
            NX_CORE_ASSERT(service, "Required module service is not registered.");
            return service;
        }
    } // namespace

    void Engine::startEngine() {
        g_runtime_global_context.startSystems();
        getRequiredModuleService<WindowService>()->setEventCallback(NX_BIND_EVENT_FN(Engine::onEvent));

        enableEditorMode(m_is_editor_mode);

        NX_CORE_INFO("NexAur Engine started.");
    }

    void Engine::shutdownEngine() {
        g_runtime_global_context.shutdownSystems();
        NX_CORE_INFO("NexAur Engine shut down.");
    }

    void Engine::enableEditorMode(bool enable) {
        m_is_editor_mode = enable;

        if (std::shared_ptr<EditorService> editor_service = getModuleService<EditorService>()) {
            editor_service->setEnabled(enable);
        }
    }

    void Engine::run() {
        while (m_is_running) {
            TimeStep delta_time = calculateDeltaTime();
            tickOneFrame(delta_time);
        }
    }

    bool Engine::tickOneFrame(TimeStep delta_time) {
        calculateFPS(delta_time);

        ModuleManager* module_manager = g_runtime_global_context.getModuleManager();
        if (module_manager) {
            // 帧早期模块更新：当前主要让 PlatformModule 刷新 InputState。
            module_manager->tickModules(TickContext{ delta_time });
        }

        // Runtime 场景逻辑仍暂时由 Engine 顶层调度，后续可以继续下沉到 RuntimeModule。
        logicalTick(delta_time);

        if (module_manager) {
            // Scene 提取 RenderData 后，Editor 可以覆盖 viewport 相机等编辑器态数据。
            module_manager->postUpdateModules(TickContext{ delta_time });
        }

        std::shared_ptr<UIService> ui_service = getRequiredModuleService<UIService>();
        ui_service->beginFrame();

        if (module_manager) {
            // EditorModule/GameModule 等模块在这里提交 UI。
            module_manager->renderUIModules(TickContext{ delta_time });
        }

        ui_service->finalizeFrame();

        // 双缓冲交换：本帧写入的数据变成 Renderer 读取的数据。
        std::shared_ptr<RenderContext> render_context = getRequiredModuleService<RenderContext>();
        render_context->swapBuffers();

        rendererTick(delta_time);

        std::shared_ptr<WindowService> window_service = getRequiredModuleService<WindowService>();
        // Vulkan renderer 负责 swapchain present；WindowService 这里只处理平台事件。
        window_service->present();
        window_service->pollEvents();
        window_service->setTitle(std::string("NexAur: " + std::to_string(m_fps) + "FPS").c_str());

        return m_is_running;
    }

    void Engine::logicalTick(TimeStep delta_time) {
        std::shared_ptr<SceneV2> active_scene = getRequiredModuleService<SceneService>()->getActiveScene();
        if (!active_scene) {
            return;
        }

        active_scene->tick(delta_time);

        // Scene 只提取轻量 RenderDataPacket；GPU 资源解析交给 RendererModule。
        std::shared_ptr<RenderContext> render_context = getRequiredModuleService<RenderContext>();
        RenderDataPacket* write_packet = &render_context->getWriteData();
        active_scene->extractSceneData(write_packet);
    }

    void Engine::rendererTick(TimeStep delta_time) {
        const RenderDataPacket& render_data = getRequiredModuleService<RenderContext>()->getReadData();
        getRequiredModuleService<RendererService>()->render(delta_time, render_data);
    }

    void Engine::calculateFPS(TimeStep delta_time) {
        m_frame_count_accumulator++;
        m_fps_timer += delta_time;

        if (m_fps_timer.GetSeconds() >= 1.0) {
            m_fps = m_frame_count_accumulator;
            m_frame_count_accumulator = 0;
            m_fps_timer.reset();
        }
    }

    TimeStep Engine::calculateDeltaTime() {
        return m_clock.restart();
    }

    void Engine::onEvent(Event& event) {
        EventDispatcher dispatcher(event);
        // Engine 只先处理应用生命周期事件；普通输入交给模块事件路由。
        dispatcher.dispatch<WindowCloseEvent>(NX_BIND_EVENT_FN(Engine::onWindowClose));
        dispatcher.dispatch<WindowResizeEvent>(NX_BIND_EVENT_FN(Engine::onWindowResize));

        if (event.handled) {
            return;
        }

        if (ModuleManager* module_manager = g_runtime_global_context.getModuleManager()) {
            module_manager->dispatchEvent(event);
        }
    }

    bool Engine::onWindowClose(WindowCloseEvent& event) {
        (void)event;
        NX_CORE_INFO("Window close event received. Shutting down engine.");
        m_is_running = false;
        return true;
    }

    bool Engine::onWindowResize(WindowResizeEvent& event) {
        NX_CORE_INFO("Window resize event received: {}x{}", event.getWidth(), event.getHeight());
        return false;
    }
} // namespace NexAur
