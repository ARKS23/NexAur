#pragma once

#include "Core/Base.h"
#include "Function/Renderer/RHI/render_pass.h"
#include "Function/Renderer/RHI/vertex_array.h"
#include "Function/Renderer/RHI/shader.h"
#include "Function/Renderer/RHI/buffer.h"
#include "Function/Renderer/RHI/texture.h"
#include "Function/Renderer/RHI/material.h"
#include "Core/Time/Clock.h"

namespace NexAur {
    class Camera;

    class SkyboxPass : public RenderPass {
    public:
        SkyboxPass(const RenderPassSpecification& spec) : RenderPass(spec) {
            initResources();
        }

        virtual ~SkyboxPass() = default;

        virtual void execute() override;

        void setCamera(const std::shared_ptr<Camera>& camera) { m_camera = camera; }

    private:
        void initResources();

    private:
        std::shared_ptr<VertexArray> m_vertex_array;
        std::shared_ptr<Shader> m_shader;
        std::shared_ptr<TextureCubeMap> m_skybox_texture;
        std::shared_ptr<Camera> m_camera;
    };
} // namespace NexAur
