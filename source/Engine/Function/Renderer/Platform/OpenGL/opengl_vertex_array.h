#pragma once

#include "Core/Base.h"
#include "Function/Renderer/RHI/vertex_array.h"

namespace NexAur {
    class OpenGLVertexArray : public VertexArray {
    public:
        OpenGLVertexArray();
        virtual ~OpenGLVertexArray() override;

        virtual void bind() const override;
        virtual void unbind() const override;

        virtual void addVertexBuffer(const std::shared_ptr<VertexBuffer>& vertexBuffer) override; // 添加VBO
        virtual void setIndexBuffer(const std::shared_ptr<IndexBuffer>& indexBuffer) override; // 添加EBO

        virtual const std::vector<std::shared_ptr<VertexBuffer>>& getVertexBuffers() const override; // 获取当前持有的VBO Buffer
        virtual const std::shared_ptr<IndexBuffer>& getIndexBuffer() const override;  // 获取当前的EBO

    private:
        uint32_t m_ID;
        uint32_t m_vertexbuffer_index = 0; // 用于管理GLSL里的layout(location = x)
        std::vector<std::shared_ptr<VertexBuffer>> m_vertex_buffers;
        std::shared_ptr<IndexBuffer> m_index_buffer;
    };
} // namespace NexAur