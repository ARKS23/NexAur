#pragma once

#include "Core/Base.h"
#include "Function/Renderer/RHI/framebuffer.h"

namespace NexAur {
    struct RenderPassSpecification {
        std::shared_ptr<Framebuffer> target_framebuffer = nullptr;  // 渲染目标,nullptr代表渲染到默认窗口
       
        glm::vec4 clear_color = { 0.1f, 0.1f, 0.1f, 1.0f };      // 清屏颜色
        ClearBufferFlag clear_buffer_flags = ClearBufferFlag::ColorDepth; // 清除缓冲区标志位
        
        std::string debug_name = "RenderPass";                  // 调试用名称
    };


    class NEXAUR_API RenderPass {
    public:
        RenderPass(const RenderPassSpecification& spec) : m_specification(spec) {}
        virtual ~RenderPass() = default;

        void run();

    protected:
        virtual void execute() = 0;

    private:
        void begin();
        void end();

    protected:
        RenderPassSpecification m_specification;
    };
} // namespace NexAur
