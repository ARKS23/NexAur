#pragma once

#include "Core/Base.h"

namespace NexAur {
    class NEXAUR_API UniformBuffer {
    public:
        virtual ~UniformBuffer() {}
		virtual void SetData(const void* data, uint32_t size, uint32_t offset = 0) = 0; //上传数据的接口

        static std::shared_ptr<UniformBuffer> create(uint32_t size, uint32_t binding); // 第二个参数是绑定点
    };
} // namespace NexAur
