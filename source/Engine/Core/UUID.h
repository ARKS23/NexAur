#pragma once
#include <cstdint>
#include <random>

namespace NexAur {
    // UUID作为资源的唯一识别码
    class UUID {
    public:
        UUID(); // 默认随机生成64位无符号整数
        UUID(uint64_t uuid);
        UUID(const UUID&) = default;

        operator uint64_t() const { return m_UUID; }

    private:
        uint64_t m_UUID;
    };

    constexpr uint64_t INVALID_UUID = 0;  // 无效UUID常量
} // namespace NexAur


// std::unordered_map特化，让UUID可以作为key使用
namespace std {
    template<typename T> struct hash;

    template<>
    struct hash<NexAur::UUID> {
        size_t operator()(const NexAur::UUID& uuid) const {
            return (uint64_t)uuid;
        }
    };
} // namespace std

