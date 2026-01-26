#pragma once

#include "Core/Base.h"
#include "Function/Renderer/RHI/renderer_api.h"

namespace NexAur {
    class OpenGLRendererAPI : public RendererAPI {
    public:
        virtual void init() override;
        virtual void setViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) override;
        virtual void setClearColor(const glm::vec4& color) override;
        virtual void clear(ClearBufferFlag flags = ClearBufferFlag::ColorDepth) override;

        virtual void drawIndex(const std::shared_ptr<VertexArray>& vertex_array, uint32_t index_count = 0) override;
        virtual void drawLines(const std::shared_ptr<VertexArray>& vertex_array, uint32_t vertex_count) override;
        virtual void drawTriangles(const std::shared_ptr<VertexArray>& vertex_array, uint32_t vertex_count) override;

        virtual void setLineWidth(float width) override;
        virtual void setDepthFunc(DepthFunc depth_func) override;
        virtual void setDepthMask(bool writable) override;
    };
} // namespace NexAur
