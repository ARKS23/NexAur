#pragma once
#include <entt/entt.hpp>
#include "Core/Base.h"
#include "Core/Log/log_system.h"
#include "scene_v2.h"

namespace NexAur {
    class NEXAUR_API Entity {
    public:
        Entity() = default;
        Entity(entt::entity handle, SceneV2* scene);
        Entity(const Entity& other) = default;

        // 添加组件
        template<typename T, typename... Args>
        T& addComponent(Args&&... args);

        template<typename T, typename... Args>
        T& addOrReplaceComponent(Args&&... args);

        // 获取组件
        template<typename T>
        T& getComponent() const;

        // 检查是否包含组件
        template<typename T>
        bool hasComponent() const;

        // 移除组件
        template<typename T>
        void removeComponent();

        // 隐式转换
        operator bool() const { return m_EntityHandle != entt::null; }
        operator entt::entity() const { return m_EntityHandle; }
        operator uint32_t() const { return static_cast<uint32_t>(m_EntityHandle); }

        // 比较运算符
        bool operator==(const Entity& other) const { return m_EntityHandle == other.m_EntityHandle && m_Scene == other.m_Scene; }
        bool operator!=(const Entity& other) const { return !(*this == other); }

    private:
        entt::entity m_EntityHandle{ entt::null };
        SceneV2* m_Scene = nullptr;
    };


    // ===================================================== 模板函数实现 =====================================================
    template<typename T, typename... Args>
    T& Entity::addComponent(Args&&... args) {
        NX_CORE_ASSERT(!hasComponent<T>(), "Entity already has component!");
        return m_Scene->m_Registry.emplace<T>(m_EntityHandle, std::forward<Args>(args)...);
    }

    template<typename T, typename... Args>
    T& Entity::addOrReplaceComponent(Args&&... args) {
        return m_Scene->m_Registry.emplace_or_replace<T>(m_EntityHandle, std::forward<Args>(args)...);
    }

    template<typename T>
    T& Entity::getComponent() const {
        return m_Scene->m_Registry.get<T>(m_EntityHandle);
    }

    template<typename T>
    bool Entity::hasComponent() const {
        return m_Scene->m_Registry.any_of<T>(m_EntityHandle);
    }

    template<typename T>
    void Entity::removeComponent() {
        NX_CORE_ASSERT(hasComponent<T>(), "Entity does not have component!");
        m_Scene->m_Registry.remove<T>(m_EntityHandle);
    }
} // namespace NexAur
