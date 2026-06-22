#pragma once

#include "Core/Base.h"
#include "Function/Renderer/RHI/framebuffer.h"
#include "Function/Renderer/RHI/render_pass_context.h"
#include "Function/Renderer/data/render_data.h"

namespace NexAur {
    struct RenderPassSpecificationV2 {
        // nullptr 表示使用 RenderPassContext 中的 viewport framebuffer。
        std::shared_ptr<Framebuffer> target_framebuffer = nullptr;

        glm::vec4 clear_color = { 0.1f, 0.1f, 0.1f, 1.0f };
        ClearBufferFlag clear_buffer_flags = ClearBufferFlag::ColorDepth;

        std::string debug_name = "RenderPass";
    };

    class NEXAUR_API IRenderPass {
    public:
        explicit IRenderPass(const RenderPassSpecificationV2& spec) : m_specification(spec) {}
        virtual ~IRenderPass() = default;

        std::shared_ptr<Framebuffer> getTargetFramebuffer() const { return m_specification.target_framebuffer; }

        void run(const RenderPassContext& pass_context, const ResolvedRenderDataPacket& render_data);
        virtual void run_without_begin_end(const RenderPassContext& pass_context, const ResolvedRenderDataPacket& render_data) {
            (void)pass_context;
            (void)render_data;
        }

    protected:
        virtual void execute(const RenderPassContext& pass_context, const ResolvedRenderDataPacket& render_data) = 0;

    protected:
        RenderPassSpecificationV2 m_specification;

    private:
        void begin(const RenderPassContext& pass_context);
        void end(const RenderPassContext& pass_context);
    };
} // namespace NexAur
