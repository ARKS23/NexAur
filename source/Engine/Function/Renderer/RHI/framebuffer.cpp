#include "pch.h"
#include "framebuffer.h"
#include "Function/Renderer/Platform/OpenGL/opengl_framebuffer.h"

namespace NexAur {
    std::shared_ptr<Framebuffer> Framebuffer::create(const FramebufferSpecification& spec) {
        return std::make_shared<OpenGLFramebuffer>(spec);
    }
} // namespace NexAur