#pragma once

#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>

#include "Core/Base.h"

namespace NexAur {
    class AssetManager;
    class SceneV2;

    struct SceneSerializationResult {
        bool success = false;
        std::string message;
        size_t entity_count = 0;

        explicit operator bool() const { return success; }
    };

    struct SceneLoadResult {
        bool success = false;
        std::string message;
        size_t entity_count = 0;
        std::shared_ptr<SceneV2> scene;

        explicit operator bool() const { return success; }
    };

    class NEXAUR_API SceneSerializer final {
    public:
        explicit SceneSerializer(AssetManager& asset_manager);

        SceneSerializationResult save(
            const SceneV2& scene,
            const std::filesystem::path& path) const;

        SceneLoadResult load(const std::filesystem::path& path) const;

    private:
        AssetManager& m_asset_manager;
    };
} // namespace NexAur
