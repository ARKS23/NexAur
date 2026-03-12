#include "pch.h"
#include "UUID.h"

namespace NexAur {
    // 匿名空间把随机数生成器和分布限制在当前编译单元，避免外部访问
    namespace {
        static std::random_device rd;       // 随机数种子
        static std::mt19937_64 s_engine(rd()); // 64位Mersenne Twister引擎
        static std::uniform_int_distribution<uint64_t> s_udist; // 均匀分布生成64位整数
    } // namespace 

    UUID::UUID() : m_UUID(s_udist(s_engine)) {} // 默认构造函数生成随机UUID
    UUID::UUID(uint64_t uuid) : m_UUID(uuid) {}
} // namespace NexAur