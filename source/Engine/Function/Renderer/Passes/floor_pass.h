#pragma once

#include "Core/Base.h"
#include "Function/Renderer/RHI/render_pass.h"
#include "Function/Renderer/RHI/vertex_array.h"
#include "Function/Renderer/RHI/shader.h"
#include "Function/Renderer/RHI/buffer.h"
#include "Function/Renderer/RHI/texture.h"
#include "Function/Renderer/RHI/material.h"

namespace NexAur {
    class FloorPass : public RenderPass {
    public:
        FloorPass(const RenderPassSpecification& spec) : RenderPass(spec) {
            initResources();
        }

        virtual ~FloorPass() = default;

        virtual void execute() override;

    private:
        void initResources();

    private:
        std::shared_ptr<VertexArray> m_vertex_array;
        std::shared_ptr<Shader> m_shader;
        std::shared_ptr<Texture> m_texture;
        std::shared_ptr<Material> m_material;
    };
} // namespace NexAur
