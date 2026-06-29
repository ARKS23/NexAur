#include "pch.h"
#include "Function/Resource/Import/model_importer_registry.h"

namespace NexAur {
    void ModelImporterRegistry::registerImporter(std::unique_ptr<IModelImporter> importer) {
        if (importer) {
            m_importers.push_back(std::move(importer));
        }
    }

    void ModelImporterRegistry::clear() {
        m_importers.clear();
    }

    const IModelImporter* ModelImporterRegistry::findImporter(const std::filesystem::path& path) const {
        for (const std::unique_ptr<IModelImporter>& importer : m_importers) {
            if (importer && importer->canImport(path)) {
                return importer.get();
            }
        }

        return nullptr;
    }

    ModelImportResult ModelImporterRegistry::importModel(const ModelImportRequest& request) const {
        ModelImportResult result;

        const IModelImporter* importer = findImporter(request.path);
        if (!importer) {
            result.errors.push_back("No model importer registered for: " + request.path.string());
            return result;
        }

        return importer->importModel(request);
    }
} // namespace NexAur
