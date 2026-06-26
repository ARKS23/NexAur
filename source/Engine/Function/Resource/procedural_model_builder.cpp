#include "pch.h"
#include "procedural_model_builder.h"

#include "Function/Resource/model.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace NexAur {
    namespace {
        Vertex makeVertex(const glm::vec3& position, const glm::vec3& normal, const glm::vec2& uv) {
            Vertex vertex;
            vertex.position = position;
            vertex.normal = normal;
            vertex.texCoords = uv;
            vertex.tangent = glm::vec3{ 0.0f };
            vertex.bitangent = glm::vec3{ 0.0f };
            return vertex;
        }

        void appendQuad(
            std::vector<Vertex>& vertices,
            std::vector<unsigned int>& indices,
            const std::array<glm::vec3, 4>& positions,
            const glm::vec3& normal) {
            const unsigned int base_index = static_cast<unsigned int>(vertices.size());
            vertices.push_back(makeVertex(positions[0], normal, { 0.0f, 0.0f }));
            vertices.push_back(makeVertex(positions[1], normal, { 1.0f, 0.0f }));
            vertices.push_back(makeVertex(positions[2], normal, { 1.0f, 1.0f }));
            vertices.push_back(makeVertex(positions[3], normal, { 0.0f, 1.0f }));

            indices.insert(indices.end(), {
                base_index,
                base_index + 1,
                base_index + 2,
                base_index + 2,
                base_index + 3,
                base_index
            });
        }
    } // namespace

    std::shared_ptr<Model> ProceduralModelBuilder::createCubeModel(const MaterialImportData& material) {
        std::vector<Vertex> vertices;
        std::vector<unsigned int> indices;
        vertices.reserve(24);
        indices.reserve(36);

        appendQuad(vertices, indices, { {
            { -0.5f, -0.5f, -0.5f },
            { 0.5f, -0.5f, -0.5f },
            { 0.5f, 0.5f, -0.5f },
            { -0.5f, 0.5f, -0.5f }
        } }, { 0.0f, 0.0f, -1.0f });

        appendQuad(vertices, indices, { {
            { 0.5f, -0.5f, 0.5f },
            { -0.5f, -0.5f, 0.5f },
            { -0.5f, 0.5f, 0.5f },
            { 0.5f, 0.5f, 0.5f }
        } }, { 0.0f, 0.0f, 1.0f });

        appendQuad(vertices, indices, { {
            { -0.5f, -0.5f, 0.5f },
            { -0.5f, -0.5f, -0.5f },
            { -0.5f, 0.5f, -0.5f },
            { -0.5f, 0.5f, 0.5f }
        } }, { -1.0f, 0.0f, 0.0f });

        appendQuad(vertices, indices, { {
            { 0.5f, -0.5f, -0.5f },
            { 0.5f, -0.5f, 0.5f },
            { 0.5f, 0.5f, 0.5f },
            { 0.5f, 0.5f, -0.5f }
        } }, { 1.0f, 0.0f, 0.0f });

        appendQuad(vertices, indices, { {
            { -0.5f, -0.5f, 0.5f },
            { 0.5f, -0.5f, 0.5f },
            { 0.5f, -0.5f, -0.5f },
            { -0.5f, -0.5f, -0.5f }
        } }, { 0.0f, -1.0f, 0.0f });

        appendQuad(vertices, indices, { {
            { -0.5f, 0.5f, -0.5f },
            { 0.5f, 0.5f, -0.5f },
            { 0.5f, 0.5f, 0.5f },
            { -0.5f, 0.5f, 0.5f }
        } }, { 0.0f, 1.0f, 0.0f });

        std::vector<Mesh> meshes;
        meshes.emplace_back(vertices, indices, material);
        return std::make_shared<Model>(std::move(meshes), "ProceduralCube");
    }

    std::shared_ptr<Model> ProceduralModelBuilder::createSphereModel(
        unsigned int x_segments,
        unsigned int y_segments,
        const MaterialImportData& material) {
        constexpr float kPi = 3.14159265359f;
        x_segments = std::max(3u, x_segments);
        y_segments = std::max(2u, y_segments);

        std::vector<Vertex> vertices;
        std::vector<unsigned int> indices;
        vertices.reserve(static_cast<size_t>((x_segments + 1) * (y_segments + 1)));
        indices.reserve(static_cast<size_t>(x_segments * y_segments * 6));

        for (unsigned int y = 0; y <= y_segments; ++y) {
            for (unsigned int x = 0; x <= x_segments; ++x) {
                const float u = static_cast<float>(x) / static_cast<float>(x_segments);
                const float v = static_cast<float>(y) / static_cast<float>(y_segments);
                const float x_pos = std::cos(u * 2.0f * kPi) * std::sin(v * kPi);
                const float y_pos = std::cos(v * kPi);
                const float z_pos = std::sin(u * 2.0f * kPi) * std::sin(v * kPi);
                vertices.push_back(makeVertex({ x_pos, y_pos, z_pos }, { x_pos, y_pos, z_pos }, { u, v }));
            }
        }

        for (unsigned int y = 0; y < y_segments; ++y) {
            for (unsigned int x = 0; x < x_segments; ++x) {
                const unsigned int current = y * (x_segments + 1) + x;
                const unsigned int next = current + 1;
                const unsigned int below = (y + 1) * (x_segments + 1) + x;
                const unsigned int below_next = below + 1;

                indices.push_back(current);
                indices.push_back(below);
                indices.push_back(next);
                indices.push_back(next);
                indices.push_back(below);
                indices.push_back(below_next);
            }
        }

        std::vector<Mesh> meshes;
        meshes.emplace_back(vertices, indices, material);
        return std::make_shared<Model>(std::move(meshes), "ProceduralSphere");
    }
} // namespace NexAur
