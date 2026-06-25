#include "pch.h"
#include "global_context.h"

#include "Editor/editor_module.h"
#include "Core/Module/core_engine_module.h"
#include "Core/Module/module_manager.h"
#include "Function/File/file_system.h"
#include "Function/File/file_system_module.h"
#include "Function/Input/input_system.h"
#include "Function/Platform/platform_module.h"
#include "Function/Renderer/renderer_module.h"
#include "Function/Renderer/data/render_context.h"
#include "Function/Platform/window_system.h"
#include "Function/Resource/resource_module.h"
#include "Function/Scene/scene_manager.h"
#include "Function/Scene/runtime_module.h"
#include "Function/UI/ui_module.h"
#include "Function/UI/ui_system.h"

namespace NexAur {
    RunTimeGlobalContext g_runtime_global_context;

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
        m_module_manager->registerModule(createCoreEngineModule());
        m_module_manager->registerModule(createFileSystemModule());
        m_module_manager->registerModule(createPlatformModule());
        m_module_manager->registerModule(createResourceModule());
        m_module_manager->registerModule(createRenderContextModule());
        m_module_manager->registerModule(createRendererModule());
        m_module_manager->registerModule(createRuntimeModule());
        m_module_manager->registerModule(createUIModule());
        m_module_manager->registerModule(createEditorModule());
        m_module_manager->initializeModules();
        syncCompatibilityServices();
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

    void RunTimeGlobalContext::syncCompatibilityServices() {
        ModuleRegistry* registry = getModuleRegistry();
        if (!registry) {
            clearCompatibilityServices();
            return;
        }

        // 兼容桥统一在组合根同步；模块实现不再反向依赖 RunTimeGlobalContext。
        m_file_system = registry->getService<FileSystem>();
        m_window_system = registry->getService<WindowSystem>();
        m_input_system = registry->getService<InputSystem>();
        m_scene_manager = registry->getService<SceneManager>();
        m_render_context = registry->getService<RenderContext>();
        m_ui_system = registry->getService<UISystem>();
    }

    void RunTimeGlobalContext::clearCompatibilityServices() {
        m_ui_system.reset();
        m_scene_manager.reset();
        m_render_context.reset();
        m_input_system.reset();
        m_window_system.reset();
        m_file_system.reset();
    }
} // namespace NexAur
