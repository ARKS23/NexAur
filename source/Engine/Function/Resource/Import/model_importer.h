#pragma once

#include <filesystem>

#include "Core/Base.h"
#include "Function/Resource/Import/model_import_types.h"

namespace NexAur {
    class NEXAUR_API IModelImporter {
    public:
        virtual ~IModelImporter() = default;

        virtual bool canImport(const std::filesystem::path& path) const = 0;
        virtual ModelImportResult importModel(const ModelImportRequest& request) const = 0;
    };
} // namespace NexAur
