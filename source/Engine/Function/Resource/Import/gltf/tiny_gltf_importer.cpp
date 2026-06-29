#include "pch.h"
#include "Function/Resource/Import/gltf/tiny_gltf_importer.h"

#include "Function/Resource/model.h"

#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include <tiny_gltf.h>

#include <cctype>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <limits>

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

        template <typename T>
        T readPod(const unsigned char* data) {
            T value{};
            std::memcpy(&value, data, sizeof(T));
            return value;
        }

        glm::vec3 safeNormalize(const glm::vec3& value, const glm::vec3& fallback = glm::vec3{ 0.0f }) {
            const float length_squared = glm::dot(value, value);
            if (length_squared <= 0.000001f) {
                return fallback;
            }

            return value / std::sqrt(length_squared);
        }

        glm::vec3 fallbackTangent(const glm::vec3& normal) {
            const glm::vec3 axis = std::abs(normal.x) < 0.9f
                ? glm::vec3{ 1.0f, 0.0f, 0.0f }
                : glm::vec3{ 0.0f, 1.0f, 0.0f };
            return safeNormalize(axis - normal * glm::dot(normal, axis), glm::vec3{ 1.0f, 0.0f, 0.0f });
        }

        struct AccessorSpan {
            const tinygltf::Accessor* accessor = nullptr;
            const unsigned char* data = nullptr;
            size_t stride = 0;
            size_t component_count = 0;
        };

        bool prepareAccessorSpan(
            const tinygltf::Model& model,
            int accessor_index,
            int expected_type,
            AccessorSpan& span,
            std::vector<std::string>& errors,
            const std::string& label) {
            if (accessor_index < 0 || accessor_index >= static_cast<int>(model.accessors.size())) {
                errors.push_back(label + " accessor index is out of range.");
                return false;
            }

            const tinygltf::Accessor& accessor = model.accessors[accessor_index];
            if (accessor.sparse.isSparse) {
                errors.push_back(label + " uses sparse accessor, which is not supported by PR-L2.");
                return false;
            }
            if (accessor.type != expected_type) {
                errors.push_back(label + " accessor type does not match expected glTF type.");
                return false;
            }
            if (accessor.bufferView < 0 || accessor.bufferView >= static_cast<int>(model.bufferViews.size())) {
                errors.push_back(label + " accessor has an invalid bufferView.");
                return false;
            }

            const tinygltf::BufferView& buffer_view = model.bufferViews[accessor.bufferView];
            if (buffer_view.buffer < 0 || buffer_view.buffer >= static_cast<int>(model.buffers.size())) {
                errors.push_back(label + " bufferView references an invalid buffer.");
                return false;
            }

            const tinygltf::Buffer& buffer = model.buffers[buffer_view.buffer];
            const int component_size = tinygltf::GetComponentSizeInBytes(static_cast<uint32_t>(accessor.componentType));
            const int component_count = tinygltf::GetNumComponentsInType(static_cast<uint32_t>(accessor.type));
            if (component_size <= 0 || component_count <= 0) {
                errors.push_back(label + " accessor uses an unsupported component layout.");
                return false;
            }

            const int byte_stride = accessor.ByteStride(buffer_view);
            const size_t element_size = static_cast<size_t>(component_size) * static_cast<size_t>(component_count);
            if (byte_stride <= 0 || static_cast<size_t>(byte_stride) < element_size) {
                errors.push_back(label + " accessor has an invalid byte stride.");
                return false;
            }

            const size_t base_offset = buffer_view.byteOffset + accessor.byteOffset;
            if (base_offset > buffer.data.size()) {
                errors.push_back(label + " accessor byte offset is outside its buffer.");
                return false;
            }

            if (accessor.count > 0) {
                const size_t last_element_begin =
                    accessor.byteOffset + static_cast<size_t>(byte_stride) * (accessor.count - 1);
                const size_t view_required_size = last_element_begin + element_size;
                const size_t buffer_required_size = buffer_view.byteOffset + view_required_size;
                if (view_required_size > buffer_view.byteLength || buffer_required_size > buffer.data.size()) {
                    errors.push_back(label + " accessor range exceeds its bufferView or buffer.");
                    return false;
                }
            }

            span.accessor = &accessor;
            span.data = buffer.data.data() + base_offset;
            span.stride = static_cast<size_t>(byte_stride);
            span.component_count = static_cast<size_t>(component_count);
            return true;
        }

        float readFloatComponent(const unsigned char* data, int component_type, bool normalized) {
            switch (component_type) {
                case TINYGLTF_COMPONENT_TYPE_BYTE: {
                    const int8_t value = readPod<int8_t>(data);
                    return normalized ? std::max(static_cast<float>(value) / 127.0f, -1.0f) : static_cast<float>(value);
                }
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
                    const uint8_t value = readPod<uint8_t>(data);
                    return normalized ? static_cast<float>(value) / 255.0f : static_cast<float>(value);
                }
                case TINYGLTF_COMPONENT_TYPE_SHORT: {
                    const int16_t value = readPod<int16_t>(data);
                    return normalized ? std::max(static_cast<float>(value) / 32767.0f, -1.0f) : static_cast<float>(value);
                }
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
                    const uint16_t value = readPod<uint16_t>(data);
                    return normalized ? static_cast<float>(value) / 65535.0f : static_cast<float>(value);
                }
                case TINYGLTF_COMPONENT_TYPE_FLOAT:
                    return readPod<float>(data);
                default:
                    return 0.0f;
            }
        }

        bool canReadFloatComponents(int component_type) {
            switch (component_type) {
                case TINYGLTF_COMPONENT_TYPE_BYTE:
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                case TINYGLTF_COMPONENT_TYPE_SHORT:
                case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                case TINYGLTF_COMPONENT_TYPE_FLOAT:
                    return true;
                default:
                    return false;
            }
        }

        glm::vec4 readFloatVector(const AccessorSpan& span, size_t element_index) {
            glm::vec4 value{ 0.0f };
            const tinygltf::Accessor& accessor = *span.accessor;
            const int component_size = tinygltf::GetComponentSizeInBytes(static_cast<uint32_t>(accessor.componentType));
            const unsigned char* element = span.data + span.stride * element_index;
            for (size_t component = 0; component < span.component_count && component < 4; ++component) {
                value[static_cast<int>(component)] = readFloatComponent(
                    element + static_cast<size_t>(component_size) * component,
                    accessor.componentType,
                    accessor.normalized);
            }
            return value;
        }

        bool readVec3Accessor(
            const tinygltf::Model& model,
            int accessor_index,
            std::vector<glm::vec3>& values,
            std::vector<std::string>& errors,
            const std::string& label) {
            AccessorSpan span;
            if (!prepareAccessorSpan(model, accessor_index, TINYGLTF_TYPE_VEC3, span, errors, label)) {
                return false;
            }
            if (!canReadFloatComponents(span.accessor->componentType)) {
                errors.push_back(label + " accessor has an unsupported component type.");
                return false;
            }

            values.resize(span.accessor->count);
            for (size_t index = 0; index < span.accessor->count; ++index) {
                const glm::vec4 value = readFloatVector(span, index);
                values[index] = glm::vec3{ value.x, value.y, value.z };
            }
            return true;
        }

        bool readVec2Accessor(
            const tinygltf::Model& model,
            int accessor_index,
            std::vector<glm::vec2>& values,
            std::vector<std::string>& errors,
            const std::string& label) {
            AccessorSpan span;
            if (!prepareAccessorSpan(model, accessor_index, TINYGLTF_TYPE_VEC2, span, errors, label)) {
                return false;
            }
            if (!canReadFloatComponents(span.accessor->componentType)) {
                errors.push_back(label + " accessor has an unsupported component type.");
                return false;
            }

            values.resize(span.accessor->count);
            for (size_t index = 0; index < span.accessor->count; ++index) {
                const glm::vec4 value = readFloatVector(span, index);
                values[index] = glm::vec2{ value.x, value.y };
            }
            return true;
        }

        bool readVec4Accessor(
            const tinygltf::Model& model,
            int accessor_index,
            std::vector<glm::vec4>& values,
            std::vector<std::string>& errors,
            const std::string& label) {
            AccessorSpan span;
            if (!prepareAccessorSpan(model, accessor_index, TINYGLTF_TYPE_VEC4, span, errors, label)) {
                return false;
            }
            if (!canReadFloatComponents(span.accessor->componentType)) {
                errors.push_back(label + " accessor has an unsupported component type.");
                return false;
            }

            values.resize(span.accessor->count);
            for (size_t index = 0; index < span.accessor->count; ++index) {
                values[index] = readFloatVector(span, index);
            }
            return true;
        }

        bool readIndices(
            const tinygltf::Model& model,
            int accessor_index,
            size_t vertex_count,
            std::vector<unsigned int>& indices,
            std::vector<std::string>& errors,
            const std::string& label) {
            if (accessor_index < 0) {
                if (vertex_count > static_cast<size_t>((std::numeric_limits<unsigned int>::max)())) {
                    errors.push_back(label + " has too many vertices for generated uint32 indices.");
                    return false;
                }
                indices.resize(vertex_count);
                for (size_t index = 0; index < vertex_count; ++index) {
                    indices[index] = static_cast<unsigned int>(index);
                }
                return true;
            }

            AccessorSpan span;
            if (!prepareAccessorSpan(model, accessor_index, TINYGLTF_TYPE_SCALAR, span, errors, label)) {
                return false;
            }

            const tinygltf::Accessor& accessor = *span.accessor;
            indices.resize(accessor.count);
            for (size_t index = 0; index < accessor.count; ++index) {
                const unsigned char* element = span.data + span.stride * index;
                uint32_t value = 0;
                switch (accessor.componentType) {
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                        value = readPod<uint8_t>(element);
                        break;
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                        value = readPod<uint16_t>(element);
                        break;
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                        value = readPod<uint32_t>(element);
                        break;
                    default:
                        errors.push_back(label + " index accessor uses an unsupported component type.");
                        return false;
                }

                if (value >= vertex_count) {
                    errors.push_back(label + " index accessor references a vertex outside POSITION range.");
                    return false;
                }

                indices[index] = static_cast<unsigned int>(value);
            }
            return true;
        }

        void generateNormals(std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices) {
            if (vertices.empty() || indices.size() < 3) {
                return;
            }

            for (Vertex& vertex : vertices) {
                vertex.normal = glm::vec3{ 0.0f };
            }

            for (size_t index = 0; index + 2 < indices.size(); index += 3) {
                const unsigned int i0 = indices[index + 0];
                const unsigned int i1 = indices[index + 1];
                const unsigned int i2 = indices[index + 2];
                if (i0 >= vertices.size() || i1 >= vertices.size() || i2 >= vertices.size()) {
                    continue;
                }

                const glm::vec3 edge1 = vertices[i1].position - vertices[i0].position;
                const glm::vec3 edge2 = vertices[i2].position - vertices[i0].position;
                const glm::vec3 face_normal = glm::cross(edge1, edge2);
                if (glm::dot(face_normal, face_normal) <= 0.000001f) {
                    continue;
                }

                vertices[i0].normal += face_normal;
                vertices[i1].normal += face_normal;
                vertices[i2].normal += face_normal;
            }

            for (Vertex& vertex : vertices) {
                vertex.normal = safeNormalize(vertex.normal, glm::vec3{ 0.0f, 1.0f, 0.0f });
            }
        }

        void generateTangents(std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices) {
            if (vertices.empty() || indices.size() < 3 || indices.size() % 3 != 0) {
                return;
            }

            std::vector<glm::vec3> tangent_sums(vertices.size(), glm::vec3{ 0.0f });
            std::vector<glm::vec3> bitangent_sums(vertices.size(), glm::vec3{ 0.0f });

            for (size_t index = 0; index + 2 < indices.size(); index += 3) {
                const unsigned int i0 = indices[index + 0];
                const unsigned int i1 = indices[index + 1];
                const unsigned int i2 = indices[index + 2];
                if (i0 >= vertices.size() || i1 >= vertices.size() || i2 >= vertices.size()) {
                    continue;
                }

                const Vertex& v0 = vertices[i0];
                const Vertex& v1 = vertices[i1];
                const Vertex& v2 = vertices[i2];
                const glm::vec3 edge1 = v1.position - v0.position;
                const glm::vec3 edge2 = v2.position - v0.position;
                const glm::vec2 delta_uv1 = v1.texCoords - v0.texCoords;
                const glm::vec2 delta_uv2 = v2.texCoords - v0.texCoords;
                const float denominator = delta_uv1.x * delta_uv2.y - delta_uv2.x * delta_uv1.y;
                if (std::abs(denominator) <= 0.000001f) {
                    continue;
                }

                const float inverse_denominator = 1.0f / denominator;
                tangent_sums[i0] += (edge1 * delta_uv2.y - edge2 * delta_uv1.y) * inverse_denominator;
                tangent_sums[i1] += (edge1 * delta_uv2.y - edge2 * delta_uv1.y) * inverse_denominator;
                tangent_sums[i2] += (edge1 * delta_uv2.y - edge2 * delta_uv1.y) * inverse_denominator;
                bitangent_sums[i0] += (edge2 * delta_uv1.x - edge1 * delta_uv2.x) * inverse_denominator;
                bitangent_sums[i1] += (edge2 * delta_uv1.x - edge1 * delta_uv2.x) * inverse_denominator;
                bitangent_sums[i2] += (edge2 * delta_uv1.x - edge1 * delta_uv2.x) * inverse_denominator;
            }

            for (size_t vertex_index = 0; vertex_index < vertices.size(); ++vertex_index) {
                Vertex& vertex = vertices[vertex_index];
                const glm::vec3 normal = safeNormalize(vertex.normal, glm::vec3{ 0.0f, 0.0f, 1.0f });
                glm::vec3 tangent = tangent_sums[vertex_index] -
                    normal * glm::dot(normal, tangent_sums[vertex_index]);
                tangent = safeNormalize(tangent, fallbackTangent(normal));

                const glm::vec3 bitangent_sum = bitangent_sums[vertex_index];
                const float handedness =
                    glm::dot(bitangent_sum, bitangent_sum) > 0.000001f &&
                    glm::dot(glm::cross(normal, tangent), bitangent_sum) < 0.0f
                        ? -1.0f
                        : 1.0f;

                vertex.tangent = tangent;
                vertex.bitangent = safeNormalize(glm::cross(normal, tangent) * handedness);
            }
        }

        MaterialAlphaMode parseAlphaMode(const std::string& alpha_mode) {
            if (alpha_mode == "MASK") {
                return MaterialAlphaMode::Mask;
            }
            if (alpha_mode == "BLEND") {
                return MaterialAlphaMode::Blend;
            }
            return MaterialAlphaMode::Opaque;
        }

        MaterialImportData buildMaterial(const tinygltf::Model& model, int material_index) {
            MaterialImportData material;
            if (material_index < 0 || material_index >= static_cast<int>(model.materials.size())) {
                material.name = "TinyGltfDefaultMaterial";
                return material;
            }

            const tinygltf::Material& gltf_material = model.materials[material_index];
            material.name = gltf_material.name.empty() ? "TinyGltfMaterial" : gltf_material.name;

            const std::vector<double>& base_color = gltf_material.pbrMetallicRoughness.baseColorFactor;
            if (base_color.size() >= 4) {
                material.base_color_factor = {
                    static_cast<float>(base_color[0]),
                    static_cast<float>(base_color[1]),
                    static_cast<float>(base_color[2]),
                    static_cast<float>(base_color[3])
                };
            }
            material.metallic_factor = static_cast<float>(gltf_material.pbrMetallicRoughness.metallicFactor);
            material.roughness_factor = static_cast<float>(gltf_material.pbrMetallicRoughness.roughnessFactor);
            material.alpha_mode = parseAlphaMode(gltf_material.alphaMode);
            material.alpha_cutoff = static_cast<float>(gltf_material.alphaCutoff);
            return material;
        }

        const int* findAttribute(const tinygltf::Primitive& primitive, const std::string& name) {
            const auto it = primitive.attributes.find(name);
            return it == primitive.attributes.end() ? nullptr : &it->second;
        }

        bool appendPrimitiveMesh(
            const tinygltf::Model& model,
            const tinygltf::Mesh& gltf_mesh,
            const tinygltf::Primitive& primitive,
            const ModelImportRequest& request,
            ModelImportResult& result,
            std::vector<Mesh>& meshes) {
            if (primitive.mode != -1 && primitive.mode != TINYGLTF_MODE_TRIANGLES) {
                result.errors.push_back("TinyGltfImporter PR-L2 only supports triangle primitives.");
                return false;
            }

            const int* position_accessor = findAttribute(primitive, "POSITION");
            if (!position_accessor) {
                result.errors.push_back("glTF primitive is missing POSITION attribute.");
                return false;
            }

            std::vector<glm::vec3> positions;
            if (!readVec3Accessor(model, *position_accessor, positions, result.errors, "POSITION")) {
                return false;
            }
            if (positions.empty()) {
                result.errors.push_back("glTF primitive POSITION attribute is empty.");
                return false;
            }

            std::vector<unsigned int> indices;
            if (!readIndices(model, primitive.indices, positions.size(), indices, result.errors, "INDICES")) {
                return false;
            }

            std::vector<glm::vec3> normals(positions.size(), glm::vec3{ 0.0f });
            const int* normal_accessor = findAttribute(primitive, "NORMAL");
            if (normal_accessor) {
                if (!readVec3Accessor(model, *normal_accessor, normals, result.errors, "NORMAL")) {
                    return false;
                }
                if (normals.size() != positions.size()) {
                    result.errors.push_back("NORMAL accessor count does not match POSITION count.");
                    return false;
                }
            }

            std::vector<glm::vec2> texcoords(positions.size(), glm::vec2{ 0.0f });
            const int* texcoord_accessor = findAttribute(primitive, "TEXCOORD_0");
            if (texcoord_accessor) {
                if (!readVec2Accessor(model, *texcoord_accessor, texcoords, result.errors, "TEXCOORD_0")) {
                    return false;
                }
                if (texcoords.size() != positions.size()) {
                    result.errors.push_back("TEXCOORD_0 accessor count does not match POSITION count.");
                    return false;
                }
            }

            std::vector<glm::vec4> tangents;
            const int* tangent_accessor = findAttribute(primitive, "TANGENT");
            if (tangent_accessor) {
                if (!readVec4Accessor(model, *tangent_accessor, tangents, result.errors, "TANGENT")) {
                    return false;
                }
                if (tangents.size() != positions.size()) {
                    result.errors.push_back("TANGENT accessor count does not match POSITION count.");
                    return false;
                }
            }

            std::vector<Vertex> vertices(positions.size());
            for (size_t vertex_index = 0; vertex_index < positions.size(); ++vertex_index) {
                Vertex& vertex = vertices[vertex_index];
                vertex.position = positions[vertex_index];
                vertex.normal = normal_accessor
                    ? safeNormalize(normals[vertex_index], glm::vec3{ 0.0f, 1.0f, 0.0f })
                    : glm::vec3{ 0.0f };
                vertex.texCoords = texcoords[vertex_index];
                vertex.tangent = glm::vec3{ 0.0f };
                vertex.bitangent = glm::vec3{ 0.0f };

                if (tangent_accessor) {
                    const glm::vec4 tangent = tangents[vertex_index];
                    vertex.tangent = safeNormalize(glm::vec3{ tangent.x, tangent.y, tangent.z }, fallbackTangent(vertex.normal));
                    vertex.bitangent = safeNormalize(glm::cross(vertex.normal, vertex.tangent) * tangent.w);
                }
            }

            if (!normal_accessor) {
                generateNormals(vertices, indices);
            }
            if (!tangent_accessor && request.tangent_policy == TangentGenerationPolicy::GenerateIfMissing) {
                generateTangents(vertices, indices);
            }

            MaterialImportData material = buildMaterial(model, primitive.material);
            if (!gltf_mesh.name.empty() && material.name == "TinyGltfDefaultMaterial") {
                material.name = gltf_mesh.name + ".Material";
            }

            result.metadata.vertex_count += vertices.size();
            result.metadata.index_count += indices.size();
            meshes.emplace_back(vertices, indices, material);
            return true;
        }

        bool buildCpuModel(
            const tinygltf::Model& gltf_model,
            const std::filesystem::path& path,
            const ModelImportRequest& request,
            ModelImportResult& result) {
            std::vector<Mesh> meshes;
            for (const tinygltf::Mesh& gltf_mesh : gltf_model.meshes) {
                for (const tinygltf::Primitive& primitive : gltf_mesh.primitives) {
                    if (!appendPrimitiveMesh(gltf_model, gltf_mesh, primitive, request, result, meshes)) {
                        return false;
                    }
                }
            }

            if (meshes.empty()) {
                result.errors.push_back("TinyGltfImporter did not produce any CPU meshes.");
                return false;
            }

            const std::string debug_name = path.filename().string().empty()
                ? "TinyGltfModel"
                : path.filename().string();
            result.model = std::make_shared<Model>(std::move(meshes), debug_name);
            return result.model && result.model->isLoaded();
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

        result.metadata = buildMetadata(request.path, model);
        if (request.mode == ModelImportMode::FullModel) {
            if (!buildCpuModel(model, request.path, request, result)) {
                result.success = false;
                return result;
            }
        }

        result.success = true;
        return result;
    }
} // namespace NexAur
