#include "pch.h"
#include "procedural_model_builder.h"

#include "Function/Resource/model.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace NexAur {
    namespace {
        glm::vec3 safeNormalize(const glm::vec3& value, const glm::vec3& fallback) {
            const float length_squared = glm::dot(value, value);
            if (length_squared <= 0.000001f) {
                return fallback;
            }

            return value / std::sqrt(length_squared);
        }

        Vertex makeVertex(
            const glm::vec3& position,
            const glm::vec3& normal,
            const glm::vec2& uv,
            const glm::vec3& tangent = glm::vec3{ 0.0f },
            const glm::vec3& bitangent = glm::vec3{ 0.0f }) {
            Vertex vertex;
            vertex.position = position;
            vertex.normal = normal;
            vertex.texCoords = uv;
            vertex.tangent = tangent;
            vertex.bitangent = bitangent;
            return vertex;
        }

        void assignQuadTangentSpace(std::vector<Vertex>& vertices, unsigned int base_index) {
            const glm::vec3 edge1 = vertices[base_index + 1].position - vertices[base_index].position;
            const glm::vec3 edge2 = vertices[base_index + 2].position - vertices[base_index].position;
            const glm::vec2 delta_uv1 = vertices[base_index + 1].texCoords - vertices[base_index].texCoords;
            const glm::vec2 delta_uv2 = vertices[base_index + 2].texCoords - vertices[base_index].texCoords;

            const float denominator = delta_uv1.x * delta_uv2.y - delta_uv2.x * delta_uv1.y;
            if (std::abs(denominator) <= 0.000001f) {
                return;
            }

            const float inverse_denominator = 1.0f / denominator;
            const glm::vec3 tangent = safeNormalize(
                (edge1 * delta_uv2.y - edge2 * delta_uv1.y) * inverse_denominator,
                glm::vec3{ 1.0f, 0.0f, 0.0f });
            const glm::vec3 bitangent = safeNormalize(
                (edge2 * delta_uv1.x - edge1 * delta_uv2.x) * inverse_denominator,
                glm::vec3{ 0.0f, 1.0f, 0.0f });

            for (unsigned int index = 0; index < 4; ++index) {
                vertices[base_index + index].tangent = tangent;
                vertices[base_index + index].bitangent = bitangent;
            }
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
            assignQuadTangentSpace(vertices, base_index);

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
                const glm::vec3 normal = safeNormalize({ x_pos, y_pos, z_pos }, glm::vec3{ 0.0f, 1.0f, 0.0f });
                const float phi = u * 2.0f * kPi;
                const glm::vec3 tangent = safeNormalize(
                    { -std::sin(phi), 0.0f, std::cos(phi) },
                    glm::vec3{ 1.0f, 0.0f, 0.0f });
                const glm::vec3 bitangent = safeNormalize(glm::cross(normal, tangent), glm::vec3{ 0.0f, 1.0f, 0.0f });
                vertices.push_back(makeVertex({ x_pos, y_pos, z_pos }, normal, { u, v }, tangent, bitangent));
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
