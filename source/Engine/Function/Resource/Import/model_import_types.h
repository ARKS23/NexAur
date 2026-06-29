#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "Core/Base.h"

namespace NexAur {
    enum class ModelImportMode {
        MetadataOnly,
        FullModel
    };

    enum class TangentGenerationPolicy {
        PreserveImportedTangents,
        GenerateIfMissing,
        None
    };

    struct NEXAUR_API ModelImportRequest {
        std::filesystem::path path;
        ModelImportMode mode = ModelImportMode::MetadataOnly;
        TangentGenerationPolicy tangent_policy = TangentGenerationPolicy::PreserveImportedTangents;
    };

    struct NEXAUR_API ModelImportMetadata {
        std::string source_path;
        std::string generator;
        std::string asset_version;
        int default_scene = -1;
        size_t scene_count = 0;
        size_t node_count = 0;
        size_t mesh_count = 0;
        size_t primitive_count = 0;
        size_t material_count = 0;
        size_t texture_count = 0;
        size_t image_count = 0;
        size_t sampler_count = 0;
        size_t animation_count = 0;
        size_t skin_count = 0;
        std::vector<std::string> extensions_used;
        std::vector<std::string> extensions_required;
    };

    struct NEXAUR_API ModelImportResult {
        bool success = false;
        ModelImportMetadata metadata;
        std::vector<std::string> warnings;
        std::vector<std::string> errors;

        explicit operator bool() const { return success; }
    };
} // namespace NexAur
