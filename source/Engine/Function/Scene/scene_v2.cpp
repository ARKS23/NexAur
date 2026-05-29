#include "pch.h"
#include "entity.h"
#include "scene_v2.h"
#include "component.h"
#include "Function/Global/global_context.h"
#include "Function/File/file_system.h"
#include "Function/Renderer/data/render_data.h"
#include "Function/Resource/asset_manager.h"

#include "Function/Renderer/editor_camera.h" // 测试用

namespace NexAur {
    SceneV2::SceneV2() {
        AssetManager& asset_manager = AssetManager::getInstance();

        // 测试摄像机
        m_editor_camera = std::make_shared<EditorCamera>(45.0f, 1920.0f / 1080.0f, 0.1f, 1000.0f);

        // 设置主相机
        Entity camera_entity = createEntity("MainCamera");
        CameraComponent& camera_component = camera_entity.addComponent<CameraComponent>();
        camera_component.position = { 0.0f, 0.0f, 5.0f };
        camera_component.viewMatrix = glm::translate(glm::mat4(1.0f), -camera_component.position); 
        camera_component.projectionMatrix = glm::perspective(glm::radians(45.0f), 1280.0f / 720.0f, 0.1f, 100.0f);
        camera_component.viewProjectionMatrix = camera_component.projectionMatrix * camera_component.viewMatrix;

        // 环境光
        Entity env_entity = createEntity("Environment");
        AssetHandle env_map_asset = asset_manager.importEnvironmentMapAsset(NX_ASSET("assets/textures/HDR/warm_restaurant_8k.hdr"));
        if (env_map_asset) {
            env_entity.addComponent<EnvironmentComponent>(env_map_asset);
            env_entity.getComponent<EnvironmentComponent>().intensity = 1.0f;
        }

        // 方向光
        Entity dir_light_entity = createEntity("DirectionalLight");
        DirectionalLightComponent& dir_light_comp = dir_light_entity.addComponent<DirectionalLightComponent>();
        dir_light_comp.direction = glm::normalize(glm::vec3(-0.6f, -0.6f, 0.6f));
        dir_light_comp.color = glm::vec3(1.0f, 1.0f, 1.0f);
        dir_light_comp.intensity = 3.3f;

        // 点光源
        Entity point_light_entity = createEntity("PointLight");
        PointLightComponent& point_light_comp = point_light_entity.addComponent<PointLightComponent>();
        point_light_comp.color = glm::vec3(1.0f, 1.0f, 1.0f);
        point_light_comp.intensity = 1.0f;
        point_light_comp.constant = 1.0f;
        point_light_comp.linear = 0.09f;
        point_light_comp.quadratic = 0.032f;
        TransformComponent& point_light_transform = point_light_entity.getComponent<TransformComponent>();
        point_light_transform.translation = glm::vec3(-5.0f, 0.0f, 0.0f);
        
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

    Entity SceneV2::findEntityByName(const std::string& name) {
        auto view = m_Registry.view<TagComponent>();
        for (auto entity : view) {
            const auto& tag = view.get<TagComponent>(entity);
            if (tag.name == name) {
                return Entity(entity, this);
            }
        }
        return Entity(entt::null, this); // 返回无效实体
    }

    void SceneV2::extractSceneData(RenderDataPacket* render_packet) {
        render_packet->clear();

        // 摄像机数据
        auto view_camera = m_Registry.view<CameraComponent>();
        for (auto entity : view_camera) {
            const auto& cam_comp = view_camera.get<CameraComponent>(entity);
            render_packet->camera_data.view_matrix = cam_comp.viewMatrix;
            render_packet->camera_data.projection_matrix = cam_comp.projectionMatrix;
            render_packet->camera_data.view_projection_matrix = cam_comp.viewProjectionMatrix;
            render_packet->camera_data.position = cam_comp.position;
        }

        // 方向光数据
        auto view_dir_light = m_Registry.view<DirectionalLightComponent>();
        for (auto entity : view_dir_light) {
            const auto& dir_light_comp = view_dir_light.get<DirectionalLightComponent>(entity);
            render_packet->directional_light_data.direction = dir_light_comp.direction;
            render_packet->directional_light_data.color = dir_light_comp.color;
            render_packet->directional_light_data.intensity = dir_light_comp.intensity;
            break; // 目前版本只支持一个方向光
        }

        // 点光源数据
        auto view_point_light = m_Registry.view<PointLightComponent, TransformComponent>();
        view_point_light.each([&](auto entity, const auto& point_light_comp, const auto& transform_comp) {
            RendererPointLightData point_light_data;
            
            point_light_data.position = transform_comp.translation; 
            point_light_data.color = point_light_comp.color;
            point_light_data.intensity = point_light_comp.intensity;
            point_light_data.constant = point_light_comp.constant;
            point_light_data.linear = point_light_comp.linear;
            point_light_data.quadratic = point_light_comp.quadratic;

            render_packet->point_lights_data.push_back(point_light_data);
        });


        // IBL
        auto view_env = m_Registry.view<EnvironmentComponent>();
        for (auto entity : view_env) {
            const auto& env_comp = view_env.get<EnvironmentComponent>(entity);
            AssetHandle environment_handle = env_comp.getEnvironmentHandle();
            if (!environment_handle) break;

            // Scene 只输出 HDR 环境资产引用，IBL GPU 数据由 RendererSystem 解析。
            render_packet->environment_data.environment_asset = environment_handle;
            render_packet->environment_data.intensity = env_comp.intensity;
            break;
        }

        // 网格体数据处理
        auto view_meshes = m_Registry.view<MeshRendererComponent, TransformComponent>();
        view_meshes.each([&](auto entity, const auto& mesh_renderer_comp, const auto& transform_comp) {
            AssetHandle model_handle = mesh_renderer_comp.getModelHandle();
            if (!model_handle) return;

            RenderObjectData object_data;
            // Scene 只输出资产引用，GPU 数据由 RendererSystem 在渲染前解析。
            object_data.model_asset = model_handle;
            object_data.transform = transform_comp.getTransform();
            object_data.entity_id = static_cast<int>(static_cast<uint32_t>(entity)); // 标记实体ID，编辑器选中时需要

            // 分流推入不同的数组
            if (mesh_renderer_comp.is_transparent) 
                render_packet->transparent_objects.push_back(object_data);
            else 
                render_packet->opaque_objects.push_back(object_data);
        });
    }

    void SceneV2::destroyEntity(Entity entity) {
        m_Registry.destroy(entity);
    }

    void SceneV2::tick(float deltaTime) {
        m_editor_camera->onUpdate(TimeStep(deltaTime)); // 目前是测试相机
        auto view = m_Registry.view<CameraComponent>();
        for (auto entity : view) {
            auto& cam_comp = view.get<CameraComponent>(entity);
            cam_comp.position = m_editor_camera->getPosition();
            cam_comp.viewMatrix = m_editor_camera->getView();
            cam_comp.projectionMatrix = m_editor_camera->getProjection();
            cam_comp.viewProjectionMatrix = m_editor_camera->getViewProjection();
            break;  // 只设置主相机
        }
    }
} // namespace NexAur
