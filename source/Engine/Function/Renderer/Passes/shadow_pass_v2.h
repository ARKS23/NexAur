#pragma once

#include "Core/Base.h"
#include "Function/Renderer/Passes/interface_render_pass.h"
#include "Function/Renderer/RHI/shader.h"
#include "Function/Renderer/RHI/framebuffer.h"

namespace NexAur {
    class Texture2D;

    class ShadowPassV2 : public IRenderPass {
    public:
        ShadowPassV2(const RenderPassSpecificationV2& spec);
        virtual ~ShadowPassV2() = default;

        glm::mat4 getLightSpaceMatrix() const { return m_light_space_matrix; }
        std::shared_ptr<Texture2D> getDepthMapTexture() const {return m_depth_texture;};

        virtual void run_without_begin_end(const ResolvedRenderDataPacket& render_data) override;

    private:
        void initResources();

        virtual void execute(const ResolvedRenderDataPacket& render_data) override {} // 不走默认管线流程，单独调用run_without_begin_end

    private:
        std::shared_ptr<Shader> m_shader;
        std::shared_ptr<Framebuffer> m_framebuffer;
        std::shared_ptr<Texture2D> m_depth_texture;
        glm::mat4 m_light_space_matrix; // 光空间矩阵，用于将世界坐标转换到光源视角

        const uint32_t SHADOW_WIDTH = 4096;
        const uint32_t SHADOW_HEIGHT = 4096;

        // 正交投影参数
        const float ORTHO_LEFT = -70.0f;
        const float ORTHO_RIGHT = 70.0f;
        const float ORTHO_BOTTOM = -70.0f;
        const float ORTHO_TOP = 70.0f;
        const float ORTHO_NEAR = 1.0f;
        const float ORTHO_FAR = 100.0f;
    };
} // namespace NexAur
