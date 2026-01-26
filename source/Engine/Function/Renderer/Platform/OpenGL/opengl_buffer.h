#pragma once

#include "Core/Base.h"
#include "Function/Renderer/RHI/buffer.h"

namespace NexAur {
    class OpenGLVertexBuffer : public VertexBuffer {
    public:
        OpenGLVertexBuffer(uint32_t size);
        OpenGLVertexBuffer(float* vertices, uint32_t size);
        virtual ~OpenGLVertexBuffer() override;

        virtual void bind() const override;
        virtual void unbind() const override;

        virtual void setData(const void* data, uint32_t size) override;

        virtual const BufferLayout& getLayout() const override;
        virtual void setLayout(const BufferLayout& layout) override;

    private:
        uint32_t m_ID = 0;
        BufferLayout m_layout;
    };


    class OpenGLIndexBuffer : public IndexBuffer {
    public:
        OpenGLIndexBuffer(uint32_t* indices, uint32_t count);
        virtual ~OpenGLIndexBuffer() override;

        virtual void bind() const override;
        virtual void unbind() const override;

        virtual uint32_t getCount() const override;

    private:
        uint32_t m_ID = 0;
        uint32_t m_count = 0;
    };
} // namespace NexAur
