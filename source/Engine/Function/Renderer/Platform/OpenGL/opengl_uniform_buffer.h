#pragma once

#include "Core/Base.h"
#include "Function/Renderer/RHI/uniform_buffer.h"

namespace NexAur {
    class OpenGLUniformBuffer : public UniformBuffer {
    public:
        OpenGLUniformBuffer(uint32_t size, uint32_t binding);
        virtual ~OpenGLUniformBuffer() override;

        virtual void setData(const void* data, uint32_t size, uint32_t offset = 0) override;

    private:
        uint32_t m_ID = 0;
    };
} // namespace NexAur
