#pragma once
#include "Core/Base.h"
#include "Function/Renderer/RHI/framebuffer.h"
#include "Function/Renderer/data/render_data.h"

namespace NexAur {
    struct RenderPassSpecificationV2 {
        std::shared_ptr<Framebuffer> target_framebuffer = nullptr;  // 渲染目标,nullptr代表渲染到默认窗口
       
        glm::vec4 clear_color = { 0.1f, 0.1f, 0.1f, 1.0f };      // 清屏颜色
        ClearBufferFlag clear_buffer_flags = ClearBufferFlag::ColorDepth; // 清除缓冲区标志位
        
        std::string debug_name = "RenderPass";                  // 调试用名称
    };

    class NEXAUR_API IRenderPass {
    public:
        IRenderPass(const RenderPassSpecificationV2& spec) : m_specification(spec) {}
        virtual ~IRenderPass() = default;

        std::shared_ptr<Framebuffer> getTargetFramebuffer() const { return m_specification.target_framebuffer; }
        void run(const ResolvedRenderDataPacket& render_data);      // run调用begin和end包裹execute, execute由子类实现具体渲染逻辑
        virtual void run_without_begin_end(const ResolvedRenderDataPacket& render_data) {}; // 子类自行实现begin和end的run版本，不受默认流程限制

    protected:
        virtual void execute(const ResolvedRenderDataPacket& render_data) = 0;

    protected:
        RenderPassSpecificationV2 m_specification;

    private:
        void begin();
        void end();
    };
} // namespace NexAur
