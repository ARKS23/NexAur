#include "pch.h"
#include "render_forward_pipeline.h"
#include "Function/Renderer/Passes/skybox_pass_v2.h"
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
            m_pbr_shader->setInt("u_EntityID", obj.entity_id); // 传递实体ID，供编辑器选中使用

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
