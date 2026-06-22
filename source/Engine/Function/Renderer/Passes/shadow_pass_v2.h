#pragma once

#include "Core/Base.h"
#include "Function/Renderer/Passes/interface_render_pass.h"
#include "Function/Renderer/RHI/framebuffer.h"
#include "Function/Renderer/RHI/shader.h"

namespace NexAur {
    class Texture2D;

    class ShadowPassV2 : public IRenderPass {
    public:
        explicit ShadowPassV2(const RenderPassSpecificationV2& spec);
        ~ShadowPassV2() override = default;

        glm::mat4 getLightSpaceMatrix() const { return m_light_space_matrix; }
        std::shared_ptr<Texture2D> getDepthMapTexture() const { return m_depth_texture; }

        void run_without_begin_end(const RenderPassContext& pass_context, const ResolvedRenderDataPacket& render_data) override;

    private:
        void initResources();
        void execute(const RenderPassContext& pass_context, const ResolvedRenderDataPacket& render_data) override {
            (void)pass_context;
            (void)render_data;
        }

    private:
        std::shared_ptr<Shader> m_shader;
        std::shared_ptr<Framebuffer> m_framebuffer;
        std::shared_ptr<Texture2D> m_depth_texture;
        glm::mat4 m_light_space_matrix;

        const uint32_t SHADOW_WIDTH = 4096;
        const uint32_t SHADOW_HEIGHT = 4096;

        const float ORTHO_LEFT = -70.0f;
        const float ORTHO_RIGHT = 70.0f;
        const float ORTHO_BOTTOM = -70.0f;
        const float ORTHO_TOP = 70.0f;
        const float ORTHO_NEAR = 1.0f;
        const float ORTHO_FAR = 100.0f;
    };
} // namespace NexAur
