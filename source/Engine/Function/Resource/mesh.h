#pragma once
#include <vector>
#include <memory>
#include <string>

#include "glm/glm.hpp"

namespace NexAur {
    // 标准顶点结构
    struct Vertex {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec2 texCoords;
        glm::vec3 tangent;
        glm::vec3 bitangent;
    };

    // 贴图路径
    struct MaterialData {
        std::string name;
        std::string albedo_path;
        std::string normal_path;
        std::string metallic_path;
        std::string roughness_path;
        std::string ao_path;
    };

    // 网格体
    class Mesh {
    public:
        Mesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices, const MaterialData& material)
            : m_vertices(vertices), m_indices(indices), m_material(material) {}

        const std::vector<Vertex>& GetVertices() const { return m_vertices; }
        const std::vector<unsigned int>& GetIndices() const { return m_indices; }
        const MaterialData& GetMaterialData() const { return m_material; }

    private:
        std::vector<Vertex> m_vertices;
        std::vector<unsigned int> m_indices;
        MaterialData m_material;
    };
} // namespace NexAur