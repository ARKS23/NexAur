#pragma once

#include "Core/Base.h"
#include "Function/Renderer/RHI/render_pass.h"
#include "Function/Renderer/RHI/vertex_array.h"
#include "Function/Renderer/RHI/shader.h"
#include "Function/Renderer/RHI/buffer.h"

namespace NexAur {
    class TrianglePass : public RenderPass {
    public:
        TrianglePass(const RenderPassSpecification& spec) : RenderPass(spec) {
            initResources();
        }

        virtual ~TrianglePass() = default;

    protected:
        virtual void execute() override;

    private:
        void initResources();

    private:
        std::shared_ptr<VertexArray> m_vertex_array;
        std::shared_ptr<Shader> m_shader;
    };
} // namespace NexAur
