#pragma once
#include <string>
#include <vector>

#include <glm/mat4x4.hpp>

#include "Function/Resource/mesh.h"

namespace NexAur {
    class Model {
    public:
        Model(std::vector<Mesh> meshes, std::string debug_name);
        ~Model() = default;

        // 获取解析出来的子网格体
        const std::vector<Mesh>& getMeshes() const { return m_meshes; }

        // 获取模型所在目录
        const std::string& getDirectory() const { return m_directory; }
        
        // 加载模型状态
        bool isLoaded() const { return m_is_loaded; }

    private:
        std::vector<Mesh> m_meshes;
        std::string m_directory;
        bool m_is_loaded = false;
    };
} // namespace NexAur
