#pragma once

#include <memory>
#include <stdexcept>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>

#include "Core/Base.h"
#include "Core/Events/event.h"
#include "Core/Time/TimeStep.h"

namespace NexAur {
    // 模块所属的大阶段。生命周期初始化仍由依赖关系决定；
    // 这里的 stage 主要用于事件分发和后续按阶段调度。
    enum class ModuleStage {
        Core = 0,
        Platform,
        Resource,
        Runtime,
        Physics,
        Audio,
        Renderer,
        Editor,
        Game,
        UI
    };

    // 每个模块对 ModuleManager 暴露的元信息：
    // name 用于唯一标识，dependencies 用于拓扑排序，stage 用于事件优先级。
    struct ModuleInfo {
        std::string name;
        ModuleStage stage = ModuleStage::Core;
        std::vector<std::string> dependencies;
    };

    // 轻量服务注册表。模块把“对外能力”注册成窄接口，
    // 其他模块只取需要的 Service，而不是直接依赖内部系统实现。
    class NEXAUR_API ModuleRegistry {
    public:
        template<typename T>
        void registerService(std::shared_ptr<T> service) {
            m_services[std::type_index(typeid(T))] = std::move(service);
        }

        template<typename T>
        std::shared_ptr<T> getService() const {
            auto it = m_services.find(std::type_index(typeid(T)));
            if (it == m_services.end()) {
                return nullptr;
            }

            return std::static_pointer_cast<T>(it->second);
        }

        template<typename T>
        std::shared_ptr<T> getRequiredService() const {
            std::shared_ptr<T> service = getService<T>();
            if (!service) {
                throw std::runtime_error("Required module service is not registered.");
            }

            return service;
        }

        template<typename T>
        void resetService() {
            m_services.erase(std::type_index(typeid(T)));
        }

        void clear() { m_services.clear(); }

    private:
        std::unordered_map<std::type_index, std::shared_ptr<void>> m_services;
    };

    struct ModuleContext {
        ModuleRegistry& registry;
    };

    struct TickContext {
        TimeStep delta_time;
    };

    // 引擎模块的统一生命周期接口。
    // 新模块优先通过 Service 暴露能力，不要让深层对象直接访问全局上下文。
    class NEXAUR_API EngineModule {
    public:
        virtual ~EngineModule() = default;

        virtual ModuleInfo getInfo() const = 0;
        // 预留阶段：适合提前注册 Service 壳，避免 initialize 阶段互相找不到接口。
        virtual void registerServices(ModuleContext& context) { (void)context; }
        // 创建真实系统资源，例如窗口、渲染器、场景管理器等。
        virtual void initialize(ModuleContext& context) { (void)context; }
        // 所有模块 initialize 后的补线阶段，适合处理跨模块最终连接。
        virtual void postInitialize(ModuleContext& context) { (void)context; }
        // 每帧早期更新，当前主要用于 PlatformModule 刷新输入快照。
        virtual void tick(const TickContext& tick_context) { (void)tick_context; }
        // 每帧逻辑提取后更新，EditorCamera 这类会覆盖渲染数据的逻辑放这里。
        virtual void postUpdate(const TickContext& tick_context) { (void)tick_context; }
        // UI 绘制阶段，EditorModule/GameModule 后续都可以在这里画自己的 UI。
        virtual void renderUI(const TickContext& tick_context) { (void)tick_context; }
        // 事件由 ModuleManager 按 stage 顺序分发，event.handled 后停止传播。
        virtual void onEvent(Event& event) { (void)event; }
        // 关闭顺序由 ModuleManager 反向执行，保证依赖方先释放。
        virtual void shutdown(ModuleContext& context) { (void)context; }
    };
} // namespace NexAur
