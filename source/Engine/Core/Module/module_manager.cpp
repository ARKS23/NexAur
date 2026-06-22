#include "pch.h"
#include "Core/Module/module_manager.h"

#include "Core/Log/log_system.h"

#include <algorithm>
#include <stdexcept>

namespace NexAur {
    namespace {
        bool canWriteModuleLog() {
            return LogSystem::getCoreLogger() != nullptr;
        }
    } // namespace

    ModuleManager::~ModuleManager() {
        shutdownModules();
    }

    void ModuleManager::registerModule(std::unique_ptr<EngineModule> module) {
        if (!module) {
            throw std::runtime_error("Attempted to register null engine module.");
        }

        if (m_is_initialized) {
            throw std::runtime_error("Cannot register module after ModuleManager initialization.");
        }

        const std::string module_name = module->getInfo().name;
        if (module_name.empty()) {
            throw std::runtime_error("Engine module name must not be empty.");
        }

        const bool duplicate_name = std::any_of(
            m_modules.begin(),
            m_modules.end(),
            [&](const std::unique_ptr<EngineModule>& registered_module) {
                return registered_module->getInfo().name == module_name;
            });

        if (duplicate_name) {
            throw std::runtime_error("Duplicate engine module registered: " + module_name);
        }

        m_modules.push_back(std::move(module));
    }

    void ModuleManager::initializeModules() {
        if (m_is_initialized) {
            return;
        }

        ModuleContext context{ m_registry };
        std::vector<size_t> ordered_modules = resolveInitializationOrder();

        // 第一轮只注册服务壳。以后如果模块之间有循环查询接口需求，
        // 可以先在这里暴露 Service，再在 initialize 里创建真实资源。
        for (size_t module_index : ordered_modules) {
            EngineModule* module = m_modules[module_index].get();
            module->registerServices(context);
        }

        // 第二轮创建真实系统资源。这里严格遵守依赖拓扑顺序，
        // 例如 Renderer 必须晚于 Platform，Editor 必须晚于 UI/Runtime/Renderer。
        for (size_t module_index : ordered_modules) {
            EngineModule* module = m_modules[module_index].get();
            const ModuleInfo module_info = module->getInfo();
            module->initialize(context);
            m_initialized_modules.push_back(module);

            if (canWriteModuleLog()) {
                NX_CORE_INFO("Engine module initialized: {}", module_info.name);
            }
        }

        for (size_t module_index : ordered_modules) {
            m_modules[module_index]->postInitialize(context);
        }

        // 事件分发不能简单复用初始化顺序：
        // UI 捕获输入必须先于 Editor/Runtime，所以这里单独生成事件顺序。
        // 先反转初始化顺序，再按 stage 稳定排序，可以保留同阶段“后初始化者优先”的直觉。
        m_event_modules.assign(m_initialized_modules.rbegin(), m_initialized_modules.rend());
        std::stable_sort(
            m_event_modules.begin(),
            m_event_modules.end(),
            [](EngineModule* lhs, EngineModule* rhs) {
                return static_cast<int>(lhs->getInfo().stage) > static_cast<int>(rhs->getInfo().stage);
            });

        m_is_initialized = true;
    }

    void ModuleManager::tickModules(const TickContext& tick_context) {
        for (EngineModule* module : m_initialized_modules) {
            module->tick(tick_context);
        }
    }

    void ModuleManager::postUpdateModules(const TickContext& tick_context) {
        for (EngineModule* module : m_initialized_modules) {
            module->postUpdate(tick_context);
        }
    }

    void ModuleManager::renderUIModules(const TickContext& tick_context) {
        for (EngineModule* module : m_initialized_modules) {
            module->renderUI(tick_context);
        }
    }

    void ModuleManager::dispatchEvent(Event& event) {
        for (EngineModule* module : m_event_modules) {
            module->onEvent(event);
            // handled 表示事件已经被消费，后续模块不应该再响应。
            if (event.handled) {
                break;
            }
        }
    }

    void ModuleManager::shutdownModules() {
        if (!m_is_initialized && m_initialized_modules.empty()) {
            return;
        }

        ModuleContext context{ m_registry };
        // 反向关闭可以保证依赖方先释放资源：
        // 例如 Editor 先于 UI，Renderer 先于 Platform/window context。
        for (auto it = m_initialized_modules.rbegin(); it != m_initialized_modules.rend(); ++it) {
            const ModuleInfo module_info = (*it)->getInfo();
            (*it)->shutdown(context);

            if (canWriteModuleLog()) {
                NX_CORE_INFO("Engine module shut down: {}", module_info.name);
            }
        }

        m_initialized_modules.clear();
        m_event_modules.clear();
        m_registry.clear();
        m_is_initialized = false;
    }

    std::vector<size_t> ModuleManager::resolveInitializationOrder() const {
        std::vector<uint8_t> visit_state(m_modules.size(), 0);
        std::vector<size_t> ordered_modules;
        ordered_modules.reserve(m_modules.size());

        for (size_t i = 0; i < m_modules.size(); ++i) {
            visitModule(i, visit_state, ordered_modules);
        }

        return ordered_modules;
    }

    void ModuleManager::visitModule(size_t module_index, std::vector<uint8_t>& visit_state, std::vector<size_t>& ordered_modules) const {
        if (visit_state[module_index] == 2) {
            return;
        }

        if (visit_state[module_index] == 1) {
            throw std::runtime_error("Engine module dependency cycle detected.");
        }

        visit_state[module_index] = 1;

        const ModuleInfo module_info = m_modules[module_index]->getInfo();
        for (const std::string& dependency_name : module_info.dependencies) {
            // 先访问依赖模块，当前模块会在所有依赖之后进入 ordered_modules。
            const size_t dependency_index = findModuleIndex(dependency_name);
            visitModule(dependency_index, visit_state, ordered_modules);
        }

        visit_state[module_index] = 2;
        ordered_modules.push_back(module_index);
    }

    size_t ModuleManager::findModuleIndex(const std::string& module_name) const {
        for (size_t i = 0; i < m_modules.size(); ++i) {
            if (m_modules[i]->getInfo().name == module_name) {
                return i;
            }
        }

        throw std::runtime_error("Engine module dependency is not registered: " + module_name);
    }
} // namespace NexAur
