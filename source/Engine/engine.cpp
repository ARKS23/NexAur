#include "pch.h"
#include "engine.h"

#include "Core/Events/window_event.h"
#include "Core/Module/module_manager.h"
#include "Editor/editor_services.h"
#include "Function/Global/global_context.h"
#include "Function/Platform/platform_services.h"
#include "Function/Renderer/RHI/renderer_service.h"
#include "Function/Renderer/RHI/renderer_system.h"
#include "Function/Renderer/data/render_context.h"
#include "Function/Renderer/window_system.h"
#include "Function/Scene/scene_manager.h"
#include "Function/Scene/scene_service.h"
#include "Function/Scene/scene_v2.h"
#include "Function/UI/ui_system.h"

namespace NexAur {
    namespace {
        template<typename Service>
        std::shared_ptr<Service> getModuleService() {
            ModuleRegistry* registry = g_runtime_global_context.getModuleRegistry();
            return registry ? registry->getService<Service>() : nullptr;
        }
    } // namespace

    void Engine::startEngine() {
        g_runtime_global_context.startSystems();
        g_runtime_global_context.m_window_system->setEventCallback(NX_BIND_EVENT_FN(Engine::onEvent));

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
            module_manager->tickModules(TickContext{ delta_time });
        }

        logicalTick(delta_time);

        if (module_manager) {
            module_manager->postUpdateModules(TickContext{ delta_time });
        }

        std::shared_ptr<UIService> ui_service = getModuleService<UIService>();
        if (ui_service) {
            ui_service->beginFrame();
        } else if (g_runtime_global_context.m_ui_system) {
            g_runtime_global_context.m_ui_system->beginFrame();
        }

        if (module_manager) {
            module_manager->renderUIModules(TickContext{ delta_time });
        }

        std::shared_ptr<RenderContext> render_context = g_runtime_global_context.m_render_context;
        render_context->swapBuffers();

        rendererTick(delta_time);

        if (ui_service) {
            ui_service->endFrameAndRender();
        } else if (g_runtime_global_context.m_ui_system) {
            g_runtime_global_context.m_ui_system->endFrameAndRender();
        }

        std::shared_ptr<WindowService> window_service = getModuleService<WindowService>();
        if (window_service) {
            window_service->present();
            window_service->pollEvents();
            window_service->setTitle(std::string("NexAur: " + std::to_string(m_fps) + "FPS").c_str());
        } else if (g_runtime_global_context.m_window_system) {
            g_runtime_global_context.m_window_system->update();
            g_runtime_global_context.m_window_system->pollEvents();
            g_runtime_global_context.m_window_system->setTitle(std::string("NexAur: " + std::to_string(m_fps) + "FPS").c_str());
        }

        return m_is_running;
    }

    void Engine::logicalTick(TimeStep delta_time) {
        std::shared_ptr<SceneV2> active_scene;
        if (std::shared_ptr<SceneService> scene_service = getModuleService<SceneService>()) {
            active_scene = scene_service->getActiveScene();
        } else if (g_runtime_global_context.m_scene_manager) {
            active_scene = g_runtime_global_context.m_scene_manager->getActiveScene();
        }

        if (!active_scene) {
            return;
        }

        active_scene->tick(delta_time);

        std::shared_ptr<RenderContext> render_context = g_runtime_global_context.m_render_context;
        RenderDataPacket* write_packet = &render_context->getWriteData();
        active_scene->extractSceneData(write_packet);
    }

    void Engine::rendererTick(TimeStep delta_time) {
        const RenderDataPacket& render_data = g_runtime_global_context.m_render_context->getReadData();
        if (std::shared_ptr<RendererService> renderer_service = getModuleService<RendererService>()) {
            renderer_service->render(delta_time, render_data);
        } else if (g_runtime_global_context.m_renderer_system) {
            g_runtime_global_context.m_renderer_system->tick(delta_time, render_data);
        }
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
