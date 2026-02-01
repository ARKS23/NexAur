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
    class ContainerPass : public RenderPass {
    public:
        ContainerPass(const RenderPassSpecification& spec) : RenderPass(spec) {
            initResources();
        }

        virtual ~ContainerPass() = default;

        virtual void execute() override;

    private:
        void initResources();

    private:
        std::shared_ptr<VertexArray> m_vertex_array;
        std::shared_ptr<Shader> m_shader;
        std::shared_ptr<Texture> m_texture;
        std::shared_ptr<Material> m_material;
        Clock m_clock;
    };
} // namespace NexAur
