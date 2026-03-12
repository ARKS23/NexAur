#include "pch.h"
#include "entity.h"
#include "component.h"

namespace NexAur {
    Entity::Entity(entt::entity handle, SceneV2* scene)
        : m_EntityHandle(handle), m_Scene(scene) {}
} // namespace NexAur