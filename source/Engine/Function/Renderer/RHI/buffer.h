#pragma once
#include <cstdint>

#include <memory>

#include "Core/Base.h"
#include "definitions.h"
#include "Core/Log/log_system.h"

namespace NexAur {
    //根据类型返回对应的字节数大小，之后计算步长需要用到
    static uint32_t ShaderDataTypeSize(ShaderDataType type) {
        switch (type)
        {
            case NexAur::ShaderDataType::Float:
                return 4;
            case NexAur::ShaderDataType::Float2:
                return 4 * 2;
            case NexAur::ShaderDataType::Float3:
                return 4 * 3;
            case NexAur::ShaderDataType::Float4:
                return 4 * 4;
            case NexAur::ShaderDataType::Mat3:
                return 4 * 3 * 3;
            case NexAur::ShaderDataType::Mat4:
                return 4 * 4 * 4;
            case NexAur::ShaderDataType::Int:
                return 4;
            case NexAur::ShaderDataType::Int2:
                return 4 * 2;
            case NexAur::ShaderDataType::Int3:
                return 4 * 3;
            case NexAur::ShaderDataType::Int4:
                return 4 * 4;
            case NexAur::ShaderDataType::Bool:
                return 1;
        }

        NX_CORE_WARN("ShaderDataTypeSize::Unknow ShaderDataType");
        return 0;
    }


    // 描述单个顶点数据属性的封装数据结构
    struct NEXAUR_API BufferElement {
    public:
        std::string name;		// 调试名称
        ShaderDataType type;	// 数据类型
        uint32_t size;			// 占多少字节
        size_t Offset;			// 一个顶点数据块中的偏移量
        bool need_normalized;	// 是否需要归一化

    public:
        BufferElement() = default;
        BufferElement(ShaderDataType type_, const std::string& name_, bool normalized_ = false)
            : name(name_), type(type_), size(ShaderDataTypeSize(type)), Offset(0), need_normalized(normalized_) {}

    public:
        uint32_t getComponentCount() const {
            /* 在AddVertexBuffer中被调用，告诉该属性由几个基本单位组成,比如位置(x, y, z)是3 */
            switch (type)
            {
                case ShaderDataType::Float:   return 1;
                case ShaderDataType::Float2:  return 2;
                case ShaderDataType::Float3:  return 3;
                case ShaderDataType::Float4:  return 4;
                case ShaderDataType::Mat3:    return 3; // 3 * float3 = 3 * vec3
                case ShaderDataType::Mat4:    return 4; // 4 * float4 = 4 * vec4
                case ShaderDataType::Int:     return 1;
                case ShaderDataType::Int2:    return 2;
                case ShaderDataType::Int3:    return 3;
                case ShaderDataType::Int4:    return 4;
                case ShaderDataType::Bool:    return 1;
            }

            NX_CORE_WARN("BufferElement::GetComponentCount::Unknown ShaderDataType!");
            return 0;
        }
    };

    class NEXAUR_API BufferLayout {
    public:
        BufferLayout() {}
        BufferLayout(std::initializer_list<BufferElement> elements) : m_elements(elements) {
            calculateOffsetAndStride();
        }

    public:
        uint32_t getStride() const { return m_stride; }
        const std::vector<BufferElement>& getElements() const { return m_elements; }

    public:
        std::vector<BufferElement>::iterator begin() { return m_elements.begin(); }
        std::vector<BufferElement>::iterator end() { return m_elements.end(); }
        std::vector<BufferElement>::const_iterator begin() const { return m_elements.begin(); }
        std::vector<BufferElement>::const_iterator end() const { return m_elements.end(); }

    private:
        void calculateOffsetAndStride() {
            /* 笔记中有该函数的解释 */
            size_t offset = 0;
            m_stride = 0;
            for (BufferElement& element : m_elements) {
                element.Offset = offset;	// 当前元素的偏移量 = 之前的总大小
                offset += element.size;		// 累加大小
                m_stride += element.size;	// 步长也累加
            }
        }

    private:
        std::vector<BufferElement> m_elements;
        uint32_t m_stride = 0;
    };

    class NEXAUR_API VertexBuffer {
    public:
        virtual ~VertexBuffer() = default;

        virtual void bind() const = 0;
        virtual void unbind() const = 0;

        virtual void setData(const void* data, uint32_t size) = 0;

        virtual const BufferLayout& getLayout() const = 0;
        virtual void setLayout(const BufferLayout& layout) = 0;

        static std::shared_ptr<VertexBuffer> create(uint32_t size);
        static std::shared_ptr<VertexBuffer> create(float* vertices, uint32_t size);
    };


    class NEXAUR_API IndexBuffer {
    public:
        virtual ~IndexBuffer() = default;

        virtual void bind() const = 0;
        virtual void unbind() const = 0;

        virtual uint32_t getCount() const = 0;

        static std::shared_ptr<VertexBuffer> create(uint32_t* indices, uint32_t count);
    };
} // namespace NexAur

