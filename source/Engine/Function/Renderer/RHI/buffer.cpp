#include "pch.h"
#include "buffer.h"
#include "Function/Renderer/Platform/OpenGL/opengl_buffer.h"

namespace NexAur {
    std::shared_ptr<VertexBuffer> VertexBuffer::create(uint32_t size) {
        return std::shared_ptr<OpenGLVertexBuffer>(new OpenGLVertexBuffer(size));
    }

    std::shared_ptr<VertexBuffer> VertexBuffer::create(float* vertices, uint32_t size) {
        return std::shared_ptr<OpenGLVertexBuffer>(new OpenGLVertexBuffer(vertices, size));
    }

    std::shared_ptr<IndexBuffer> IndexBuffer::create(uint32_t* indices, uint32_t count) {
        return std::shared_ptr<OpenGLIndexBuffer>(new OpenGLIndexBuffer(indices, count));
    }
} // namespace NexAur
