#pragma once
#include <vector>
#include <memory>

#include "Core/Base.h"
#include "buffer.h"

namespace NexAur {
    class NEXAUR_API VertexArray {
    public:
        virtual ~VertexArray() = default;

        virtual void bind() const = 0;
        virtual void unbind() const = 0;

        virtual void addVertexBuffer(const std::shared_ptr<VertexBuffer>& vertexBuffer) = 0; // 添加VBO
        virtual void setIndexBuffer(const std::shared_ptr<IndexBuffer>& indexBuffer) = 0; // 添加EBO

        virtual const std::vector<std::shared_ptr<VertexBuffer>>& getVertexBuffers() const = 0; // 获取当前持有的VBO Buffer
        virtual const std::shared_ptr<IndexBuffer>& getIndexBuffer() const = 0;  // 获取当前的EBO

    public:
        static std::shared_ptr<VertexArray> create();
};
} // namespace NexAur 