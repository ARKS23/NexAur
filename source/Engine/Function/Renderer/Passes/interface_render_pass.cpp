#include "pch.h"
#include "interface_render_pass.h"

#include "Function/Renderer/RHI/renderer_command.h"

#include <algorithm>

namespace NexAur {
    void IRenderPass::run(const RenderPassContext& pass_context, const ResolvedRenderDataPacket& render_data) {
        begin(pass_context);
        execute(pass_context, render_data);
        end(pass_context);
    }

    void IRenderPass::begin(const RenderPassContext& pass_context) {
        std::shared_ptr<Framebuffer> target_framebuffer =
            m_specification.target_framebuffer ? m_specification.target_framebuffer : pass_context.viewport_framebuffer;

        if (target_framebuffer) {
            target_framebuffer->bind();
            const FramebufferSpecification& spec = target_framebuffer->getSpecification();
            RendererCommand::setViewport(0, 0, std::max(1u, spec.width), std::max(1u, spec.height));
        } else {
            RendererCommand::bindDefaultFramebuffer();
            RendererCommand::setViewport(0, 0, pass_context.viewport_width, pass_context.viewport_height);
        }

        RendererCommand::setClearColor(m_specification.clear_color);
        RendererCommand::clear(m_specification.clear_buffer_flags);
    }

    void IRenderPass::end(const RenderPassContext& pass_context) {
        (void)pass_context;

        // 显式 target 是 Pass 自己拥有的临时目标，默认 viewport target 则由 RendererSystem 控制生命周期。
        if (m_specification.target_framebuffer) {
            m_specification.target_framebuffer->unbind();
        }
    }
} // namespace NexAur
