#include "pch.h"
#include "uniform_buffer.h"
#include "Function/Renderer/Platform/OpenGL/opengl_uniform_buffer.h"

namespace NexAur {
    std::shared_ptr<UniformBuffer> UniformBuffer::create(uint32_t size, uint32_t binding) {
        return std::make_shared<OpenGLUniformBuffer>(size, binding);
    }
} // namespace NexAur