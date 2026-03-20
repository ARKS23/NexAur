#include "pch.h"
#include "entity.h"
#include "scene_v2.h"
#include "component.h"
#include "Function/Global/global_context.h"
#include "Function/File/file_system.h"
#include "Function/Renderer/data/render_data.h"
#include "Function/Resource/asset_manager.h"
#include "Function/Resource/ibl_builder.h"

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
        UUID env_map_id = asset_manager.loadEnvironmentMap(NX_ASSET("assets/textures/HDR/warm_restaurant_8k.hdr"));
        if (env_map_id != INVALID_UUID) {
            env_entity.addComponent<EnvironmentComponent>(env_map_id);
            env_entity.getComponent<EnvironmentComponent>().intensity = 1.0f;
        }

        // 方向光
        Entity dir_light_entity = createEntity("DirectionalLight");
        DirectionalLightComponent& dir_light_comp = dir_light_entity.addComponent<DirectionalLightComponent>();
        dir_light_comp.direction = glm::normalize(glm::vec3(-0.5f, -1.0f, -0.5f));
        dir_light_comp.color = glm::vec3(1.0f, 1.0f, 1.0f);
        dir_light_comp.intensity = 1.5f;

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

    void SceneV2::extractSceneData(RenderDataPacket* render_packet) {
        render_packet->clear();

        AssetManager& asset_manager = AssetManager::getInstance();

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
            if (env_comp.environment_map_id == INVALID_UUID) break;

            std::shared_ptr<EnvironmentMap> env_map = asset_manager.getEnvironmentMap(env_comp.environment_map_id);
            if (env_map) {
                render_packet->environment_data.skybox_texture = env_map->skybox_texture;
                render_packet->environment_data.irradiance_map = env_map->irradiance_map;
                render_packet->environment_data.prefilter_map = env_map->prefilter_map;
                render_packet->environment_data.brdf_lut_map = env_map->brdf_lut_map;
                render_packet->environment_data.intensity = env_comp.intensity;
            }
            break;
        }

        // 网格体数据处理
        auto view_meshes = m_Registry.view<MeshRendererComponent, TransformComponent>();
        view_meshes.each([&](auto entity, const auto& mesh_renderer_comp, const auto& transform_comp) {
            if (mesh_renderer_comp.model_id == INVALID_UUID) return; 

            // 获取数据并防御性判断
            auto gpu_model = asset_manager.getRenderModel(mesh_renderer_comp.model_id);
            if (!gpu_model) return; 

            RenderObjectData object_data;
            object_data.model_data = gpu_model; // 直接复用刚才获取的指针
            object_data.transform = transform_comp.getTransform();

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
