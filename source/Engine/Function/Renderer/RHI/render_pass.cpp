#include "pch.h"
#include "render_pass.h"
#include "renderer_command.h"
#include "Function/Renderer/window_system.h"
#include "Function/Global/global_context.h"

namespace NexAur {
    void RenderPass::run() {
        begin();
        execute();
        end();
    }

    void RenderPass::begin() {
        if (m_specification.target_framebuffer) {
            m_specification.target_framebuffer->bind();
        } 
        else {
            auto [width, height] = g_runtime_global_context.m_window_system->getWindowSize();
            RendererCommand::setViewport(0, 0, width, height);
        }

        // 清除缓冲区
        RendererCommand::setClearColor(m_specification.clear_color);
        RendererCommand::clear(m_specification.clear_buffer_flags);
    }

    void RenderPass::end() {
        if (m_specification.target_framebuffer) {
            m_specification.target_framebuffer->unbind();
        }
    }
} // namespace NexAur