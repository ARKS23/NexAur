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

        virtual void run_without_begin_end(const RenderDataPacket& render_data) override;

    private:
        void initResources();

        virtual void execute(const RenderDataPacket& render_data) override {}

    private:
        std::shared_ptr<Shader> m_shader;
        std::shared_ptr<Framebuffer> m_framebuffer;
        std::shared_ptr<Texture2D> m_depth_texture;
        glm::mat4 m_light_space_matrix; // 光空间矩阵，用于将世界坐标转换到光源视角

        const uint32_t SHADOW_WIDTH = 4096;
        const uint32_t SHADOW_HEIGHT = 4096;
    };
} // namespace NexAur
