#pragma once
#include <type_traits>

// 位运算代码生成宏
#define ENABLE_ENUM_BITMASK(EnumType)                                             \
inline constexpr EnumType operator|(EnumType a, EnumType b) noexcept              \
{                                                                                 \
    using U = std::underlying_type_t<EnumType>;                                   \
    return static_cast<EnumType>(static_cast<U>(a) | static_cast<U>(b));          \
}                                                                                 \
inline constexpr EnumType operator&(EnumType a, EnumType b) noexcept              \
{                                                                                 \
    using U = std::underlying_type_t<EnumType>;                                   \
    return static_cast<EnumType>(static_cast<U>(a) & static_cast<U>(b));          \
}                                                                                 \
inline constexpr EnumType operator^(EnumType a, EnumType b) noexcept              \
{                                                                                 \
    using U = std::underlying_type_t<EnumType>;                                   \
    return static_cast<EnumType>(static_cast<U>(a) ^ static_cast<U>(b));          \
}                                                                                 \
inline constexpr EnumType operator~(EnumType a) noexcept                           \
{                                                                                 \
    using U = std::underlying_type_t<EnumType>;                                   \
    return static_cast<EnumType>(~static_cast<U>(a));                             \
}                                                                                 \
inline EnumType& operator|=(EnumType& a, EnumType b) noexcept                      \
{                                                                                 \
    return a = (a | b);                                                           \
}                                                                                 \
inline EnumType& operator&=(EnumType& a, EnumType b) noexcept                      \
{                                                                                 \
    return a = (a & b);                                                           \
}                                                                                 \
inline EnumType& operator^=(EnumType& a, EnumType b) noexcept                      \
{                                                                                 \
    return a = (a ^ b);                                                           \
}