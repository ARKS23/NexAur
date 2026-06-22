#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Core/Base.h"
#include "Core/Module/engine_module.h"

namespace NexAur {
    class NEXAUR_API ModuleManager {
    public:
        ModuleManager() = default;
        ~ModuleManager();

        ModuleManager(const ModuleManager&) = delete;
        ModuleManager& operator=(const ModuleManager&) = delete;
        ModuleManager(ModuleManager&&) = delete;
        ModuleManager& operator=(ModuleManager&&) = delete;

        void registerModule(std::unique_ptr<EngineModule> module);

        void initializeModules();
        void tickModules(const TickContext& tick_context);
        void postUpdateModules(const TickContext& tick_context);
        void renderUIModules(const TickContext& tick_context);
        void dispatchEvent(Event& event);
        void shutdownModules();

        ModuleRegistry& getRegistry() { return m_registry; }
        const ModuleRegistry& getRegistry() const { return m_registry; }

    private:
        std::vector<size_t> resolveInitializationOrder() const;
        void visitModule(
            size_t module_index,
            std::vector<uint8_t>& visit_state,
            std::vector<size_t>& ordered_modules) const;
        size_t findModuleIndex(const std::string& module_name) const;

    private:
        std::vector<std::unique_ptr<EngineModule>> m_modules;
        std::vector<EngineModule*> m_initialized_modules;
        std::vector<EngineModule*> m_event_modules;
        ModuleRegistry m_registry;
        bool m_is_initialized = false;
    };
} // namespace NexAur
