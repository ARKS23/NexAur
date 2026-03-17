#include "pch.h"
#include "global_context.h"

#include "Core/Log/log_system.h"
#include "Function/Renderer/window_system.h"
#include "Function/Input/input_system_glfw.h"
#include "Function/Scene/scene_manager.h"
#include "Function/Renderer/RHI/renderer_system.h"
#include "Function/Renderer/data/render_context.h"
#include "Function/File/file_system.h"

#ifndef ENGINE_ROOT_DIR
#define ENGINE_ROOT_DIR "."
#endif

namespace NexAur {
    RunTimeGlobalContext g_runtime_global_context; // 全局运行时上下文实例

    void RunTimeGlobalContext::startSystems() {
        // 初始化全局静态单例日志系统
        LogSystem::init();

        // 文件系统
        m_file_system = std::make_shared<FileSystem>(ENGINE_ROOT_DIR);

        // 窗口系统
        m_window_system = std::make_shared<WindowSystem>();
        WindowSpecification specification;
        m_window_system->init(specification);

        // 输入系统
        m_input_system = std::make_shared<InputSystemGLFW>();

        // 渲染数据上下文
        m_render_context = std::make_shared<RenderContext>();

        // 场景管理系统
        m_scene_manager = std::make_shared<SceneManager>();
        m_scene_manager->init();

        // 渲染系统
        m_renderer_system =  std::make_shared<RendererSystem>();
        m_renderer_system->init();

        // 物理系统
    }

    void RunTimeGlobalContext::shutdownSystems() {
        m_file_system.reset();

        m_window_system->shutdown();
        m_window_system.reset();

        m_input_system.reset();

        m_renderer_system->shutdown();
        m_renderer_system.reset();

        m_render_context.reset();

        m_scene_manager->shutdown();
        m_scene_manager.reset();
    }
} // namespace NexAur