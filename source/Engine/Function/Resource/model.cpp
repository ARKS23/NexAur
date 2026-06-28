#include "pch.h"

#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"

#include "Core/Log/log_system.h"
#include "model.h"

#include <filesystem>
#include <utility>

namespace NexAur {
    Model::Model(const std::string& path) {
        loadModel(path);
    }

    Model::Model(std::vector<Mesh> meshes, std::string debug_name)
        : m_meshes(std::move(meshes)), m_directory(std::move(debug_name)), m_is_loaded(!m_meshes.empty()) {}

    void Model::loadModel(const std::string& path) {
        Assimp::Importer importer;

        const aiScene* scene = importer.ReadFile(path,
            aiProcess_Triangulate|      // 保证所有面是三角形
            aiProcess_GenSmoothNormals| // 没法线就生成平滑法线
            //aiProcess_FlipUVs|          // 图片坐标常常和Y轴相反，进行反转
            aiProcess_CalcTangentSpace);// 生成切线和副切线

        // 检查加载错误
        if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
            NX_CORE_ERROR("Assimp Error: {}", importer.GetErrorString());
            m_is_loaded = false;
            return;
        }

        const std::filesystem::path model_path(path);
        m_directory = model_path.has_parent_path() ? model_path.parent_path().string() : "";

        // 递归处理根节点
        processNode(scene->mRootNode, scene);

        m_is_loaded = true;
        NX_CORE_INFO("Model loaded successfully: {}", path);
    }

    void Model::processNode(aiNode* node, const aiScene* scene) {
        // 处理当前节点的所有网格体
        for (unsigned int i = 0; i < node->mNumMeshes; ++i) {
            aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
            m_meshes.push_back(processMesh(mesh, scene));
        }

        // 递归处理所有子节点
        for (unsigned int i = 0; i < node->mNumChildren; ++i) {
            processNode(node->mChildren[i], scene);
        }
    }

    Mesh Model::processMesh(aiMesh* mesh, const aiScene* scene) {
        std::vector<Vertex> vertices;
        std::vector<unsigned int> indices;
        MaterialImportData material;

        // 处理顶点数据
        for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
            Vertex vertex;
            // 位置
            vertex.position = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };
            // 法线
            if (mesh->HasNormals()) {
                vertex.normal = { mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z };
            }
            else {
                vertex.normal = { 0.0f, 0.0f, 0.0f }; // 没有法线时设置为零向量
            }
            // 纹理坐标（目前只处理第一层纹理坐标）
            if (mesh->mTextureCoords[0]) {
                vertex.texCoords = { mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y };
                if (mesh->HasTangentsAndBitangents()) {
                    vertex.tangent = { mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z };
                    vertex.bitangent = { mesh->mBitangents[i].x, mesh->mBitangents[i].y, mesh->mBitangents[i].z };
                }
                else {
                    vertex.tangent = { 0.0f, 0.0f, 0.0f };
                    vertex.bitangent = { 0.0f, 0.0f, 0.0f };
                }
            }
            else {
                vertex.texCoords = { 0.0f, 0.0f }; // 没有纹理坐标时设置为零
                vertex.tangent = { 0.0f, 0.0f, 0.0f };
                vertex.bitangent = { 0.0f, 0.0f, 0.0f };
            }

            vertices.push_back(vertex);
        }

        // 处理索引数据
        for (unsigned int i = 0; i < mesh->mNumFaces; ++i) {
            aiFace face = mesh->mFaces[i];
            for (unsigned int j = 0; j < face.mNumIndices; ++j) {
                indices.push_back(face.mIndices[j]);
            }
        }

        // 材质数据
        if (mesh->mMaterialIndex >= 0) {
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
            if (AI_SUCCESS == mat->Get(AI_MATKEY_METALLIC_FACTOR, metallic_factor)) {
                material.metallic_factor = metallic_factor;
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
            material.ao_texture_path = loadMaterialTexturePath(mat, aiTextureType_AMBIENT_OCCLUSION);
            if (material.ao_texture_path.empty()) {
                material.ao_texture_path = loadMaterialTexturePath(mat, aiTextureType_LIGHTMAP);
            }
            material.emissive_texture_path = loadMaterialTexturePath(mat, aiTextureType_EMISSIVE);

            if (!material.metallic_texture_path.empty() &&
                material.metallic_texture_path == material.roughness_texture_path) {
                material.metallic_roughness_texture_path = material.metallic_texture_path;
                material.metallic_texture_path.clear();
                material.roughness_texture_path.clear();
                material.metallic_roughness_mode = MaterialMetallicRoughnessTextureMode::PackedGltf;
            }
        }

        return Mesh(vertices, indices, material);
    }

    std::string Model::loadMaterialTexturePath(aiMaterial* material, int type) {
        // 后续做枚举封装
        aiTextureType aiType = static_cast<aiTextureType>(type);
        if (material->GetTextureCount(aiType) > 0) {
            aiString str;
            material->GetTexture(aiType, 0, &str);
            const std::filesystem::path texture_path(str.C_Str());
            if (texture_path.empty() || texture_path.string().starts_with("*")) {
                return "";
            }
            if (texture_path.is_absolute() || m_directory.empty()) {
                return texture_path.string();
            }

            return (std::filesystem::path(m_directory) / texture_path).string();
        }

        return "";
    }
} // namespace NexAur
