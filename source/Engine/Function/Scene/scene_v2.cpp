#include "pch.h"
#include "entity.h"
#include "scene_v2.h"
#include "component.h"

namespace NexAur {
    SceneV2::SceneV2() {
        // 后续补充场景初始化逻辑
    }

    SceneV2::~SceneV2() {
        // 后续补充场景销毁逻辑
    }

    Entity SceneV2::createEntity(const std::string& name) {
        // 创建Entity到场景中
        entt::entity entity = m_Registry.create();
        Entity newEntity(entity, this);

        // 场景内可视的物体需要Transform变换和Tag名字
        newEntity.addComponent<TagComponent>(name);
        newEntity.addComponent<TransformComponent>();

        return newEntity;
    }

    void SceneV2::destroyEntity(Entity entity) {
        m_Registry.destroy(entity);
    }

    void SceneV2::tick(float deltaTime) {
        // 后续补充场景更新逻辑,如系统更新等
    }
} // namespace NexAur
