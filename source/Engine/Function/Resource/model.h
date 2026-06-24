#pragma once
#include <string>
#include <vector>

#include "Function/Resource/mesh.h"

// 前置声明，避免引入 Assimp 头文件
struct aiNode;
struct aiScene;
struct aiMesh;
struct aiMaterial;

namespace NexAur {
    class Model {
    public:
        // 传递路径解析模型
        Model(const std::string& path);
        Model(std::vector<Mesh> meshes, std::string debug_name);
        ~Model() = default;

        // 获取解析出来的子网格体
        const std::vector<Mesh>& getMeshes() const { return m_meshes; }

        // 获取模型所在目录
        const std::string& getDirectory() const { return m_directory; }
        
        // 加载模型状态
        bool isLoaded() const { return m_is_loaded; }

    private:
        void loadModel(const std::string& path);
        void processNode(aiNode* node, const aiScene* scene);
        Mesh processMesh(aiMesh* mesh, const aiScene* scene);
        std::string loadMaterialTexturePath(aiMaterial* material, int type);
        
    private:
        std::vector<Mesh> m_meshes;
        std::string m_directory;
        bool m_is_loaded = false;
    };
} // namespace NexAur
