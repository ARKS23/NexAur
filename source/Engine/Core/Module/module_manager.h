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

        // 按 dependencies 做拓扑排序后初始化模块。
        void initializeModules();
        // 每帧早期阶段：适合平台输入、低成本状态刷新。
        void tickModules(const TickContext& tick_context);
        // 每帧逻辑更新后阶段：适合 Editor 覆盖相机、同步选择等。
        void postUpdateModules(const TickContext& tick_context);
        // UI 阶段：模块可以在这里提交 ImGui 或未来 runtime UI。
        void renderUIModules(const TickContext& tick_context);
        // 按事件优先级分发。UI 会先于 Editor/Runtime 获得输入事件。
        void dispatchEvent(Event& event);
        // 按初始化顺序的反向关闭模块。
        void shutdownModules();

        ModuleRegistry& getRegistry() { return m_registry; }
        const ModuleRegistry& getRegistry() const { return m_registry; }

    private:
        // 根据 ModuleInfo::dependencies 计算安全初始化顺序。
        std::vector<size_t> resolveInitializationOrder() const;
        // DFS 访问状态：0 未访问，1 正在访问，2 已完成；1 再次出现说明依赖成环。
        void visitModule(size_t module_index, std::vector<uint8_t>& visit_state, std::vector<size_t>& ordered_modules) const;
        size_t findModuleIndex(const std::string& module_name) const;

    private:
        // ModuleManager 拥有模块实例本身。
        std::vector<std::unique_ptr<EngineModule>> m_modules;
        // 已初始化模块的生命周期顺序，用于 tick 和反向 shutdown。
        std::vector<EngineModule*> m_initialized_modules;
        // 独立的事件顺序：按 stage 排序，不等同于初始化顺序。
        std::vector<EngineModule*> m_event_modules;
        ModuleRegistry m_registry;
        bool m_is_initialized = false;
    };
} // namespace NexAur
