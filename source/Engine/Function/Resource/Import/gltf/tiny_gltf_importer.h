#pragma once

#include "Core/Base.h"
#include "Function/Resource/Import/model_importer.h"

namespace NexAur {
    class NEXAUR_API TinyGltfImporter final : public IModelImporter {
    public:
        bool canImport(const std::filesystem::path& path) const override;
        ModelImportResult importModel(const ModelImportRequest& request) const override;
    };
} // namespace NexAur
