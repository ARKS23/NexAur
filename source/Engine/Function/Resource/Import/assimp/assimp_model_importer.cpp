#include "pch.h"
#include "Function/Resource/Import/assimp/assimp_model_importer.h"

#include "Function/Resource/model.h"

#include <assimp/GltfMaterial.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <utility>

#include <glm/gtc/matrix_inverse.hpp>

namespace NexAur {
    namespace {
        std::string toLower(std::string value) {
            for (char& character : value) {
                character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
            }
            return value;
        }

        glm::mat4 toGlmMatrix(const aiMatrix4x4& matrix) {
            return glm::mat4{
                matrix.a1, matrix.b1, matrix.c1, matrix.d1,
                matrix.a2, matrix.b2, matrix.c2, matrix.d2,
                matrix.a3, matrix.b3, matrix.c3, matrix.d3,
                matrix.a4, matrix.b4, matrix.c4, matrix.d4
            };
        }

        glm::vec3 safeNormalize(const glm::vec3& value, const glm::vec3& fallback = glm::vec3{ 0.0f }) {
            const float length_squared = glm::dot(value, value);
            if (length_squared <= 0.000001f) {
                return fallback;
            }

            return value / std::sqrt(length_squared);
        }

        glm::mat3 buildNormalMatrix(const glm::mat4& transform) {
            const glm::mat3 basis(transform);
            if (std::abs(glm::determinant(basis)) <= 0.000001f) {
                return glm::mat3{ 1.0f };
            }

            return glm::inverseTranspose(basis);
        }

        std::string normalizePathString(const std::filesystem::path& path) {
            return path.lexically_normal().string();
        }

        MaterialAlphaMode parseGltfAlphaMode(const aiString& value) {
            const std::string alpha_mode = value.C_Str();
            if (alpha_mode == "MASK") {
                return MaterialAlphaMode::Mask;
            }
            if (alpha_mode == "BLEND") {
                return MaterialAlphaMode::Blend;
            }

            return MaterialAlphaMode::Opaque;
        }

        bool sameTexturePath(const std::string& lhs, const std::string& rhs) {
            return !lhs.empty() &&
                   !rhs.empty() &&
                   normalizePathString(lhs) == normalizePathString(rhs);
        }

        bool isFinite(const glm::vec3& value) {
            return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
        }

        bool isValidDirection(const glm::vec3& value) {
            return isFinite(value) && glm::dot(value, value) > 0.000001f;
        }

        glm::vec3 fallbackTangent(const glm::vec3& normal) {
            const glm::vec3 axis = std::abs(normal.x) < 0.9f
                ? glm::vec3{ 1.0f, 0.0f, 0.0f }
                : glm::vec3{ 0.0f, 1.0f, 0.0f };
            return safeNormalize(axis - normal * glm::dot(normal, axis), glm::vec3{ 1.0f, 0.0f, 0.0f });
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
                const glm::vec2 duv1 = v1.texCoords - v0.texCoords;
                const glm::vec2 duv2 = v2.texCoords - v0.texCoords;
                const float denominator = duv1.x * duv2.y - duv2.x * duv1.y;
                if (std::abs(denominator) <= 0.000001f) {
                    continue;
                }

                const float inv_denominator = 1.0f / denominator;
                const glm::vec3 tangent = (edge1 * duv2.y - edge2 * duv1.y) * inv_denominator;
                const glm::vec3 bitangent = (edge2 * duv1.x - edge1 * duv2.x) * inv_denominator;
                if (!isValidDirection(tangent) || !isValidDirection(bitangent)) {
                    continue;
                }

                tangent_sums[i0] += tangent;
                tangent_sums[i1] += tangent;
                tangent_sums[i2] += tangent;
                bitangent_sums[i0] += bitangent;
                bitangent_sums[i1] += bitangent;
                bitangent_sums[i2] += bitangent;
            }

            for (size_t vertex_index = 0; vertex_index < vertices.size(); ++vertex_index) {
                Vertex& vertex = vertices[vertex_index];
                const glm::vec3 normal = safeNormalize(vertex.normal, glm::vec3{ 0.0f, 0.0f, 1.0f });

                glm::vec3 tangent = tangent_sums[vertex_index] -
                    normal * glm::dot(normal, tangent_sums[vertex_index]);
                tangent = safeNormalize(tangent, fallbackTangent(normal));

                const glm::vec3 bitangent_sum = bitangent_sums[vertex_index];
                const float handedness =
                    isValidDirection(bitangent_sum) &&
                    glm::dot(glm::cross(normal, tangent), bitangent_sum) < 0.0f
                        ? -1.0f
                        : 1.0f;

                vertex.tangent = tangent;
                vertex.bitangent = safeNormalize(glm::cross(normal, tangent) * handedness);
            }
        }

        struct AssimpImportContext {
            std::string directory;

            std::string loadMaterialTexturePath(aiMaterial* material, int type) const {
                const aiTextureType ai_type = static_cast<aiTextureType>(type);
                if (!material || material->GetTextureCount(ai_type) == 0) {
                    return "";
                }

                aiString value;
                material->GetTexture(ai_type, 0, &value);
                const std::filesystem::path texture_path(value.C_Str());
                if (texture_path.empty() || texture_path.string().starts_with("*")) {
                    return "";
                }
                if (texture_path.is_absolute() || directory.empty()) {
                    return normalizePathString(texture_path);
                }

                return normalizePathString(std::filesystem::path(directory) / texture_path);
            }

            Mesh processMesh(aiMesh* mesh, const aiScene* scene, const glm::mat4& node_transform) const {
                std::vector<Vertex> vertices;
                std::vector<unsigned int> indices;
                MaterialImportData material;
                const glm::mat3 normal_matrix = buildNormalMatrix(node_transform);
                const glm::mat3 direction_matrix(node_transform);
                const bool has_texcoords = mesh->mTextureCoords[0] != nullptr;

                vertices.reserve(mesh->mNumVertices);
                for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
                    Vertex vertex;
                    const glm::vec4 local_position{
                        mesh->mVertices[i].x,
                        mesh->mVertices[i].y,
                        mesh->mVertices[i].z,
                        1.0f
                    };
                    vertex.position = glm::vec3(node_transform * local_position);

                    if (mesh->HasNormals()) {
                        const glm::vec3 local_normal{
                            mesh->mNormals[i].x,
                            mesh->mNormals[i].y,
                            mesh->mNormals[i].z
                        };
                        vertex.normal = safeNormalize(normal_matrix * local_normal);
                    } else {
                        vertex.normal = { 0.0f, 0.0f, 0.0f };
                    }

                    if (has_texcoords) {
                        vertex.texCoords = { mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y };
                        if (mesh->HasTangentsAndBitangents()) {
                            const glm::vec3 local_tangent{
                                mesh->mTangents[i].x,
                                mesh->mTangents[i].y,
                                mesh->mTangents[i].z
                            };
                            const glm::vec3 local_bitangent{
                                mesh->mBitangents[i].x,
                                mesh->mBitangents[i].y,
                                mesh->mBitangents[i].z
                            };
                            vertex.tangent = safeNormalize(direction_matrix * local_tangent);
                            vertex.bitangent = safeNormalize(direction_matrix * local_bitangent);
                        } else {
                            vertex.tangent = { 0.0f, 0.0f, 0.0f };
                            vertex.bitangent = { 0.0f, 0.0f, 0.0f };
                        }
                    } else {
                        vertex.texCoords = { 0.0f, 0.0f };
                        vertex.tangent = { 0.0f, 0.0f, 0.0f };
                        vertex.bitangent = { 0.0f, 0.0f, 0.0f };
                    }

                    vertices.push_back(vertex);
                }

                for (unsigned int i = 0; i < mesh->mNumFaces; ++i) {
                    const aiFace& face = mesh->mFaces[i];
                    for (unsigned int j = 0; j < face.mNumIndices; ++j) {
                        indices.push_back(face.mIndices[j]);
                    }
                }

                if (has_texcoords) {
                    generateTangents(vertices, indices);
                }

                if (mesh->mMaterialIndex < scene->mNumMaterials) {
                    aiMaterial* mat = scene->mMaterials[mesh->mMaterialIndex];
                    material.name = mat->GetName().C_Str();

                    aiColor4D base_color_factor;
                    if (AI_SUCCESS == aiGetMaterialColor(mat, AI_MATKEY_BASE_COLOR, &base_color_factor) ||
                        AI_SUCCESS == aiGetMaterialColor(mat, AI_MATKEY_COLOR_DIFFUSE, &base_color_factor)) {
                        material.base_color_factor = {
                            base_color_factor.r,
                            base_color_factor.g,
                            base_color_factor.b,
                            base_color_factor.a
                        };
                    }

                    float metallic_factor = material.metallic_factor;
                    bool has_metallic_factor = false;
                    if (AI_SUCCESS == mat->Get(AI_MATKEY_METALLIC_FACTOR, metallic_factor)) {
                        material.metallic_factor = metallic_factor;
                        has_metallic_factor = true;
                    }

                    float roughness_factor = material.roughness_factor;
                    if (AI_SUCCESS == mat->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness_factor)) {
                        material.roughness_factor = roughness_factor;
                    }

                    aiColor3D emissive_factor;
                    if (AI_SUCCESS == mat->Get(AI_MATKEY_COLOR_EMISSIVE, emissive_factor)) {
                        material.emissive_factor = {
                            emissive_factor.r,
                            emissive_factor.g,
                            emissive_factor.b
                        };
                    }

                    material.base_color_texture_path = loadMaterialTexturePath(mat, aiTextureType_BASE_COLOR);
                    material.normal_texture_path = loadMaterialTexturePath(mat, aiTextureType_NORMALS);
                    material.metallic_texture_path = loadMaterialTexturePath(mat, aiTextureType_METALNESS);
                    material.roughness_texture_path = loadMaterialTexturePath(mat, aiTextureType_DIFFUSE_ROUGHNESS);
                    material.metallic_roughness_texture_path = loadMaterialTexturePath(mat, aiTextureType_GLTF_METALLIC_ROUGHNESS);
                    material.ao_texture_path = loadMaterialTexturePath(mat, aiTextureType_AMBIENT_OCCLUSION);
                    if (material.ao_texture_path.empty()) {
                        material.ao_texture_path = loadMaterialTexturePath(mat, aiTextureType_LIGHTMAP);
                    }
                    material.emissive_texture_path = loadMaterialTexturePath(mat, aiTextureType_EMISSIVE);

                    if (!material.metallic_roughness_texture_path.empty()) {
                        if (!has_metallic_factor) {
                            material.metallic_factor = 1.0f;
                        }
                        material.metallic_texture_path.clear();
                        material.roughness_texture_path.clear();
                        material.metallic_roughness_mode = MaterialMetallicRoughnessTextureMode::PackedGltf;
                    } else if (sameTexturePath(material.metallic_texture_path, material.roughness_texture_path)) {
                        if (!has_metallic_factor) {
                            material.metallic_factor = 1.0f;
                        }
                        material.metallic_roughness_texture_path = material.metallic_texture_path;
                        material.metallic_texture_path.clear();
                        material.roughness_texture_path.clear();
                        material.metallic_roughness_mode = MaterialMetallicRoughnessTextureMode::PackedGltf;
                    }

                    float normal_scale = material.normal_scale;
                    if (AI_SUCCESS == mat->Get(AI_MATKEY_GLTF_TEXTURE_SCALE(aiTextureType_NORMALS, 0), normal_scale)) {
                        material.normal_scale = normal_scale;
                    }

                    float occlusion_strength = material.occlusion_strength;
                    if (AI_SUCCESS == mat->Get(AI_MATKEY_GLTF_TEXTURE_STRENGTH(aiTextureType_AMBIENT_OCCLUSION, 0), occlusion_strength) ||
                        AI_SUCCESS == mat->Get(AI_MATKEY_GLTF_TEXTURE_STRENGTH(aiTextureType_LIGHTMAP, 0), occlusion_strength)) {
                        material.occlusion_strength = occlusion_strength;
                    }

                    aiString alpha_mode;
                    if (AI_SUCCESS == mat->Get(AI_MATKEY_GLTF_ALPHAMODE, alpha_mode)) {
                        material.alpha_mode = parseGltfAlphaMode(alpha_mode);
                    }

                    float alpha_cutoff = material.alpha_cutoff;
                    if (AI_SUCCESS == mat->Get(AI_MATKEY_GLTF_ALPHACUTOFF, alpha_cutoff)) {
                        material.alpha_cutoff = alpha_cutoff;
                    }
                }

                return Mesh(vertices, indices, material);
            }

            void processNode(
                aiNode* node,
                const aiScene* scene,
                const glm::mat4& parent_transform,
                std::vector<Mesh>& meshes) const {
                const glm::mat4 node_transform = parent_transform * toGlmMatrix(node->mTransformation);

                for (unsigned int i = 0; i < node->mNumMeshes; ++i) {
                    aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
                    meshes.push_back(processMesh(mesh, scene, node_transform));
                }

                for (unsigned int i = 0; i < node->mNumChildren; ++i) {
                    processNode(node->mChildren[i], scene, node_transform, meshes);
                }
            }
        };

        ModelImportMetadata buildMetadata(const std::filesystem::path& path, const aiScene* scene) {
            ModelImportMetadata metadata;
            metadata.source_path = path.string();
            metadata.generator = "Assimp";
            metadata.mesh_count = scene ? scene->mNumMeshes : 0;
            metadata.material_count = scene ? scene->mNumMaterials : 0;
            metadata.node_count = scene && scene->mRootNode ? 1 : 0;
            metadata.scene_count = scene ? 1 : 0;
            metadata.primitive_count = metadata.mesh_count;

            if (scene) {
                for (unsigned int mesh_index = 0; mesh_index < scene->mNumMeshes; ++mesh_index) {
                    const aiMesh* mesh = scene->mMeshes[mesh_index];
                    if (!mesh) {
                        continue;
                    }
                    metadata.vertex_count += mesh->mNumVertices;
                    for (unsigned int face_index = 0; face_index < mesh->mNumFaces; ++face_index) {
                        metadata.index_count += mesh->mFaces[face_index].mNumIndices;
                    }
                }
            }

            return metadata;
        }

        constexpr unsigned int kAssimpImportFlags =
            aiProcess_Triangulate |
            aiProcess_GenSmoothNormals |
            aiProcess_CalcTangentSpace;
    } // namespace

    bool AssimpModelImporter::canImport(const std::filesystem::path& path) const {
        const std::string extension = toLower(path.extension().string());
        return extension == ".obj" ||
               extension == ".fbx" ||
               extension == ".dae" ||
               extension == ".3ds" ||
               extension == ".ply" ||
               extension == ".stl";
    }

    ModelImportResult AssimpModelImporter::importModel(const ModelImportRequest& request) const {
        ModelImportResult result;

        if (!canImport(request.path)) {
            result.errors.push_back("AssimpModelImporter is registered only for non-glTF fallback formats: " + request.path.string());
            return result;
        }
        if (request.path.empty()) {
            result.errors.push_back("AssimpModelImporter received an empty model path.");
            return result;
        }
        if (!std::filesystem::exists(request.path)) {
            result.errors.push_back("Assimp source file does not exist: " + request.path.string());
            return result;
        }

        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(request.path.string(), kAssimpImportFlags);
        if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode) {
            result.errors.push_back("Assimp error: " + std::string(importer.GetErrorString()));
            return result;
        }

        result.metadata = buildMetadata(request.path, scene);
        if (request.mode == ModelImportMode::FullModel) {
            std::vector<Mesh> meshes;
            AssimpImportContext context;
            const std::filesystem::path model_path(request.path);
            context.directory = model_path.has_parent_path() ? model_path.parent_path().string() : "";
            context.processNode(scene->mRootNode, scene, glm::mat4{ 1.0f }, meshes);

            if (meshes.empty()) {
                result.errors.push_back("AssimpModelImporter did not produce any CPU meshes.");
                return result;
            }

            const std::string debug_name = request.path.filename().string().empty()
                ? "AssimpModel"
                : request.path.filename().string();
            result.model = std::make_shared<Model>(std::move(meshes), debug_name);
            if (!result.model || !result.model->isLoaded()) {
                result.errors.push_back("AssimpModelImporter produced an invalid CPU model.");
                return result;
            }
        }

        result.success = true;
        return result;
    }
} // namespace NexAur
