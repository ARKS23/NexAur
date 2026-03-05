#include "pch.h"
#include "render_forward_pipeline.h"
#include "Function/Renderer/Passes/container_pass.h"
#include "Function/Renderer/Passes/sphere_pass.h"
#include "Function/Renderer/Passes/skybox_pass.h"
#include "Function/Renderer/Passes/floor_pass.h"
#include "Function/Renderer/RHI/renderer_command.h"
#include "Function/Renderer/editor_camera.h"
#include "Function/Renderer/RHI/Renderer.h"

namespace NexAur {
    void RenderForwardPipeline::init() {
        // 初始化前向渲染管线所需的资源，如渲染通道、着色器等        
        RenderPassSpecification sphereSpec;
        sphereSpec.debug_name = "Sphere Pass";
        sphereSpec.target_framebuffer = nullptr; // 直接画到屏幕
        sphereSpec.clear_color = { 0.0f, 0.0f, 0.0f, 1.0f }; // 黑色背景
        sphereSpec.clear_buffer_flags = ClearBufferFlag::ColorDepth; // 不清除缓冲
        m_sphere_pass = std::make_shared<SpherePass>(sphereSpec);

        RenderPassSpecification containerSpec;
        containerSpec.debug_name = "Container Pass";
        containerSpec.clear_buffer_flags = ClearBufferFlag::None;
        m_container_pass = std::make_shared<ContainerPass>(containerSpec);

        RenderPassSpecification floorSpec;
        floorSpec.debug_name = "Floor pass";
        floorSpec.clear_buffer_flags = ClearBufferFlag::None;
        m_floor_pass = std::make_shared<FloorPass>(floorSpec);

        RenderPassSpecification skyboxSpec;
        skyboxSpec.debug_name = "Skybox Pass";
        skyboxSpec.clear_buffer_flags = ClearBufferFlag::None;
        m_skybox_pass = std::make_shared<SkyboxPass>(skyboxSpec);
        m_skybox_pass->setCamera(nullptr); // render时再传入
    }

    void RenderForwardPipeline::render(std::shared_ptr<Camera> camera) {
        // 设置vp矩阵
        Renderer::setCameraMatrix(camera->getViewProjection());

        // 执行前向渲染流程
        m_sphere_pass->run();
        m_container_pass->run();
        m_floor_pass->run();

        // 天空盒
        m_skybox_pass->setCamera(camera);
        m_skybox_pass->run();
    }

    void RenderForwardPipeline::renderScene(std::shared_ptr<Scene> scene, std::shared_ptr<Camera> camera) {
        RendererCommand::clear(ClearBufferFlag::Depth | ClearBufferFlag::Color);
        
        // vp矩阵
        Renderer::setCameraMatrix(camera->getViewProjection());

        // 光源信息
        const DirectionalLight& dir_light = scene->getDirectionalLight();
        const std::vector<PointLight>& point_lights = scene->getPointLights();

        // 绘制场景物体
        if (!scene) return;
        for (const RenderEntity& entity : scene->getEntities()) {
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

            Renderer::submit(entity.material, entity.mesh, entity.transform);
        }

        // 绘制点光源
        for (const RenderEntity& entity : scene->getLightsEntities()) {
            Renderer::submit(entity.material, entity.mesh, entity.transform);
        }

        // 天空盒
        if (scene->isSkyboxEnabled()) {
            m_skybox_pass->setCamera(camera);
            m_skybox_pass->run();
        }
    }
} // namespace NexAur
