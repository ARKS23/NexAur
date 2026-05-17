#include "pch.h"
#include "global_context.h"

#include "Core/Log/log_system.h"
#include "Function/File/file_system.h"
#include "Function/Input/input_system_glfw.h"
#include "Function/Renderer/RHI/renderer_system.h"
#include "Function/Renderer/data/render_context.h"
#include "Function/Renderer/window_system.h"
#include "Function/Resource/asset_manager.h"
#include "Function/Scene/scene_manager.h"
#include "Function/UI/ui_system.h"

#ifndef ENGINE_ROOT_DIR
#define ENGINE_ROOT_DIR "."
#endif

namespace NexAur {
    RunTimeGlobalContext g_runtime_global_context;

    void RunTimeGlobalContext::startSystems() {
        LogSystem::init();

        m_file_system = std::make_shared<FileSystem>(ENGINE_ROOT_DIR);

        m_window_system = std::make_shared<WindowSystem>();
        WindowSpecification specification;
        m_window_system->init(specification);

        m_input_system = std::make_shared<InputSystemGLFW>();
        m_render_context = std::make_shared<RenderContext>();

        // Scene initialization may load GPU-backed assets, so the renderer must be ready first.
        m_renderer_system = std::make_shared<RendererSystem>();
        m_renderer_system->init();

        AssetManager::getInstance().init();

        m_scene_manager = std::make_shared<SceneManager>();
        m_scene_manager->init();

        m_ui_system = std::make_shared<UISystem>();
        m_ui_system->init();
    }

    void RunTimeGlobalContext::shutdownSystems() {
        // Release GL-backed resources before destroying the window/context.
        if (m_ui_system) {
            m_ui_system->shutdown();
        }
        m_ui_system.reset();

        if (m_scene_manager) {
            m_scene_manager->shutdown();
        }
        m_scene_manager.reset();

        AssetManager::getInstance().shutdown();

        if (m_renderer_system) {
            m_renderer_system->shutdown();
        }
        m_renderer_system.reset();

        m_render_context.reset();
        m_input_system.reset();

        if (m_window_system) {
            m_window_system->shutdown();
        }
        m_window_system.reset();

        m_file_system.reset();
    }
} // namespace NexAur
