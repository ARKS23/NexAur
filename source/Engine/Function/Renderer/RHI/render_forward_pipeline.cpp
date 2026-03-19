#include "pch.h"
#include "render_forward_pipeline.h"
#include "Function/Renderer/Passes/skybox_pass.h"
#include "Function/Renderer/Passes/skybox_pass_v2.h"
#include "Function/Renderer/Passes/shadow_pass.h"
#include "Function/Renderer/Passes/shadow_pass_v2.h"
#include "Function/Renderer/RHI/renderer_command.h"
#include "Function/Renderer/editor_camera.h"
#include "Function/Renderer/RHI/Renderer.h"
#include "Function/Renderer/data/render_data.h"
#include "Function/Global/global_context.h"
#include "Function/File/file_system.h"
#include "Function/Renderer/RHI/renderer_system.h"

namespace NexAur {
    void RenderForwardPipeline::init() {
        // 初始化前向渲染管线所需的资源，如渲染通道、着色器等        
        RenderPassSpecification skyboxSpec;
        skyboxSpec.debug_name = "Skybox Pass";
        skyboxSpec.clear_buffer_flags = ClearBufferFlag::None;
        m_skybox_pass = std::make_shared<SkyboxPass>(skyboxSpec);
        m_skybox_pass->setCamera(nullptr); // render时再传入

        RenderPassSpecification shadowSpec;
        shadowSpec.debug_name = "Shadow Pass";
        shadowSpec.clear_buffer_flags = ClearBufferFlag::Depth;
        m_shadow_pass = std::make_shared<ShadowPass>(shadowSpec);

        // PBR着色器
        m_pbr_shader = RendererFactory::createShaderByPaths("sphere pbr shader",
        NX_ASSET("assets/shaders/pbr/pbr.vs"), NX_ASSET("assets/shaders/pbr/pbr.fs"));

        // 新版本天空盒Pass
        RenderPassSpecificationV2 skyboxSpecV2;
        skyboxSpecV2.debug_name = "Skybox Pass V2";
        skyboxSpecV2.clear_buffer_flags = ClearBufferFlag::None;
        m_skybox_pass_v2 = std::make_shared<SkyboxPassV2>(skyboxSpecV2);

        // 新版本阴影Pass
        RenderPassSpecificationV2 shadowSpecV2;
        shadowSpecV2.debug_name = "Shadow Pass V2";
        shadowSpecV2.clear_buffer_flags = ClearBufferFlag::Depth;
        m_shadow_pass_v2 = std::make_shared<ShadowPassV2>(shadowSpecV2);
    }

    void RenderForwardPipeline::render(std::shared_ptr<Camera> camera) {
        // 设置vp矩阵
        Renderer::setCameraMatrix(camera->getViewProjection());

        // 天空盒
        m_skybox_pass->setCamera(camera);
        m_skybox_pass->run();
    }

    void RenderForwardPipeline::renderScene(std::shared_ptr<Scene> scene, std::shared_ptr<Camera> camera) {
        RendererCommand::clear(ClearBufferFlag::Depth | ClearBufferFlag::Color);

        // 渲染shadow_map
        m_shadow_pass->execute(scene); // 后续优化
        
        // vp矩阵
        Renderer::setCameraMatrix(camera->getViewProjection());

        // 光源信息
        const DirectionalLight& dir_light = scene->getDirectionalLight();
        const std::vector<PointLight>& point_lights = scene->getPointLights();

        // 绘制场景物体
        if (!scene) return;
        for (const RenderEntity& entity : scene->getEntities()) {
            // 阴影绘制
            glm::mat4 light_space_matrix = m_shadow_pass->getLightSpaceMatrix();
            std::shared_ptr<Texture2D> shadow_map = m_shadow_pass->getDepthMapTexture();
            entity.material->setTexture("u_ShadowMap", shadow_map);
            entity.material->setMat4("u_LightSpaceMatrix", light_space_matrix);

            // 场景正常绘制
            std::shared_ptr<Shader> shader = entity.material->getShader();
            shader->bind();

            shader->setFloat3("u_CameraPos", camera->getPosition());
            shader->setFloat3("u_AmbientLight", glm::vec3(0.1f, 0.1f, 0.1f));


            // 定向光
            shader->setFloat3("u_DirLight.direction", dir_light.direction);
            shader->setFloat3("u_DirLight.color", dir_light.color);
            shader->setFloat("u_DirLight.intensity", dir_light.intensity);

            // 点光源
            int point_light_count = std::min(static_cast<int>(point_lights.size()), scene->getPointLightMax());
            shader->setInt("u_NumPointLights", point_light_count);
            for (size_t i = 0; i < point_light_count; ++i) {
                const PointLight& pl = point_lights[i];
                std::string index_str = std::to_string(i);
                shader->setFloat3("u_PointLights[" + index_str + "].position", pl.position);
                shader->setFloat3("u_PointLights[" + index_str + "].color", pl.color);
                shader->setFloat("u_PointLights[" + index_str + "].intensity", pl.intensity);
                shader->setFloat("u_PointLights[" + index_str + "].constant", pl.constant);
                shader->setFloat("u_PointLights[" + index_str + "].linear", pl.linear);
                shader->setFloat("u_PointLights[" + index_str + "].quadratic", pl.quadratic);
            }

            // 天空盒显示影响IBL
            shader->setInt("u_SkyboxEnabled", scene->isSkyboxEnabled() ? 1 : 0);

            Renderer::submit(entity.material, entity.mesh, entity.transform);
        }

        // 绘制点光源
        for (const RenderEntity& entity : scene->getLightsEntities()) {
            Renderer::submit(entity.material, entity.mesh, entity.transform);
        }

        // 天空盒
        if (scene->isSkyboxEnabled()) {
            m_skybox_pass->setSkyboxTexture(scene->getSkyboxTexture());
            m_skybox_pass->setCamera(camera);
            m_skybox_pass->run();
        }
    }

    void RenderForwardPipeline::render(const RenderDataPacket& render_data) {
        RendererCommand::clear(ClearBufferFlag::Depth | ClearBufferFlag::Color);

        // shadow pass
        m_shadow_pass_v2->run_without_begin_end(render_data);

        // 获取shader
        if (!m_pbr_shader) return;
        m_pbr_shader->bind();

        // 摄像机相关设置
        // m_pbr_shader->setMat4("u_ViewProjection", render_data.camera_data.view_projection_matrix);
        Renderer::setCameraMatrix(render_data.camera_data.view_projection_matrix);
        m_pbr_shader->setFloat3("u_CameraPos", render_data.camera_data.position);

        // 全局常量设置
        m_pbr_shader->setFloat3("u_AmbientLight", glm::vec3(0.1f, 0.1f, 0.1f));

        // 定向光
        m_pbr_shader->setFloat3("u_DirLight.direction", render_data.directional_light_data.direction);
        m_pbr_shader->setFloat3("u_DirLight.color", render_data.directional_light_data.color);
        m_pbr_shader->setFloat("u_DirLight.intensity", render_data.directional_light_data.intensity);

        // 点光源
        m_pbr_shader->setInt("u_NumPointLights", static_cast<int>(render_data.point_lights_data.size()));
        for (size_t i = 0; i < render_data.point_lights_data.size(); ++i) {
            const auto& pl = render_data.point_lights_data[i];
            std::string index_str = std::to_string(i);
            m_pbr_shader->setFloat3("u_PointLights[" + index_str + "].position", pl.position);
            m_pbr_shader->setFloat3("u_PointLights[" + index_str + "].color", pl.color);
            m_pbr_shader->setFloat("u_PointLights[" + index_str + "].intensity", pl.intensity);
            m_pbr_shader->setFloat("u_PointLights[" + index_str + "].constant", pl.constant);
            m_pbr_shader->setFloat("u_PointLights[" + index_str + "].linear", pl.linear);
            m_pbr_shader->setFloat("u_PointLights[" + index_str + "].quadratic", pl.quadratic);
        }

        // 纹理单元槽
        const int SLOT_ALBEDO    = 0;
        const int SLOT_NORMAL    = 1;
        const int SLOT_METALLIC  = 2;
        const int SLOT_ROUGHNESS = 3;
        const int SLOT_AO        = 4;
        const int SLOT_SHADOW    = 5;
        const int SLOT_IRRADIANCE= 6;
        const int SLOT_PREFILTER = 7;
        const int SLOT_BRDF_LUT  = 8;

        // 绑定阴影贴图
        glm::mat4 light_space_matrix = m_shadow_pass_v2->getLightSpaceMatrix();
        std::shared_ptr<Texture2D> shadow_map = m_shadow_pass_v2->getDepthMapTexture();
        if (shadow_map) {
            shadow_map->bind(SLOT_SHADOW);
            m_pbr_shader->setInt("u_ShadowMap", SLOT_SHADOW);
        }
        m_pbr_shader->setMat4("u_LightSpaceMatrix", light_space_matrix);

        // 环境光
        bool has_ibl = render_data.environment_data.irradiance_map != nullptr;
        m_pbr_shader->setInt("u_SkyboxEnabled", has_ibl ? 1 : 0);
        if (has_ibl) {
            if (render_data.environment_data.irradiance_map) {
                render_data.environment_data.irradiance_map->bind(SLOT_IRRADIANCE);
                m_pbr_shader->setInt("u_IrradianceMap", SLOT_IRRADIANCE);
            }
            if (render_data.environment_data.prefilter_map) {
                render_data.environment_data.prefilter_map->bind(SLOT_PREFILTER);
                m_pbr_shader->setInt("u_PrefilterMap", SLOT_PREFILTER);
            }
            if (render_data.environment_data.brdf_lut_map) {
                render_data.environment_data.brdf_lut_map->bind(SLOT_BRDF_LUT);
                m_pbr_shader->setInt("u_BrdfLUT", SLOT_BRDF_LUT);
            }
        }
        
        // 绘制不透明物体
        for (const RenderObjectData& obj : render_data.opaque_objects) {
            if (!obj.model_data) continue;
            // 设置模型矩阵
            m_pbr_shader->setMat4("u_Transform", obj.transform);

            // 遍历模型内所有网格体部分
            for (const RenderMeshData& mesh : obj.model_data->meshes) {
                // 绑定 PBR 材质贴图, 没有贴图的话就传递常量 （常量部分未完成）
                if (mesh.material.albedo_map) {
                    mesh.material.albedo_map->bind(SLOT_ALBEDO);
                    m_pbr_shader->setInt("u_Material.albedoMap", SLOT_ALBEDO);
                    m_pbr_shader->setInt("u_Material.useAlbedoMap", 1);
                } else {
                    m_pbr_shader->setInt("u_Material.useAlbedoMap", 0);
                    m_pbr_shader->setFloat3("u_Material.albedo", mesh.material.albedo);
                }

                if (mesh.material.normal_map) {
                    mesh.material.normal_map->bind(SLOT_NORMAL);
                    m_pbr_shader->setInt("u_Material.normalMap", SLOT_NORMAL);
                    m_pbr_shader->setInt("u_Material.useNormalMap", 1);
                } else {
                    m_pbr_shader->setInt("u_Material.useNormalMap", 0);
                }

                if (mesh.material.metallic_map) {
                    mesh.material.metallic_map->bind(SLOT_METALLIC);
                    m_pbr_shader->setInt("u_Material.metallicMap", SLOT_METALLIC);
                    m_pbr_shader->setInt("u_Material.useMetallicMap", 1);
                } else {
                    m_pbr_shader->setInt("u_Material.useMetallicMap", 0);
                    m_pbr_shader->setFloat("u_Material.metallic", mesh.material.metallic);
                }

                if (mesh.material.roughness_map) {
                    mesh.material.roughness_map->bind(SLOT_ROUGHNESS);
                    m_pbr_shader->setInt("u_Material.roughnessMap", SLOT_ROUGHNESS);
                    m_pbr_shader->setInt("u_Material.useRoughnessMap", 1);
                } else {
                    m_pbr_shader->setInt("u_Material.useRoughnessMap", 0);
                    m_pbr_shader->setFloat("u_Material.roughness", mesh.material.roughness);
                }

                if (mesh.material.ao_map) {
                    mesh.material.ao_map->bind(SLOT_AO);
                    m_pbr_shader->setInt("u_Material.aoMap", SLOT_AO);
                    m_pbr_shader->setInt("u_Material.useAOMap", 1);
                } else {
                    m_pbr_shader->setInt("u_Material.useAOMap", 0);
                    m_pbr_shader->setFloat("u_Material.ao", mesh.material.ao);
                }

                mesh.vertex_array->bind();
                RendererCommand::drawIndexed(mesh.vertex_array, mesh.index_count);
            }
        }

        // 绘制天空盒
        if (render_data.environment_data.skybox_texture) m_skybox_pass_v2->run(render_data);
    }
} // namespace NexAur
