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

    struct ModuleInfo {
        std::string name;
        ModuleStage stage = ModuleStage::Core;
        std::vector<std::string> dependencies;
    };

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

    class NEXAUR_API EngineModule {
    public:
        virtual ~EngineModule() = default;

        virtual ModuleInfo getInfo() const = 0;
        virtual void registerServices(ModuleContext& context) { (void)context; }
        virtual void initialize(ModuleContext& context) { (void)context; }
        virtual void postInitialize(ModuleContext& context) { (void)context; }
        virtual void tick(const TickContext& tick_context) { (void)tick_context; }
        virtual void postUpdate(const TickContext& tick_context) { (void)tick_context; }
        virtual void renderUI(const TickContext& tick_context) { (void)tick_context; }
        virtual void onEvent(Event& event) { (void)event; }
        virtual void shutdown(ModuleContext& context) { (void)context; }
    };
} // namespace NexAur
