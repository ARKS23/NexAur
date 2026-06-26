#pragma once
#include <memory>
#include <string>
#include <vector>

#include "glm/glm.hpp"

#include "Function/Resource/material_types.h"

namespace NexAur {
    struct Vertex {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec2 texCoords;
        glm::vec3 tangent;
        glm::vec3 bitangent;
    };

    class Mesh {
    public:
        Mesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices, const MaterialImportData& material)
            : m_vertices(vertices), m_indices(indices), m_material(material) {}

        const std::vector<Vertex>& GetVertices() const { return m_vertices; }
        const std::vector<unsigned int>& GetIndices() const { return m_indices; }
        const MaterialImportData& getMaterialImportData() const { return m_material; }

    private:
        std::vector<Vertex> m_vertices;
        std::vector<unsigned int> m_indices;
        MaterialImportData m_material;
    };
} // namespace NexAur
