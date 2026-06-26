#pragma once

#include <string>

#include "Function/Resource/asset_handle.h"
#include "Function/Resource/texture_types.h"

namespace NexAur {
    struct AssetMetadata {
        AssetHandle handle;
        AssetType type = AssetType::Unknown;
        std::string path;
        std::string debug_name;
        TextureColorSpace texture_color_space = TextureColorSpace::SRGB;
        bool runtime_generated = false;
    };
} // namespace NexAur
