#pragma once

#include <filesystem>
#include <memory>
#include <vector>

#include "Core/Base.h"
#include "Function/Resource/Import/model_importer.h"

namespace NexAur {
    class NEXAUR_API ModelImporterRegistry {
    public:
        ModelImporterRegistry() = default;
        ~ModelImporterRegistry() = default;
        ModelImporterRegistry(const ModelImporterRegistry&) = delete;
        ModelImporterRegistry& operator=(const ModelImporterRegistry&) = delete;
        ModelImporterRegistry(ModelImporterRegistry&&) noexcept = default;
        ModelImporterRegistry& operator=(ModelImporterRegistry&&) noexcept = default;

        void registerImporter(std::unique_ptr<IModelImporter> importer);
        void clear();

        const IModelImporter* findImporter(const std::filesystem::path& path) const;
        ModelImportResult importModel(const ModelImportRequest& request) const;

        size_t getImporterCount() const { return m_importers.size(); }

    private:
        std::vector<std::unique_ptr<IModelImporter>> m_importers;
    };
} // namespace NexAur
