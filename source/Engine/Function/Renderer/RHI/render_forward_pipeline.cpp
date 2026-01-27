#include "pch.h"
#include "render_forward_pipeline.h"
#include "Function/Renderer/Passes/triangle_pass.h"

namespace NexAur {
    void RenderForwardPipeline::init() {
        // 初始化前向渲染管线所需的资源，如渲染通道、着色器等
        RenderPassSpecification spec;
        spec.debug_name = "Gradient Triangle Pass";
        spec.target_framebuffer = nullptr; // 直接画到屏幕
        spec.clear_color = { 0.13f, 0.34f, 0.51f, 1.0f }; // 蓝色背景
        spec.clear_buffer_flags = ClearBufferFlag::ColorDepth;
        m_triangle_pass = std::make_shared<TrianglePass>(spec);
    }

    void RenderForwardPipeline::render() {
        // 执行前向渲染流程
        m_triangle_pass->run();
    }
} // namespace NexAur
