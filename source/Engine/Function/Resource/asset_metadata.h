#pragma once

#include <string>

#include "Function/Resource/asset_handle.h"

namespace NexAur {
    struct AssetMetadata {
        AssetHandle handle;
        AssetType type = AssetType::Unknown;
        std::string path;
        std::string debug_name;
        bool runtime_generated = false;
    };
} // namespace NexAur
