#include "pch.h"
#include "interface_render_pass.h"
#include "Function/Renderer/RHI/renderer_command.h"
#include "Function/Renderer/window_system.h"
#include "Function/Global/global_context.h"

namespace NexAur {
    void IRenderPass::run(const RenderDataPacket& render_data) {
        begin();
        execute(render_data);
        end();
    }

    void IRenderPass::begin() {
        if (m_specification.target_framebuffer) {
            m_specification.target_framebuffer->bind();
        } 
        else {
            RendererCommand::bindDefaultFramebuffer();
            auto [width, height] = g_runtime_global_context.m_window_system->getWindowSize();
            RendererCommand::setViewport(0, 0, width, height);
        }

        // 清除缓冲区
        RendererCommand::setClearColor(m_specification.clear_color);
        RendererCommand::clear(m_specification.clear_buffer_flags);
    }

    void IRenderPass::end() {
        if (m_specification.target_framebuffer) {
            m_specification.target_framebuffer->unbind();
        }
    }

} // namespace NexAur
