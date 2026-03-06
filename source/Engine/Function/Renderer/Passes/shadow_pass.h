#pragma once

#include "Core/Base.h"
#include "Function/Renderer/RHI/render_pass.h"
#include "Function/Renderer/RHI/shader.h"
#include "Function/Renderer/RHI/framebuffer.h"
#include "Function/Scene/scene.h"

namespace NexAur {
    class Texture2D;

    class ShadowPass : public RenderPass {
    public:
        ShadowPass(const RenderPassSpecification& spec);
        virtual ~ShadowPass() = default;

        virtual void execute() override; // 不使用这个函数，这个函数需要后期优化

        void execute(std::shared_ptr<Scene> scene);

        glm::mat4 getLightSpaceMatrix() const { return m_light_space_matrix; }
        std::shared_ptr<Texture2D> getDepthMapTexture() const;

    private:
        void initResources();

    private:
        std::shared_ptr<Shader> m_shader;
        std::shared_ptr<Framebuffer> m_framebuffer;
        std::shared_ptr<Texture2D> m_depth_texture;
        glm::mat4 m_light_space_matrix; // 光空间矩阵，用于将世界坐标转换到光源视角

        const uint32_t SHADOW_WIDTH = 4096;
        const uint32_t SHADOW_HEIGHT = 4096;
    };
} // namespace NexAur