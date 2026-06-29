#include "pch.h"
#include "Function/Resource/Import/gltf/tiny_gltf_importer.h"

#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include <tiny_gltf.h>

#include <cctype>
#include <filesystem>

namespace NexAur {
    namespace {
        std::string toLower(std::string value) {
            for (char& character : value) {
                character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
            }
            return value;
        }

        bool loadMetadataImage(
            tinygltf::Image* image,
            const int,
            std::string* err,
            std::string*,
            int,
            int,
            const unsigned char*,
            int,
            void*) {
            if (!image) {
                if (err) {
                    *err += "TinyGltf metadata image loader received a null image.\n";
                }
                return false;
            }

            image->image.clear();
            image->as_is = true;
            if (image->bits < 0) {
                image->bits = 8;
            }
            if (image->pixel_type < 0) {
                image->pixel_type = TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE;
            }
            return true;
        }

        void appendMessage(std::vector<std::string>& messages, const std::string& message) {
            if (!message.empty()) {
                messages.push_back(message);
            }
        }

        ModelImportMetadata buildMetadata(const std::filesystem::path& path, const tinygltf::Model& model) {
            ModelImportMetadata metadata;
            metadata.source_path = path.string();
            metadata.generator = model.asset.generator;
            metadata.asset_version = model.asset.version;
            metadata.default_scene = model.defaultScene;
            metadata.scene_count = model.scenes.size();
            metadata.node_count = model.nodes.size();
            metadata.mesh_count = model.meshes.size();
            metadata.material_count = model.materials.size();
            metadata.texture_count = model.textures.size();
            metadata.image_count = model.images.size();
            metadata.sampler_count = model.samplers.size();
            metadata.animation_count = model.animations.size();
            metadata.skin_count = model.skins.size();
            metadata.extensions_used = model.extensionsUsed;
            metadata.extensions_required = model.extensionsRequired;

            size_t primitive_count = 0;
            for (const tinygltf::Mesh& mesh : model.meshes) {
                primitive_count += mesh.primitives.size();
            }
            metadata.primitive_count = primitive_count;

            return metadata;
        }
    } // namespace

    bool TinyGltfImporter::canImport(const std::filesystem::path& path) const {
        const std::string extension = toLower(path.extension().string());
        return extension == ".gltf" || extension == ".glb";
    }

    ModelImportResult TinyGltfImporter::importModel(const ModelImportRequest& request) const {
        ModelImportResult result;

        if (!canImport(request.path)) {
            result.errors.push_back("TinyGltfImporter only supports .gltf and .glb files: " + request.path.string());
            return result;
        }

        if (request.path.empty()) {
            result.errors.push_back("TinyGltfImporter received an empty model path.");
            return result;
        }

        if (!std::filesystem::exists(request.path)) {
            result.errors.push_back("glTF source file does not exist: " + request.path.string());
            return result;
        }

        tinygltf::Model model;
        tinygltf::TinyGLTF loader;
        loader.SetImageLoader(loadMetadataImage, nullptr);

        std::string warning;
        std::string error;
        const std::string extension = toLower(request.path.extension().string());
        const bool loaded = extension == ".glb"
            ? loader.LoadBinaryFromFile(&model, &error, &warning, request.path.string())
            : loader.LoadASCIIFromFile(&model, &error, &warning, request.path.string());

        appendMessage(result.warnings, warning);
        appendMessage(result.errors, error);

        if (!loaded) {
            result.success = false;
            return result;
        }

        if (request.mode == ModelImportMode::FullModel) {
            result.warnings.push_back("TinyGltfImporter PR-L1 supports metadata parsing only; geometry/material conversion is deferred.");
        }

        result.metadata = buildMetadata(request.path, model);
        result.success = true;
        return result;
    }
} // namespace NexAur
