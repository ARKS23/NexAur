#include "pch.h"
#include "entity.h"
#include "scene_v2.h"
#include "component.h"
#include "Function/Renderer/data/render_data.h"
#include "Function/Renderer/data/render_debug_draw_builder.h"

namespace NexAur {
    namespace {
        void writeCameraData(RenderDataPacket& render_packet, const CameraComponent& camera) {
            render_packet.camera_data.view_matrix = camera.viewMatrix;
            render_packet.camera_data.projection_matrix = camera.projectionMatrix;
            render_packet.camera_data.view_projection_matrix = camera.viewProjectionMatrix;
            render_packet.camera_data.position = camera.position;
            render_packet.camera_data.near_clip = camera.nearClip;
            render_packet.camera_data.far_clip = camera.farClip;
        }
    } // namespace

    SceneV2::SceneV2() {
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
        if (!render_packet) return;

        render_packet->clear();
        const RenderDebugVisualizationOptions& debug_options =
            render_packet->debug_visualization_options;

        // GameView 使用场景 active camera；没有 active 标记时保留第一相机 fallback。
        bool camera_written = false;
        auto active_camera_view = m_Registry.view<CameraComponent, ActiveCameraComponent>();
        for (auto entity : active_camera_view) {
            const auto& active_camera = active_camera_view.get<ActiveCameraComponent>(entity);
            if (!active_camera.enabled) {
                continue;
            }
            writeCameraData(*render_packet, active_camera_view.get<CameraComponent>(entity));
            camera_written = true;
            break;
        }

        if (!camera_written) {
            auto camera_view = m_Registry.view<CameraComponent>();
            for (auto entity : camera_view) {
                writeCameraData(*render_packet, camera_view.get<CameraComponent>(entity));
                camera_written = true;
                break;
            }
        }

        if (camera_written && debug_options.enabled && debug_options.camera_frustum) {
            RenderDebugDrawBuilder::addFrustum(
                render_packet->debug_draw,
                render_packet->camera_data.view_projection_matrix,
                glm::vec4{ 0.35f, 0.65f, 1.0f, 1.0f });
        }

        // 方向光数据
        auto view_dir_light = m_Registry.view<DirectionalLightComponent>();
        for (auto entity : view_dir_light) {
            const auto& dir_light_comp = view_dir_light.get<DirectionalLightComponent>(entity);
            render_packet->directional_light_data.direction = dir_light_comp.direction;
            render_packet->directional_light_data.color = dir_light_comp.color;
            render_packet->directional_light_data.intensity = dir_light_comp.intensity;
            render_packet->directional_light_data.cast_shadow = dir_light_comp.cast_shadow;
            render_packet->directional_light_data.shadow_strength = dir_light_comp.shadow_strength;
            render_packet->directional_light_data.shadow_bias = dir_light_comp.shadow_bias;
            render_packet->directional_light_data.shadow_distance = dir_light_comp.shadow_distance;

            glm::vec3 light_position{ 0.0f };
            if (const TransformComponent* transform = m_Registry.try_get<TransformComponent>(entity)) {
                light_position = transform->translation;
            }
            if (debug_options.enabled && debug_options.light_gizmos) {
                RenderDebugDrawBuilder::addDirectionalLightGizmo(
                    render_packet->debug_draw,
                    light_position,
                    dir_light_comp.direction,
                    glm::vec4{ dir_light_comp.color, 1.0f });
            }
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
            point_light_data.cast_shadow = point_light_comp.cast_shadow;
            point_light_data.shadow_range = point_light_comp.shadow_range;
            point_light_data.shadow_strength = point_light_comp.shadow_strength;

            render_packet->point_lights_data.push_back(point_light_data);
            if (debug_options.enabled && debug_options.light_gizmos) {
                RenderDebugDrawBuilder::addPointLightGizmo(
                    render_packet->debug_draw,
                    point_light_data.position,
                    0.35f,
                    glm::vec4{ point_light_data.color, 1.0f });
            }
        });


        // IBL
        auto view_env = m_Registry.view<EnvironmentComponent>();
        for (auto entity : view_env) {
            const auto& env_comp = view_env.get<EnvironmentComponent>(entity);
            // Scene only exports environment identity; GPU resources are resolved by the renderer backend.
            render_packet->environment_data.environment_asset = env_comp.getEnvironmentHandle();
            render_packet->environment_data.background_color = env_comp.background_color;
            render_packet->environment_data.intensity = env_comp.intensity;
            render_packet->environment_data.skybox_intensity = env_comp.skybox_intensity;
            render_packet->environment_data.ibl_intensity = env_comp.ibl_intensity;
            break;
        }

        // 网格体数据处理
        auto view_meshes = m_Registry.view<MeshRendererComponent, TransformComponent>();
        view_meshes.each([&](auto entity, const auto& mesh_renderer_comp, const auto& transform_comp) {
            AssetHandle model_handle = mesh_renderer_comp.getModelHandle();
            if (!model_handle) return;

            RenderObjectData object_data;
            // Scene 只输出资产引用，GPU 数据由 Renderer 后端在渲染前解析。
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
        // Scene 只推进运行时逻辑，不再用编辑器观察相机覆盖 CameraComponent。
        (void)deltaTime;
    }
} // namespace NexAur
