#include "pch.h"
#include "opengl_buffer.h"

namespace NexAur {
    OpenGLVertexBuffer::OpenGLVertexBuffer(uint32_t size) {
        glCreateBuffers(1, &m_ID);
        glBindBuffer(GL_ARRAY_BUFFER, m_ID);
        glBufferData(GL_ARRAY_BUFFER, size, nullptr, GL_DYNAMIC_DRAW);
    }

    OpenGLVertexBuffer::OpenGLVertexBuffer(float* vertices, uint32_t size) {
        glCreateBuffers(1, &m_ID);
        glBindBuffer(GL_ARRAY_BUFFER, m_ID);
        glBufferData(GL_ARRAY_BUFFER, size, vertices, GL_STATIC_DRAW);
    }

    OpenGLVertexBuffer::~OpenGLVertexBuffer() {
        glDeleteBuffers(1, &m_ID);
    }

    void OpenGLVertexBuffer::bind() const {
        glBindBuffer(GL_ARRAY_BUFFER, m_ID);
    }

    void OpenGLVertexBuffer::unbind() const {
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    void OpenGLVertexBuffer::setData(const void* data, uint32_t size) {
        glBindBuffer(GL_ARRAY_BUFFER, m_ID);
        glBufferSubData(GL_ARRAY_BUFFER, 0, size, data);
    }

    const BufferLayout& OpenGLVertexBuffer::getLayout() const {
        return m_layout;
    }

    void OpenGLVertexBuffer::setLayout(const BufferLayout& layout) {
        m_layout = layout;
    }

    /* ---------------------- OpenGLIndexBuffer ---------------------- */
    OpenGLIndexBuffer::OpenGLIndexBuffer(uint32_t* indices, uint32_t count) : m_count(count) {
        glCreateBuffers(1, &m_ID);
        // 如果构造时没有VAO被绑定，此时GL_ELEMENT_ARRAY_BUFFER是非法的，因此先用全局插槽把数据存入显存
        glBindBuffer(GL_ARRAY_BUFFER, m_ID);
        glBufferData(GL_ARRAY_BUFFER, count * sizeof(uint32_t), indices, GL_STATIC_DRAW);
    }

    OpenGLIndexBuffer::~OpenGLIndexBuffer() {
        glDeleteBuffers(1, &m_ID);
    }

    void OpenGLIndexBuffer::bind() const {
        // 等真正要用的时候再作为索引读取
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ID);
    }

    void OpenGLIndexBuffer::unbind() const {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }

    uint32_t OpenGLIndexBuffer::getCount() const {
        return m_count;
    }
} // namespace NexAur