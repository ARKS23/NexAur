#pragma once

#include <cstdint>
#include <functional>

#include "Core/UUID.h"

namespace NexAur {
    enum class AssetType : uint8_t {
        Unknown = 0,
        Model,
        Texture2D,
        TextureCube,
        Shader,
        EnvironmentMap,
        ProceduralModel
    };

    inline const char* assetTypeToString(AssetType type) {
        switch (type) {
            case AssetType::Model: return "Model";
            case AssetType::Texture2D: return "Texture2D";
            case AssetType::TextureCube: return "TextureCube";
            case AssetType::Shader: return "Shader";
            case AssetType::EnvironmentMap: return "EnvironmentMap";
            case AssetType::ProceduralModel: return "ProceduralModel";
            default: return "Unknown";
        }
    }

    // 资产句柄是可序列化的资源身份，不等价于底层 GPU 对象句柄。
    struct AssetHandle {
        UUID id = INVALID_UUID;

        AssetHandle() = default;
        explicit AssetHandle(UUID uuid) : id(uuid) {}

        bool isValid() const { return id != INVALID_UUID; }
        explicit operator bool() const { return isValid(); }

        bool operator==(const AssetHandle& other) const { return static_cast<uint64_t>(id) == static_cast<uint64_t>(other.id); }
        bool operator!=(const AssetHandle& other) const { return !(*this == other); }
    };
} // namespace NexAur

namespace std {
    template<>
    struct hash<NexAur::AssetHandle> {
        size_t operator()(const NexAur::AssetHandle& handle) const {
            return std::hash<NexAur::UUID>{}(handle.id);
        }
    };
} // namespace std
