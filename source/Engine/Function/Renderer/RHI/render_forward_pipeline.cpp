#include "pch.h"
#include "render_forward_pipeline.h"
#include "Function/Renderer/Passes/container_pass.h"
#include "Function/Renderer/Passes/sphere_pass.h"
#include "Function/Renderer/Passes/skybox_pass.h"
#include "Function/Renderer/editor_camera.h"
#include "Function/Renderer/RHI/Renderer.h"

namespace NexAur {
    void RenderForwardPipeline::init() {
        // 初始化前向渲染管线所需的资源，如渲染通道、着色器等        
        RenderPassSpecification sphereSpec;
        sphereSpec.debug_name = "Sphere Pass";
        sphereSpec.target_framebuffer = nullptr; // 直接画到屏幕
        sphereSpec.clear_color = { 0.13f, 0.13f, 0.31f, 1.0f }; // 蓝色背景
        sphereSpec.clear_buffer_flags = ClearBufferFlag::ColorDepth; // 不清除缓冲
        m_sphere_pass = std::make_shared<SpherePass>(sphereSpec);

        RenderPassSpecification containerSpec;
        containerSpec.debug_name = "Container Pass";
        containerSpec.clear_buffer_flags = ClearBufferFlag::None;
        m_container_pass = std::make_shared<ContainerPass>(containerSpec);

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

        // 天空盒
        m_skybox_pass->setCamera(camera);
        m_skybox_pass->run();
    }
} // namespace NexAur
