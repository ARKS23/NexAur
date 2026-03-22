#pragma once
#include <entt/entt.hpp>
#include "Core/Base.h"
#include "Core/UUID.h"

namespace NexAur {
    class Entity;
    class RenderDataPacket;
    class EditorCamera;

    class NEXAUR_API SceneV2 {
    public:
        SceneV2();
        ~SceneV2();

        // 创建实体
        Entity createEntity(const std::string& name = "NexAur_Entity");

        // 销毁实体及组件
        void destroyEntity(Entity entity);

        // 查找接口
        Entity findEntityByName(const std::string& name);
        // Entity findEntityByUUID(UUID uuid);

        // 场景更新
        void tick(float deltaTime);

        // 获取底层注册表, 可供渲染器使用
        entt::registry& getRegistry() { return m_Registry; }

        // 数据打包
        void extractSceneData(RenderDataPacket* render_packet);

    private:
        entt::registry m_Registry;
        friend class Entity;

    private:
        std::shared_ptr<EditorCamera> m_editor_camera;  // 测试摄像机，该摄像机用于跑通流程，后期删除
    };
} // namespace NexAur
