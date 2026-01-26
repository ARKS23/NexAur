#include "pch.h"
#include "vertex_array.h"
#include "Function/Renderer/Platform/OpenGL/opengl_vertex_array.h"

namespace NexAur {
    std::shared_ptr<VertexArray> VertexArray::create() {
        return std::make_shared<OpenGLVertexArray>();
    }
} // namespace NexAur
