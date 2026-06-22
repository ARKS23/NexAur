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

        for (size_t module_index : ordered_modules) {
            EngineModule* module = m_modules[module_index].get();
            module->registerServices(context);
        }

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

    void ModuleManager::visitModule(
        size_t module_index,
        std::vector<uint8_t>& visit_state,
        std::vector<size_t>& ordered_modules) const {
        if (visit_state[module_index] == 2) {
            return;
        }

        if (visit_state[module_index] == 1) {
            throw std::runtime_error("Engine module dependency cycle detected.");
        }

        visit_state[module_index] = 1;

        const ModuleInfo module_info = m_modules[module_index]->getInfo();
        for (const std::string& dependency_name : module_info.dependencies) {
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
