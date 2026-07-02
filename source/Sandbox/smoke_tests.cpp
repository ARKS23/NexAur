#include <iostream>
#include <array>
#include <filesystem>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <sstream>
#include <string_view>
#include <vector>
#include "NexAur.h"
#include "Core/Module/engine_module.h"
#include "Editor/Commands/editor_command.h"
#include "Editor/editor_config.h"
#include "Function/Audio/audio_service.h"
#include "Function/File/file_system.h"
#include "Function/Game/game_state.h"
#include "Function/Game/gameplay_systems.h"
#include "Function/Game/prefab_factory.h"
#include "Function/Game/runtime_camera_controller_system.h"
#include "Function/Global/global_context.h"
#include "Function/Input/input_action_system.h"
#include "Function/Physics/trigger_overlap_system.h"
#include "Function/Platform/platform_services.h"
#include "Function/Renderer/data/render_context.h"
#include "Function/Renderer/data/render_data.h"
#include "Function/Renderer/frontend/render_scene_frame_builder.h"
#include "Function/Resource/asset_manager.h"
#include "Function/Resource/material_asset.h"
#include "Function/Resource/model.h"
#include "Function/Resource/texture_asset.h"
#include "Function/Scene/collision_component.h"
#include "Function/Scene/component.h"
#include "Function/Scene/scene_serializer.h"
#include "Function/Scene/scene_service.h"
#include "Function/Scene/gameplay_component.h"
#include "Function/Scene/scene_v2.h"
#include "scene_test.h"
#include "smoke_tests.h"

namespace {
    class FakeInputService final : public NexAur::InputService {
    public:
        void update() override {}
        const NexAur::InputState& getState() const override { return m_state; }

        void setKey(NexAur::KeyCode key_code, bool pressed) {
            m_state.setKeyPressed(key_code, pressed);
        }

        void setMouse(NexAur::MouseCode mouse_code, bool pressed) {
            m_state.setMouseButtonPressed(mouse_code, pressed);
        }

    private:
        NexAur::InputState m_state;
    };

    bool expectInputAction(bool condition, const std::string& message, std::string& failure) {
        if (condition) {
            return true;
        }

        failure = message;
        return false;
    }

    bool nearlyEqual(float lhs, float rhs, float epsilon = 0.001f) {
        return std::abs(lhs - rhs) <= epsilon;
    }

    bool nearlyEqualVec3(const glm::vec3& lhs, const glm::vec3& rhs, float epsilon = 0.001f) {
        return nearlyEqual(lhs.x, rhs.x, epsilon)
            && nearlyEqual(lhs.y, rhs.y, epsilon)
            && nearlyEqual(lhs.z, rhs.z, epsilon);
    }

    bool expectGameplay(bool condition, const std::string& message, std::string& failure) {
        if (condition) {
            return true;
        }

        failure = message;
        return false;
    }

    void writeU16LE(std::ofstream& output, uint16_t value) {
        output.put(static_cast<char>(value & 0xffu));
        output.put(static_cast<char>((value >> 8u) & 0xffu));
    }

    void writeU32LE(std::ofstream& output, uint32_t value) {
        output.put(static_cast<char>(value & 0xffu));
        output.put(static_cast<char>((value >> 8u) & 0xffu));
        output.put(static_cast<char>((value >> 16u) & 0xffu));
        output.put(static_cast<char>((value >> 24u) & 0xffu));
    }

    template <typename T>
    void appendBinary(std::vector<unsigned char>& output, const T& value) {
        const size_t offset = output.size();
        output.resize(offset + sizeof(T));
        std::memcpy(output.data() + offset, &value, sizeof(T));
    }

    void padTo4(std::vector<unsigned char>& output, unsigned char value) {
        while (output.size() % 4u != 0u) {
            output.push_back(value);
        }
    }

    std::vector<unsigned char> decodeBase64(std::string_view text) {
        constexpr std::string_view kAlphabet =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

        std::vector<unsigned char> output;
        int value = 0;
        int bits = -8;
        for (unsigned char character : text) {
            if (character == '=') {
                break;
            }

            const size_t digit = kAlphabet.find(static_cast<char>(character));
            if (digit == std::string_view::npos) {
                continue;
            }

            value = (value << 6) + static_cast<int>(digit);
            bits += 6;
            if (bits >= 0) {
                output.push_back(static_cast<unsigned char>((value >> bits) & 0xff));
                bits -= 8;
            }
        }

        return output;
    }

    constexpr std::string_view tinyPngBase64() {
        return "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+/p9sAAAAASUVORK5CYII=";
    }

    std::vector<unsigned char> tinyPngBytes() {
        return decodeBase64(tinyPngBase64());
    }

    struct GltfSmokeVertex {
        float px;
        float py;
        float pz;
        float nx;
        float ny;
        float nz;
        float u;
        float v;
    };

    bool writeTinyGltfSmokeCubeGlb(const std::filesystem::path& path) {
        if (path.has_parent_path()) {
            std::filesystem::create_directories(path.parent_path());
        }

        const std::array<GltfSmokeVertex, 24> vertices = { {
            { -0.5f, -0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f },
            { 0.5f, -0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f },
            { 0.5f, 0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f },
            { -0.5f, 0.5f, -0.5f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f },

            { 0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f },
            { -0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f },
            { -0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f },
            { 0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f },

            { -0.5f, -0.5f, 0.5f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f },
            { -0.5f, -0.5f, -0.5f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f },
            { -0.5f, 0.5f, -0.5f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f },
            { -0.5f, 0.5f, 0.5f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f },

            { 0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f },
            { 0.5f, -0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f },
            { 0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f },
            { 0.5f, 0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f },

            { -0.5f, -0.5f, 0.5f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f },
            { 0.5f, -0.5f, 0.5f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f },
            { 0.5f, -0.5f, -0.5f, 0.0f, -1.0f, 0.0f, 1.0f, 1.0f },
            { -0.5f, -0.5f, -0.5f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f },

            { -0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f },
            { 0.5f, 0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f },
            { 0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f },
            { -0.5f, 0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f },
        } };

        const std::array<uint16_t, 36> indices = { {
            0, 1, 2, 2, 3, 0,
            4, 5, 6, 6, 7, 4,
            8, 9, 10, 10, 11, 8,
            12, 13, 14, 14, 15, 12,
            16, 17, 18, 18, 19, 16,
            20, 21, 22, 22, 23, 20,
        } };

        std::vector<unsigned char> binary;
        binary.reserve(vertices.size() * sizeof(GltfSmokeVertex) + indices.size() * sizeof(uint16_t));
        for (const GltfSmokeVertex& vertex : vertices) {
            appendBinary(binary, vertex);
        }
        const size_t index_buffer_offset = binary.size();
        for (uint16_t index : indices) {
            appendBinary(binary, index);
        }
        const size_t vertex_buffer_size = vertices.size() * sizeof(GltfSmokeVertex);
        const size_t index_buffer_size = indices.size() * sizeof(uint16_t);

        std::ostringstream json_stream;
        json_stream
            << "{\"asset\":{\"version\":\"2.0\",\"generator\":\"NexAur TinyGltfGeometrySmoke\"},"
            << "\"scene\":0,\"scenes\":[{\"nodes\":[0]}],"
            << "\"nodes\":[{\"name\":\"TinyGltfSmokeCube\",\"mesh\":0}],"
            << "\"materials\":[{\"name\":\"TinyGltfSmokeMaterial\",\"pbrMetallicRoughness\":"
            << "{\"baseColorFactor\":[0.8,0.7,0.4,1.0],\"metallicFactor\":0.0,\"roughnessFactor\":0.8}}],"
            << "\"meshes\":[{\"name\":\"TinyGltfSmokeCube\",\"primitives\":[{\"attributes\":"
            << "{\"POSITION\":0,\"NORMAL\":1,\"TEXCOORD_0\":2},\"indices\":3,\"material\":0}]}],"
            << "\"buffers\":[{\"byteLength\":" << binary.size() << "}],"
            << "\"bufferViews\":["
            << "{\"buffer\":0,\"byteOffset\":0,\"byteLength\":" << vertex_buffer_size
            << ",\"byteStride\":" << sizeof(GltfSmokeVertex) << ",\"target\":34962},"
            << "{\"buffer\":0,\"byteOffset\":" << index_buffer_offset
            << ",\"byteLength\":" << index_buffer_size << ",\"target\":34963}],"
            << "\"accessors\":["
            << "{\"bufferView\":0,\"byteOffset\":0,\"componentType\":5126,\"count\":" << vertices.size()
            << ",\"type\":\"VEC3\",\"min\":[-0.5,-0.5,-0.5],\"max\":[0.5,0.5,0.5]},"
            << "{\"bufferView\":0,\"byteOffset\":12,\"componentType\":5126,\"count\":" << vertices.size()
            << ",\"type\":\"VEC3\"},"
            << "{\"bufferView\":0,\"byteOffset\":24,\"componentType\":5126,\"count\":" << vertices.size()
            << ",\"type\":\"VEC2\"},"
            << "{\"bufferView\":1,\"byteOffset\":0,\"componentType\":5123,\"count\":" << indices.size()
            << ",\"type\":\"SCALAR\"}]}";

        const std::string json_string = json_stream.str();
        std::vector<unsigned char> json(json_string.begin(), json_string.end());
        padTo4(json, static_cast<unsigned char>(' '));
        padTo4(binary, 0);

        const uint32_t total_length = static_cast<uint32_t>(12 + 8 + json.size() + 8 + binary.size());
        std::ofstream output(path, std::ios::binary);
        if (!output) {
            return false;
        }

        writeU32LE(output, 0x46546C67u);
        writeU32LE(output, 2u);
        writeU32LE(output, total_length);
        writeU32LE(output, static_cast<uint32_t>(json.size()));
        writeU32LE(output, 0x4E4F534Au);
        output.write(reinterpret_cast<const char*>(json.data()), static_cast<std::streamsize>(json.size()));
        writeU32LE(output, static_cast<uint32_t>(binary.size()));
        writeU32LE(output, 0x004E4942u);
        output.write(reinterpret_cast<const char*>(binary.data()), static_cast<std::streamsize>(binary.size()));
        return output.good();
    }

    bool writeTinyGltfMaterialSmokeGlb(const std::filesystem::path& path) {
        if (path.has_parent_path()) {
            std::filesystem::create_directories(path.parent_path());
        }

        const std::array<GltfSmokeVertex, 4> vertices = { {
            { -0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f },
            { 0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f },
            { 0.5f, 0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f },
            { -0.5f, 0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f },
        } };
        const std::array<uint16_t, 6> indices = { { 0, 1, 2, 2, 3, 0 } };
        const std::vector<unsigned char> png = tinyPngBytes();
        if (png.empty()) {
            return false;
        }

        std::vector<unsigned char> binary;
        binary.reserve(vertices.size() * sizeof(GltfSmokeVertex) + indices.size() * sizeof(uint16_t) + png.size() * 5);
        for (const GltfSmokeVertex& vertex : vertices) {
            appendBinary(binary, vertex);
        }
        const size_t index_buffer_offset = binary.size();
        for (uint16_t index : indices) {
            appendBinary(binary, index);
        }
        padTo4(binary, 0);

        std::array<size_t, 5> image_offsets{};
        std::array<size_t, 5> image_sizes{};
        for (size_t image_index = 0; image_index < image_offsets.size(); ++image_index) {
            image_offsets[image_index] = binary.size();
            image_sizes[image_index] = png.size();
            binary.insert(binary.end(), png.begin(), png.end());
            padTo4(binary, 0);
        }

        const size_t vertex_buffer_size = vertices.size() * sizeof(GltfSmokeVertex);
        const size_t index_buffer_size = indices.size() * sizeof(uint16_t);

        std::ostringstream json_stream;
        json_stream
            << "{\"asset\":{\"version\":\"2.0\",\"generator\":\"NexAur TinyGltfMaterialSmoke\"},"
            << "\"scene\":0,\"scenes\":[{\"nodes\":[0]}],"
            << "\"nodes\":[{\"name\":\"TinyGltfMaterialQuad\",\"mesh\":0}],"
            << "\"samplers\":[{\"magFilter\":9729,\"minFilter\":9729,\"wrapS\":10497,\"wrapT\":10497}],"
            << "\"images\":["
            << "{\"name\":\"BaseColorEmbedded\",\"bufferView\":2,\"mimeType\":\"image/png\"},"
            << "{\"name\":\"MetallicRoughnessEmbedded\",\"bufferView\":3,\"mimeType\":\"image/png\"},"
            << "{\"name\":\"NormalEmbedded\",\"bufferView\":4,\"mimeType\":\"image/png\"},"
            << "{\"name\":\"OcclusionEmbedded\",\"bufferView\":5,\"mimeType\":\"image/png\"},"
            << "{\"name\":\"EmissiveEmbedded\",\"bufferView\":6,\"mimeType\":\"image/png\"}],"
            << "\"textures\":["
            << "{\"sampler\":0,\"source\":0},{\"sampler\":0,\"source\":1},{\"sampler\":0,\"source\":2},"
            << "{\"sampler\":0,\"source\":3},{\"sampler\":0,\"source\":4}],"
            << "\"materials\":[{\"name\":\"TinyGltfMaterialSmoke\",\"doubleSided\":true,"
            << "\"alphaMode\":\"MASK\",\"alphaCutoff\":0.42,"
            << "\"emissiveFactor\":[0.1,0.2,0.3],\"emissiveTexture\":{\"index\":4},"
            << "\"normalTexture\":{\"index\":2,\"scale\":0.5},"
            << "\"occlusionTexture\":{\"index\":3,\"strength\":0.25},"
            << "\"pbrMetallicRoughness\":{\"baseColorFactor\":[0.2,0.4,0.6,0.8],"
            << "\"metallicFactor\":0.7,\"roughnessFactor\":0.3,"
            << "\"baseColorTexture\":{\"index\":0},\"metallicRoughnessTexture\":{\"index\":1}}}],"
            << "\"meshes\":[{\"name\":\"TinyGltfMaterialQuad\",\"primitives\":[{\"attributes\":"
            << "{\"POSITION\":0,\"NORMAL\":1,\"TEXCOORD_0\":2},\"indices\":3,\"material\":0}]}],"
            << "\"buffers\":[{\"byteLength\":" << binary.size() << "}],"
            << "\"bufferViews\":["
            << "{\"buffer\":0,\"byteOffset\":0,\"byteLength\":" << vertex_buffer_size
            << ",\"byteStride\":" << sizeof(GltfSmokeVertex) << ",\"target\":34962},"
            << "{\"buffer\":0,\"byteOffset\":" << index_buffer_offset
            << ",\"byteLength\":" << index_buffer_size << ",\"target\":34963}";

        for (size_t image_index = 0; image_index < image_offsets.size(); ++image_index) {
            json_stream
                << ",{\"buffer\":0,\"byteOffset\":" << image_offsets[image_index]
                << ",\"byteLength\":" << image_sizes[image_index] << "}";
        }

        json_stream
            << "],\"accessors\":["
            << "{\"bufferView\":0,\"byteOffset\":0,\"componentType\":5126,\"count\":" << vertices.size()
            << ",\"type\":\"VEC3\",\"min\":[-0.5,-0.5,0.0],\"max\":[0.5,0.5,0.0]},"
            << "{\"bufferView\":0,\"byteOffset\":12,\"componentType\":5126,\"count\":" << vertices.size()
            << ",\"type\":\"VEC3\"},"
            << "{\"bufferView\":0,\"byteOffset\":24,\"componentType\":5126,\"count\":" << vertices.size()
            << ",\"type\":\"VEC2\"},"
            << "{\"bufferView\":1,\"byteOffset\":0,\"componentType\":5123,\"count\":" << indices.size()
            << ",\"type\":\"SCALAR\"}]}";

        const std::string json_string = json_stream.str();
        std::vector<unsigned char> json(json_string.begin(), json_string.end());
        padTo4(json, static_cast<unsigned char>(' '));
        padTo4(binary, 0);

        const uint32_t total_length = static_cast<uint32_t>(12 + 8 + json.size() + 8 + binary.size());
        std::ofstream output(path, std::ios::binary);
        if (!output) {
            return false;
        }

        writeU32LE(output, 0x46546C67u);
        writeU32LE(output, 2u);
        writeU32LE(output, total_length);
        writeU32LE(output, static_cast<uint32_t>(json.size()));
        writeU32LE(output, 0x4E4F534Au);
        output.write(reinterpret_cast<const char*>(json.data()), static_cast<std::streamsize>(json.size()));
        writeU32LE(output, static_cast<uint32_t>(binary.size()));
        writeU32LE(output, 0x004E4942u);
        output.write(reinterpret_cast<const char*>(binary.data()), static_cast<std::streamsize>(binary.size()));
        return output.good();
    }

    bool writeTinyGltfDataUriMaterialSmokeGltf(
        const std::filesystem::path& gltf_path,
        const std::filesystem::path& bin_path) {
        if (gltf_path.has_parent_path()) {
            std::filesystem::create_directories(gltf_path.parent_path());
        }
        if (bin_path.has_parent_path()) {
            std::filesystem::create_directories(bin_path.parent_path());
        }

        const std::array<GltfSmokeVertex, 4> vertices = { {
            { -0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f },
            { 0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f },
            { 0.5f, 0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f },
            { -0.5f, 0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f },
        } };
        const std::array<uint16_t, 6> indices = { { 0, 1, 2, 2, 3, 0 } };

        std::vector<unsigned char> binary;
        binary.reserve(vertices.size() * sizeof(GltfSmokeVertex) + indices.size() * sizeof(uint16_t));
        for (const GltfSmokeVertex& vertex : vertices) {
            appendBinary(binary, vertex);
        }
        const size_t index_buffer_offset = binary.size();
        for (uint16_t index : indices) {
            appendBinary(binary, index);
        }

        std::ofstream bin_output(bin_path, std::ios::binary);
        if (!bin_output) {
            return false;
        }
        bin_output.write(reinterpret_cast<const char*>(binary.data()), static_cast<std::streamsize>(binary.size()));
        if (!bin_output.good()) {
            return false;
        }

        std::ostringstream json_stream;
        json_stream
            << "{\"asset\":{\"version\":\"2.0\",\"generator\":\"NexAur TinyGltfDataUriMaterialSmoke\"},"
            << "\"scene\":0,\"scenes\":[{\"nodes\":[0]}],"
            << "\"nodes\":[{\"name\":\"TinyGltfDataUriQuad\",\"mesh\":0}],"
            << "\"images\":[{\"name\":\"BaseColorDataUri\",\"uri\":\"data:image/png;base64,"
            << tinyPngBase64() << "\"}],"
            << "\"textures\":[{\"source\":0}],"
            << "\"materials\":[{\"name\":\"TinyGltfDataUriMaterial\",\"pbrMetallicRoughness\":"
            << "{\"baseColorFactor\":[1.0,1.0,1.0,1.0],\"baseColorTexture\":{\"index\":0}}}],"
            << "\"meshes\":[{\"name\":\"TinyGltfDataUriQuad\",\"primitives\":[{\"attributes\":"
            << "{\"POSITION\":0,\"NORMAL\":1,\"TEXCOORD_0\":2},\"indices\":3,\"material\":0}]}],"
            << "\"buffers\":[{\"uri\":\"" << bin_path.filename().string() << "\",\"byteLength\":" << binary.size() << "}],"
            << "\"bufferViews\":["
            << "{\"buffer\":0,\"byteOffset\":0,\"byteLength\":" << vertices.size() * sizeof(GltfSmokeVertex)
            << ",\"byteStride\":" << sizeof(GltfSmokeVertex) << ",\"target\":34962},"
            << "{\"buffer\":0,\"byteOffset\":" << index_buffer_offset
            << ",\"byteLength\":" << indices.size() * sizeof(uint16_t) << ",\"target\":34963}],"
            << "\"accessors\":["
            << "{\"bufferView\":0,\"byteOffset\":0,\"componentType\":5126,\"count\":" << vertices.size()
            << ",\"type\":\"VEC3\",\"min\":[-0.5,-0.5,0.0],\"max\":[0.5,0.5,0.0]},"
            << "{\"bufferView\":0,\"byteOffset\":12,\"componentType\":5126,\"count\":" << vertices.size()
            << ",\"type\":\"VEC3\"},"
            << "{\"bufferView\":0,\"byteOffset\":24,\"componentType\":5126,\"count\":" << vertices.size()
            << ",\"type\":\"VEC2\"},"
            << "{\"bufferView\":1,\"byteOffset\":0,\"componentType\":5123,\"count\":" << indices.size()
            << ",\"type\":\"SCALAR\"}]}";

        std::ofstream gltf_output(gltf_path, std::ios::binary);
        if (!gltf_output) {
            return false;
        }
        const std::string json = json_stream.str();
        gltf_output.write(json.data(), static_cast<std::streamsize>(json.size()));
        return gltf_output.good();
    }

    bool writeTinyGltfSceneSmokeGltf(
        const std::filesystem::path& gltf_path,
        const std::filesystem::path& bin_path) {
        if (gltf_path.has_parent_path()) {
            std::filesystem::create_directories(gltf_path.parent_path());
        }
        if (bin_path.has_parent_path()) {
            std::filesystem::create_directories(bin_path.parent_path());
        }

        const std::array<GltfSmokeVertex, 6> vertices = { {
            { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f },
            { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f },
            { 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f },
            { 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f },
            { 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f },
            { 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f },
        } };
        const std::array<uint16_t, 6> indices = { { 0, 1, 2, 0, 1, 2 } };

        std::vector<unsigned char> binary;
        binary.reserve(vertices.size() * sizeof(GltfSmokeVertex) + indices.size() * sizeof(uint16_t));
        for (const GltfSmokeVertex& vertex : vertices) {
            appendBinary(binary, vertex);
        }
        const size_t index_buffer_offset = binary.size();
        for (uint16_t index : indices) {
            appendBinary(binary, index);
        }

        std::ofstream bin_output(bin_path, std::ios::binary);
        if (!bin_output) {
            return false;
        }
        bin_output.write(reinterpret_cast<const char*>(binary.data()), static_cast<std::streamsize>(binary.size()));
        if (!bin_output.good()) {
            return false;
        }

        const size_t vertex_stride = sizeof(GltfSmokeVertex);
        const size_t second_primitive_vertex_offset = 3u * vertex_stride;
        const size_t second_primitive_index_offset = 3u * sizeof(uint16_t);
        std::ostringstream json_stream;
        json_stream
            << "{\"asset\":{\"version\":\"2.0\",\"generator\":\"NexAur TinyGltfSceneSmoke\"},"
            << "\"scene\":1,"
            << "\"scenes\":[{\"name\":\"UnusedScene\",\"nodes\":[2]},{\"name\":\"DefaultScene\",\"nodes\":[0]}],"
            << "\"nodes\":["
            << "{\"name\":\"RootMatrix\",\"matrix\":[1,0,0,0,0,1,0,0,0,0,1,0,10,0,0,1],\"children\":[1]},"
            << "{\"name\":\"ChildTRS\",\"mesh\":0,\"translation\":[0,20,30],\"scale\":[2,1,1]},"
            << "{\"name\":\"UnusedNode\",\"mesh\":0,\"translation\":[-100,0,0]}],"
            << "\"materials\":[{\"name\":\"SceneSmokeMat0\"},{\"name\":\"SceneSmokeMat1\"}],"
            << "\"meshes\":[{\"name\":\"SceneSmokeMesh\",\"primitives\":["
            << "{\"attributes\":{\"POSITION\":0,\"NORMAL\":1,\"TEXCOORD_0\":2},\"indices\":3,\"material\":0},"
            << "{\"attributes\":{\"POSITION\":4,\"NORMAL\":5,\"TEXCOORD_0\":6},\"indices\":7,\"material\":1}]}],"
            << "\"buffers\":[{\"uri\":\"" << bin_path.filename().string() << "\",\"byteLength\":" << binary.size() << "}],"
            << "\"bufferViews\":["
            << "{\"buffer\":0,\"byteOffset\":0,\"byteLength\":" << vertices.size() * vertex_stride
            << ",\"byteStride\":" << vertex_stride << ",\"target\":34962},"
            << "{\"buffer\":0,\"byteOffset\":" << index_buffer_offset
            << ",\"byteLength\":" << indices.size() * sizeof(uint16_t) << ",\"target\":34963}],"
            << "\"accessors\":["
            << "{\"bufferView\":0,\"byteOffset\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\",\"min\":[0,0,0],\"max\":[1,1,0]},"
            << "{\"bufferView\":0,\"byteOffset\":12,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"},"
            << "{\"bufferView\":0,\"byteOffset\":24,\"componentType\":5126,\"count\":3,\"type\":\"VEC2\"},"
            << "{\"bufferView\":1,\"byteOffset\":0,\"componentType\":5123,\"count\":3,\"type\":\"SCALAR\"},"
            << "{\"bufferView\":0,\"byteOffset\":" << second_primitive_vertex_offset
            << ",\"componentType\":5126,\"count\":3,\"type\":\"VEC3\",\"min\":[0,0,1],\"max\":[1,1,1]},"
            << "{\"bufferView\":0,\"byteOffset\":" << second_primitive_vertex_offset + 12
            << ",\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"},"
            << "{\"bufferView\":0,\"byteOffset\":" << second_primitive_vertex_offset + 24
            << ",\"componentType\":5126,\"count\":3,\"type\":\"VEC2\"},"
            << "{\"bufferView\":1,\"byteOffset\":" << second_primitive_index_offset
            << ",\"componentType\":5123,\"count\":3,\"type\":\"SCALAR\"}]}";

        std::ofstream gltf_output(gltf_path, std::ios::binary);
        if (!gltf_output) {
            return false;
        }
        const std::string json = json_stream.str();
        gltf_output.write(json.data(), static_cast<std::streamsize>(json.size()));
        return gltf_output.good();
    }

    bool writeAudioSmokeWav(const std::filesystem::path& path) {
        if (path.has_parent_path()) {
            std::filesystem::create_directories(path.parent_path());
        }

        constexpr uint16_t kChannels = 1;
        constexpr uint16_t kBitsPerSample = 16;
        constexpr uint32_t kSampleRate = 8000;
        constexpr uint32_t kSampleCount = 800;
        constexpr uint16_t kBlockAlign = kChannels * kBitsPerSample / 8;
        constexpr uint32_t kByteRate = kSampleRate * kBlockAlign;
        constexpr uint32_t kDataSize = kSampleCount * kBlockAlign;

        std::ofstream output(path, std::ios::binary);
        if (!output) {
            return false;
        }

        output.write("RIFF", 4);
        writeU32LE(output, 36u + kDataSize);
        output.write("WAVE", 4);
        output.write("fmt ", 4);
        writeU32LE(output, 16u);
        writeU16LE(output, 1u);
        writeU16LE(output, kChannels);
        writeU32LE(output, kSampleRate);
        writeU32LE(output, kByteRate);
        writeU16LE(output, kBlockAlign);
        writeU16LE(output, kBitsPerSample);
        output.write("data", 4);
        writeU32LE(output, kDataSize);

        for (uint32_t i = 0; i < kSampleCount; ++i) {
            writeU16LE(output, 0u);
        }

        return true;
    }

    template<typename T>
    const T* findComponentByName(const std::shared_ptr<NexAur::SceneV2>& scene, const std::string& name) {
        if (!scene) {
            return nullptr;
        }

        const entt::registry& registry = scene->getRegistry();
        auto view = registry.view<NexAur::TagComponent>();
        for (entt::entity entity : view) {
            if (view.get<NexAur::TagComponent>(entity).name == name) {
                return registry.try_get<T>(entity);
            }
        }

        return nullptr;
    }

    bool writeAssimpFallbackSmokeObj(const std::filesystem::path& obj_path) {
        if (obj_path.has_parent_path()) {
            std::filesystem::create_directories(obj_path.parent_path());
        }

        std::ofstream output(obj_path, std::ios::binary);
        if (!output) {
            return false;
        }

        output
            << "o AssimpFallbackTriangle\n"
            << "v 0.0 0.0 0.0\n"
            << "v 1.0 0.0 0.0\n"
            << "v 0.0 1.0 0.0\n"
            << "vt 0.0 0.0\n"
            << "vt 1.0 0.0\n"
            << "vt 0.0 1.0\n"
            << "vn 0.0 0.0 1.0\n"
            << "f 1/1/1 2/2/1 3/3/1\n";
        return output.good();
    }
} // namespace

void setupScene() {
    NexAur::SceneTestClass scene_test;
    scene_test.addCornellBox();

    // Material test scene.
    for (int i = 0; i < 5; ++i) {
        scene_test.addSphereEntity("gold Sphere" + std::to_string(i), "gold", glm::vec3(i * 1.5f, 0.0f, -2.0f), glm::vec3(0.0f), glm::vec3(0.5f));
        scene_test.addCubeEntity("gold Cube" + std::to_string(i), "gold", glm::vec3(i * 1.5f, 0.0f, 0.0f), glm::vec3(0.0f), glm::vec3(0.5f));
    }

    for (int i = 0; i < 5; ++i) {
        scene_test.addSphereEntity("rusted_iron Sphere" + std::to_string(i), "rusted_iron", glm::vec3(i * 1.5f, 1.0f, -2.0f), glm::vec3(0.0f), glm::vec3(0.5f));
        scene_test.addCubeEntity("rusted_iron Cube" + std::to_string(i), "rusted_iron", glm::vec3(i * 1.5f, 1.0f, -0.0f), glm::vec3(0.0f), glm::vec3(0.5f));
    }

    for (int i = 0; i < 5; ++i) {
        scene_test.addSphereEntity("grass Sphere" + std::to_string(i), "grass", glm::vec3(i * 1.5f, 2.0f, -2.0f), glm::vec3(0.0f), glm::vec3(0.5f));
        scene_test.addCubeEntity("grass Cube" + std::to_string(i), "grass", glm::vec3(i * 1.5f, 2.0f, -0.0f), glm::vec3(0.0f), glm::vec3(0.5f));
    }

    for (int i = 0; i < 5; ++i) {
        scene_test.addSphereEntity("plastic Sphere" + std::to_string(i), "plastic", glm::vec3(i * 1.5f, 3.0f, -2.0f), glm::vec3(0.0f), glm::vec3(0.5f));
        scene_test.addCubeEntity("plastic Cube" + std::to_string(i), "plastic", glm::vec3(i * 1.5f, 3.0f, -0.0f), glm::vec3(0.0f), glm::vec3(0.5f));
    }

    for (int i = 0; i < 5; ++i) {
        scene_test.addSphereEntity("wall Sphere" + std::to_string(i), "wall", glm::vec3(i * 1.5f, 4.0f, -2.0f), glm::vec3(0.0f), glm::vec3(0.5f));
        scene_test.addCubeEntity("wall Cube" + std::to_string(i), "wall", glm::vec3(i * 1.5f, 4.0f, -0.0f), glm::vec3(0.0f), glm::vec3(0.5f));
    }

    scene_test.addCubeEntity("Floor", "plastic", glm::vec3(5.0f, -3.5f, -5.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(50.0f, 0.1f, 50.0f));

    // DamagedHelmet is an optional local sample asset; skip it when the asset pack is missing.
    const std::string damaged_helmet_path = NX_ASSET("assets/models/DamagedHelmet/DamagedHelmet.gltf");
    if (std::filesystem::exists(damaged_helmet_path)) {
        scene_test.addImportedModelEntity("DamagedHelmet", damaged_helmet_path, glm::vec3(5.0f, 0.0f, 4.0f));
    } else {
        NX_CORE_WARN("Optional sample model missing: {0}. See assets/asset_manifest.md.", damaged_helmet_path);
    }
}

bool hasEntityNamed(const std::shared_ptr<NexAur::SceneV2>& scene, const std::string& name) {
    if (!scene) {
        return false;
    }

    const entt::registry& registry = scene->getRegistry();
    auto view = registry.view<NexAur::TagComponent>();
    for (entt::entity entity : view) {
        if (view.get<NexAur::TagComponent>(entity).name == name) {
            return true;
        }
    }

    return false;
}

bool defaultEnvironmentCalibrationMatches(const std::shared_ptr<NexAur::SceneV2>& scene) {
    if (!scene) {
        return false;
    }

    const entt::registry& registry = scene->getRegistry();
    auto view = registry.view<NexAur::TagComponent, NexAur::EnvironmentComponent>();
    for (entt::entity entity : view) {
        const auto& tag = view.get<NexAur::TagComponent>(entity);
        if (tag.name != "Environment") {
            continue;
        }

        const auto& environment = view.get<NexAur::EnvironmentComponent>(entity);
        return nearlyEqual(environment.intensity, 0.65f) &&
               nearlyEqual(environment.skybox_intensity, 0.75f) &&
               nearlyEqual(environment.ibl_intensity, 0.65f);
    }

    return false;
}

bool configureDefaultPointLightShadow(const std::shared_ptr<NexAur::SceneV2>& scene) {
    if (!scene) {
        return false;
    }

    entt::registry& registry = scene->getRegistry();
    auto view = registry.view<NexAur::TagComponent, NexAur::PointLightComponent>();
    for (entt::entity entity : view) {
        const auto& tag = view.get<NexAur::TagComponent>(entity);
        if (tag.name != "PointLight") {
            continue;
        }

        auto& light = view.get<NexAur::PointLightComponent>(entity);
        light.cast_shadow = true;
        light.shadow_range = 9.5f;
        light.shadow_strength = 0.72f;
        return true;
    }

    return false;
}

bool defaultPointLightShadowMatches(const std::shared_ptr<NexAur::SceneV2>& scene) {
    if (!scene) {
        return false;
    }

    const entt::registry& registry = scene->getRegistry();
    auto view = registry.view<NexAur::TagComponent, NexAur::PointLightComponent>();
    for (entt::entity entity : view) {
        const auto& tag = view.get<NexAur::TagComponent>(entity);
        if (tag.name != "PointLight") {
            continue;
        }

        const auto& light = view.get<NexAur::PointLightComponent>(entity);
        return light.cast_shadow &&
               nearlyEqual(light.shadow_range, 9.5f) &&
               nearlyEqual(light.shadow_strength, 0.72f);
    }

    return false;
}

bool configureDefaultRectLight(const std::shared_ptr<NexAur::SceneV2>& scene) {
    if (!scene) {
        return false;
    }

    NexAur::Entity entity = scene->createEntity("RectLight");
    auto& light = entity.addComponent<NexAur::RectLightComponent>();
    light.color = glm::vec3{ 1.0f, 0.85f, 0.65f };
    light.intensity = 6.5f;
    light.size = glm::vec2{ 2.25f, 1.15f };
    light.range = 9.0f;
    light.two_sided = true;
    light.cast_shadow = true;
    light.shadow_strength = 0.74f;

    auto& transform = entity.getComponent<NexAur::TransformComponent>();
    transform.translation = glm::vec3{ 0.0f, 3.25f, 0.0f };
    transform.rotation = glm::vec3{ 0.0f };
    return true;
}

bool defaultRectLightMatches(const std::shared_ptr<NexAur::SceneV2>& scene) {
    if (!scene) {
        return false;
    }

    const entt::registry& registry = scene->getRegistry();
    auto view = registry.view<NexAur::TagComponent, NexAur::RectLightComponent>();
    for (entt::entity entity : view) {
        const auto& tag = view.get<NexAur::TagComponent>(entity);
        if (tag.name != "RectLight") {
            continue;
        }

        const auto& light = view.get<NexAur::RectLightComponent>(entity);
        return nearlyEqual(light.color.r, 1.0f) &&
               nearlyEqual(light.color.g, 0.85f) &&
               nearlyEqual(light.color.b, 0.65f) &&
               nearlyEqual(light.intensity, 6.5f) &&
               nearlyEqual(light.size.x, 2.25f) &&
               nearlyEqual(light.size.y, 1.15f) &&
               nearlyEqual(light.range, 9.0f) &&
               light.two_sided &&
               light.cast_shadow &&
               nearlyEqual(light.shadow_strength, 0.74f);
    }

    return false;
}

int runSceneSerializerSmoke() {
    NexAur::Engine engine;
    engine.startEngine();

    bool success = true;
    std::string failure;

    NexAur::ModuleRegistry* registry = NexAur::g_runtime_global_context.getModuleRegistry();
    std::shared_ptr<NexAur::SceneService> scene_service =
        registry ? registry->getService<NexAur::SceneService>() : nullptr;
    std::shared_ptr<NexAur::AssetManager> asset_manager =
        registry ? registry->getService<NexAur::AssetManager>() : nullptr;

    const std::filesystem::path smoke_path =
        std::filesystem::path(ENGINE_ROOT_DIR) / "build" / "scene_serializer_smoke.nxscene";

    if (!scene_service || !scene_service->getActiveScene()) {
        success = false;
        failure = "SceneSerializer smoke failed: no active scene.";
    } else if (!asset_manager) {
        success = false;
        failure = "SceneSerializer smoke failed: no AssetManager service.";
    } else if (!configureDefaultPointLightShadow(scene_service->getActiveScene())) {
        success = false;
        failure = "SceneSerializer smoke failed: could not configure point light shadow settings.";
    } else if (!configureDefaultRectLight(scene_service->getActiveScene())) {
        success = false;
        failure = "SceneSerializer smoke failed: could not configure rect light settings.";
    } else {
        NexAur::SceneSerializer serializer(*asset_manager);
        const NexAur::SceneSerializationResult save_result =
            serializer.save(*scene_service->getActiveScene(), smoke_path);
        if (!save_result) {
            success = false;
            failure = save_result.message;
        } else {
            const NexAur::SceneLoadResult load_result = serializer.load(smoke_path);
            if (!load_result) {
                success = false;
                failure = load_result.message;
            } else if (
                !hasEntityNamed(load_result.scene, "MainCamera") ||
                !hasEntityNamed(load_result.scene, "Environment") ||
                !hasEntityNamed(load_result.scene, "DirectionalLight") ||
                !hasEntityNamed(load_result.scene, "PointLight") ||
                !hasEntityNamed(load_result.scene, "RectLight")) {
                success = false;
                failure = "SceneSerializer smoke failed: loaded scene is missing default entities.";
            } else if (!defaultEnvironmentCalibrationMatches(load_result.scene)) {
                success = false;
                failure = "SceneSerializer smoke failed: environment calibration settings were not preserved.";
            } else if (!defaultPointLightShadowMatches(load_result.scene)) {
                success = false;
                failure = "SceneSerializer smoke failed: point light shadow settings were not preserved.";
            } else if (!defaultRectLightMatches(load_result.scene)) {
                success = false;
                failure = "SceneSerializer smoke failed: rect light settings were not preserved.";
            } else {
                NX_CORE_INFO(
                    "SceneSerializer smoke passed. Saved {} entities and loaded {} entities.",
                    save_result.entity_count,
                    load_result.entity_count);
            }
        }
    }

    std::error_code remove_error;
    std::filesystem::remove(smoke_path, remove_error);

    if (!success) {
        NX_CORE_ERROR("{}", failure);
    }

    engine.shutdownEngine();
    return success ? 0 : 1;
}

int runAudioSmoke() {
    NexAur::Engine engine;
    engine.startEngine();

    bool success = true;
    std::string failure;

    auto expect = [&](bool condition, const std::string& message) {
        if (!success) {
            return;
        }
        success = expectGameplay(condition, message, failure);
    };

    NexAur::ModuleRegistry* registry = NexAur::g_runtime_global_context.getModuleRegistry();
    std::shared_ptr<NexAur::AudioService> audio_service =
        registry ? registry->getService<NexAur::AudioService>() : nullptr;

    expect(audio_service != nullptr, "Audio smoke failed: AudioService is not registered.");

    const std::filesystem::path smoke_path =
        std::filesystem::path(ENGINE_ROOT_DIR) / "build" / "audio_smoke.wav";

    if (success) {
        expect(writeAudioSmokeWav(smoke_path), "Audio smoke failed: could not write test wav.");
    }

    if (success && audio_service) {
        audio_service->setMasterVolume(1.25f);
        audio_service->setMusicVolume(0.5f);
        audio_service->setSfxVolume(-0.25f);

        audio_service->playMusic(smoke_path, true, 0.25f);
        audio_service->playOneShot(smoke_path, 0.25f);
        audio_service->pauseAll();
        audio_service->resumeAll();
        audio_service->stopMusic();
    }

    std::error_code remove_error;
    std::filesystem::remove(smoke_path, remove_error);

    if (!success) {
        std::cerr << failure << std::endl;
    } else if (audio_service && !audio_service->isAvailable()) {
        std::cout << "Audio smoke passed with audio backend disabled." << std::endl;
    } else {
        std::cout << "Audio smoke passed." << std::endl;
    }

    engine.shutdownEngine();
    return success ? 0 : 1;
}

int runInputActionSmoke() {
    auto fake_input = std::make_shared<FakeInputService>();
    NexAur::InputActionSystem input_actions(fake_input);
    input_actions.configureDefaultBindings();

    bool success = true;
    std::string failure;

    auto expect = [&](bool condition, const std::string& message) {
        if (!success) {
            return;
        }
        success = expectInputAction(condition, message, failure);
    };

    input_actions.update();
    expect(input_actions.getAxis2D(NexAur::DefaultInputActions::Move).y == 0.0f, "Move.y should be 0 on frame 0.");
    expect(!input_actions.wasPressed(NexAur::DefaultInputActions::Jump), "Jump should not be pressed on frame 0.");

    fake_input->setKey(NexAur::KeyCode::W, true);
    fake_input->setKey(NexAur::KeyCode::Space, true);
    input_actions.update();
    expect(input_actions.getAxis2D(NexAur::DefaultInputActions::Move).y == 1.0f, "Move.y should be 1 on frame 1.");
    expect(input_actions.isHeld(NexAur::DefaultInputActions::Jump), "Jump should be held on frame 1.");
    expect(input_actions.wasPressed(NexAur::DefaultInputActions::Jump), "Jump should be pressed on frame 1.");
    expect(!input_actions.wasReleased(NexAur::DefaultInputActions::Jump), "Jump should not be released on frame 1.");

    input_actions.update();
    expect(input_actions.getAxis2D(NexAur::DefaultInputActions::Move).y == 1.0f, "Move.y should remain 1 on frame 2.");
    expect(input_actions.isHeld(NexAur::DefaultInputActions::Jump), "Jump should remain held on frame 2.");
    expect(!input_actions.wasPressed(NexAur::DefaultInputActions::Jump), "Jump should not be pressed again on frame 2.");
    expect(!input_actions.wasReleased(NexAur::DefaultInputActions::Jump), "Jump should not be released on frame 2.");

    fake_input->setKey(NexAur::KeyCode::W, false);
    fake_input->setKey(NexAur::KeyCode::Space, false);
    input_actions.update();
    expect(input_actions.getAxis2D(NexAur::DefaultInputActions::Move).y == 0.0f, "Move.y should return to 0 on frame 3.");
    expect(!input_actions.isHeld(NexAur::DefaultInputActions::Jump), "Jump should not be held on frame 3.");
    expect(!input_actions.wasPressed(NexAur::DefaultInputActions::Jump), "Jump should not be pressed on frame 3.");
    expect(input_actions.wasReleased(NexAur::DefaultInputActions::Jump), "Jump should be released on frame 3.");

    fake_input->setKey(NexAur::KeyCode::A, true);
    fake_input->setKey(NexAur::KeyCode::D, true);
    input_actions.update();
    expect(input_actions.getAxis2D(NexAur::DefaultInputActions::Move).x == 0.0f, "Move.x should cancel opposite keys.");

    fake_input->setKey(NexAur::KeyCode::A, false);
    fake_input->setKey(NexAur::KeyCode::D, false);
    fake_input->setMouse(NexAur::MouseCode::ButtonLeft, true);
    input_actions.update();
    expect(input_actions.isHeld(NexAur::DefaultInputActions::Fire), "Fire should be held when left mouse is down.");
    expect(input_actions.wasPressed(NexAur::DefaultInputActions::Fire), "Fire should be pressed when left mouse goes down.");

    if (!success) {
        std::cerr << "InputAction smoke failed: " << failure << std::endl;
        return 1;
    }

    std::cout << "InputAction smoke passed." << std::endl;
    return 0;
}

int runRenderSettingsSmoke() {
    NexAur::RenderContext render_context;

    bool success = true;
    std::string failure;

    auto expect = [&](bool condition, const std::string& message) {
        if (!success) {
            return;
        }
        success = expectGameplay(condition, message, failure);
    };

    NexAur::RenderSettings settings;
    settings.lighting.preset = NexAur::RenderLightingPreset::Custom;
    settings.lighting.directional_light_intensity_scale = 0.25f;
    settings.lighting.point_light_intensity_scale = 1.5f;
    settings.lighting.rect_light_intensity_scale = 1.25f;
    settings.lighting.skybox_intensity_scale = 0.5f;
    settings.lighting.ibl_intensity_scale = 0.75f;
    settings.post_process.tone_mapping_mode = NexAur::RenderToneMappingMode::None;
    settings.post_process.exposure = 1.75f;
    settings.post_process.bloom_enabled = false;
    settings.post_process.bloom_intensity = 0.25f;
    settings.post_process.bloom_scatter = 0.5f;
    settings.post_process.bloom_radius = 1.25f;
    settings.post_process.color_grading_enabled = false;
    settings.post_process.color_grading_exposure_offset = 0.35f;
    settings.post_process.color_grading_contrast = 1.2f;
    settings.post_process.color_grading_saturation = 0.8f;
    settings.post_process.color_grading_temperature = 0.25f;
    settings.post_process.color_grading_tint = -0.15f;
    settings.post_process.color_grading_black_point = 0.02f;
    settings.post_process.color_grading_white_point = 1.1f;
    settings.post_process.vignette_intensity = 0.3f;
    settings.post_process.vignette_radius = 0.65f;
    settings.post_process.vignette_softness = 0.4f;
    settings.post_process.sharpen_intensity = 0.2f;
    settings.anti_aliasing.mode = NexAur::RenderAntiAliasingMode::None;
    settings.anti_aliasing.smaa_edge_threshold = 0.18f;
    settings.anti_aliasing.smaa_contrast_factor = 3.0f;
    settings.anti_aliasing.smaa_max_search_steps = 6u;
    settings.anti_aliasing.smaa_blend_strength = 0.55f;
    settings.ao.enabled = true;
    settings.ao.radius = 1.4f;
    settings.ao.intensity = 0.7f;
    settings.ao.bias = 0.03f;
    settings.ao.power = 1.35f;
    settings.ao.blur_enabled = false;
    settings.ao.half_resolution = false;
    settings.ibl_debug.mode = NexAur::RenderIblDebugMode::SpecularIbl;
    settings.ibl_debug.prefilter_mip = 3.0f;
    settings.effects_debug.view = NexAur::RenderEffectDebugView::SmaaEdgeMask;
    settings.effects_debug.bloom_mip = 3u;
    settings.effects_debug.shadow_cascade = 2u;
    settings.effects_debug.point_shadow_layer = 5u;
    settings.effects_debug.rect_shadow_layer = 2u;
    settings.shadow.enabled = false;
    settings.shadow.filter_mode = NexAur::RenderShadowFilterMode::PCSS;
    settings.shadow.strength = 0.45f;
    settings.shadow.constant_bias = 0.006f;
    settings.shadow.normal_bias = 0.05f;
    settings.shadow.slope_bias = 0.003f;
    settings.shadow.filter_radius = 2.0f;
    settings.shadow.pcss_light_radius = 1.25f;
    settings.shadow.pcss_search_radius = 5.5f;
    settings.shadow.pcss_min_filter_radius = 0.5f;
    settings.shadow.pcss_max_filter_radius = 8.0f;
    settings.shadow.distance = 64.0f;
    settings.shadow.map_resolution = 4096u;
    settings.shadow.stabilize = false;
    settings.shadow.cascades_enabled = true;
    settings.shadow.cascade_count = 4u;
    settings.shadow.cascade_split_lambda = 0.82f;
    settings.shadow.cascade_debug_overlay = true;
    settings.point_shadow.enabled = true;
    settings.point_shadow.max_shadowed_lights = 2u;
    settings.point_shadow.map_resolution = 1024u;
    settings.point_shadow.strength = 0.62f;
    settings.point_shadow.constant_bias = 0.012f;
    settings.point_shadow.normal_bias = 0.025f;
    settings.point_shadow.filter_radius = 1.5f;
    settings.contact_shadow.enabled = true;
    settings.contact_shadow.intensity = 0.4f;
    settings.contact_shadow.max_distance = 0.55f;
    settings.contact_shadow.thickness = 0.07f;
    settings.rect_shadow.enabled = false;
    settings.rect_shadow.max_shadowed_lights = 3u;
    settings.rect_shadow.map_resolution = 2048u;
    settings.rect_shadow.strength = 0.58f;
    settings.rect_shadow.constant_bias = 0.018f;
    settings.rect_shadow.normal_bias = 0.035f;
    settings.rect_shadow.filter_radius = 2.25f;
    settings.rect_shadow.projection_margin = 0.55f;
    settings.rect_shadow.soft_shadow_enabled = false;
    settings.rect_shadow.pcss_light_radius = 1.35f;
    settings.rect_shadow.pcss_search_radius = 6.25f;
    settings.rect_shadow.pcss_min_filter_radius = 0.75f;
    settings.rect_shadow.pcss_max_filter_radius = 9.5f;
    settings.rect_shadow.pcss_blocker_taps = 6u;
    settings.rect_shadow.pcss_filter_taps = 12u;
    settings.rect_light.enabled = false;
    settings.rect_light.max_lights = 7u;
    settings.rect_light.ltc_specular_enabled = false;
    settings.rect_light.specular_intensity_scale = 1.75f;
    settings.rect_light.debug_ltc_only = true;
    render_context.setRenderSettings(settings);
    render_context.getWriteData().render_settings = render_context.getRenderSettings();
    render_context.swapBuffers();

    const NexAur::RenderPostProcessSettings& first_post_process =
        render_context.getReadData().render_settings.post_process;
    const NexAur::RenderAntiAliasingSettings& first_anti_aliasing =
        render_context.getReadData().render_settings.anti_aliasing;
    const NexAur::RenderLightingCalibrationSettings& first_lighting =
        render_context.getReadData().render_settings.lighting;
    const NexAur::RenderAoSettings& first_ao =
        render_context.getReadData().render_settings.ao;
    const NexAur::RenderIblDebugSettings& first_ibl_debug =
        render_context.getReadData().render_settings.ibl_debug;
    const NexAur::RenderEffectDebugSettings& first_effects_debug =
        render_context.getReadData().render_settings.effects_debug;
    const NexAur::RenderShadowSettings& first_shadow =
        render_context.getReadData().render_settings.shadow;
    const NexAur::RenderPointShadowSettings& first_point_shadow =
        render_context.getReadData().render_settings.point_shadow;
    const NexAur::RenderContactShadowSettings& first_contact_shadow =
        render_context.getReadData().render_settings.contact_shadow;
    const NexAur::RenderRectShadowSettings& first_rect_shadow =
        render_context.getReadData().render_settings.rect_shadow;
    const NexAur::RenderRectLightSettings& first_rect_light =
        render_context.getReadData().render_settings.rect_light;
    expect(
        first_post_process.tone_mapping_mode == NexAur::RenderToneMappingMode::None,
        "RenderSettings smoke failed: tone mapping mode did not reach the read packet.");
    expect(
        nearlyEqual(first_post_process.exposure, 1.75f),
        "RenderSettings smoke failed: exposure did not reach the read packet.");
    expect(!first_post_process.bloom_enabled, "RenderSettings smoke failed: bloom enabled did not reach the read packet.");
    expect(
        nearlyEqual(first_post_process.bloom_intensity, 0.25f) &&
        nearlyEqual(first_post_process.bloom_scatter, 0.5f) &&
        nearlyEqual(first_post_process.bloom_radius, 1.25f),
        "RenderSettings smoke failed: bloom parameters did not reach the read packet.");
    expect(
        !first_post_process.color_grading_enabled &&
        nearlyEqual(first_post_process.color_grading_exposure_offset, 0.35f) &&
        nearlyEqual(first_post_process.color_grading_contrast, 1.2f) &&
        nearlyEqual(first_post_process.color_grading_saturation, 0.8f) &&
        nearlyEqual(first_post_process.color_grading_temperature, 0.25f) &&
        nearlyEqual(first_post_process.color_grading_tint, -0.15f) &&
        nearlyEqual(first_post_process.color_grading_black_point, 0.02f) &&
        nearlyEqual(first_post_process.color_grading_white_point, 1.1f) &&
        nearlyEqual(first_post_process.vignette_intensity, 0.3f) &&
        nearlyEqual(first_post_process.vignette_radius, 0.65f) &&
        nearlyEqual(first_post_process.vignette_softness, 0.4f) &&
        nearlyEqual(first_post_process.sharpen_intensity, 0.2f),
        "RenderSettings smoke failed: color grading parameters did not reach the read packet.");
    expect(
        first_anti_aliasing.mode == NexAur::RenderAntiAliasingMode::None &&
        nearlyEqual(first_anti_aliasing.smaa_edge_threshold, 0.18f) &&
        nearlyEqual(first_anti_aliasing.smaa_contrast_factor, 3.0f) &&
        first_anti_aliasing.smaa_max_search_steps == 6u &&
        nearlyEqual(first_anti_aliasing.smaa_blend_strength, 0.55f),
        "RenderSettings smoke failed: anti-aliasing parameters did not reach the read packet.");
    expect(
        first_lighting.preset == NexAur::RenderLightingPreset::Custom &&
        nearlyEqual(first_lighting.directional_light_intensity_scale, 0.25f) &&
        nearlyEqual(first_lighting.point_light_intensity_scale, 1.5f) &&
        nearlyEqual(first_lighting.rect_light_intensity_scale, 1.25f) &&
        nearlyEqual(first_lighting.skybox_intensity_scale, 0.5f) &&
        nearlyEqual(first_lighting.ibl_intensity_scale, 0.75f),
        "RenderSettings smoke failed: lighting calibration settings did not reach the read packet.");
    expect(
        first_ao.enabled &&
        nearlyEqual(first_ao.radius, 1.4f) &&
        nearlyEqual(first_ao.intensity, 0.7f) &&
        nearlyEqual(first_ao.bias, 0.03f) &&
        nearlyEqual(first_ao.power, 1.35f) &&
        !first_ao.blur_enabled &&
        !first_ao.half_resolution,
        "RenderSettings smoke failed: AO settings did not reach the read packet.");
    expect(
        first_ibl_debug.mode == NexAur::RenderIblDebugMode::SpecularIbl &&
        nearlyEqual(first_ibl_debug.prefilter_mip, 3.0f),
        "RenderSettings smoke failed: IBL debug settings did not reach the read packet.");
    expect(
        first_effects_debug.view == NexAur::RenderEffectDebugView::SmaaEdgeMask &&
        first_effects_debug.bloom_mip == 3u &&
        first_effects_debug.shadow_cascade == 2u &&
        first_effects_debug.point_shadow_layer == 5u &&
        first_effects_debug.rect_shadow_layer == 2u,
        "RenderSettings smoke failed: effects debug settings did not reach the read packet.");
    expect(
        !first_shadow.enabled &&
        first_shadow.filter_mode == NexAur::RenderShadowFilterMode::PCSS &&
        nearlyEqual(first_shadow.strength, 0.45f) &&
        nearlyEqual(first_shadow.constant_bias, 0.006f) &&
        nearlyEqual(first_shadow.normal_bias, 0.05f) &&
        nearlyEqual(first_shadow.slope_bias, 0.003f) &&
        nearlyEqual(first_shadow.filter_radius, 2.0f) &&
        nearlyEqual(first_shadow.pcss_light_radius, 1.25f) &&
        nearlyEqual(first_shadow.pcss_search_radius, 5.5f) &&
        nearlyEqual(first_shadow.pcss_min_filter_radius, 0.5f) &&
        nearlyEqual(first_shadow.pcss_max_filter_radius, 8.0f) &&
        nearlyEqual(first_shadow.distance, 64.0f) &&
        first_shadow.map_resolution == 4096u &&
        !first_shadow.stabilize &&
        first_shadow.cascades_enabled &&
        first_shadow.cascade_count == 4u &&
        nearlyEqual(first_shadow.cascade_split_lambda, 0.82f) &&
        first_shadow.cascade_debug_overlay,
        "RenderSettings smoke failed: shadow settings did not reach the read packet.");
    expect(
        first_point_shadow.enabled &&
        first_point_shadow.max_shadowed_lights == 2u &&
        first_point_shadow.map_resolution == 1024u &&
        nearlyEqual(first_point_shadow.strength, 0.62f) &&
        nearlyEqual(first_point_shadow.constant_bias, 0.012f) &&
        nearlyEqual(first_point_shadow.normal_bias, 0.025f) &&
        nearlyEqual(first_point_shadow.filter_radius, 1.5f),
        "RenderSettings smoke failed: point shadow settings did not reach the read packet.");
    expect(
        first_contact_shadow.enabled &&
        nearlyEqual(first_contact_shadow.intensity, 0.4f) &&
        nearlyEqual(first_contact_shadow.max_distance, 0.55f) &&
        nearlyEqual(first_contact_shadow.thickness, 0.07f),
        "RenderSettings smoke failed: contact shadow settings did not reach the read packet.");
    expect(
        !first_rect_shadow.enabled &&
        first_rect_shadow.max_shadowed_lights == 3u &&
        first_rect_shadow.map_resolution == 2048u &&
        nearlyEqual(first_rect_shadow.strength, 0.58f) &&
        nearlyEqual(first_rect_shadow.constant_bias, 0.018f) &&
        nearlyEqual(first_rect_shadow.normal_bias, 0.035f) &&
        nearlyEqual(first_rect_shadow.filter_radius, 2.25f) &&
        nearlyEqual(first_rect_shadow.projection_margin, 0.55f) &&
        !first_rect_shadow.soft_shadow_enabled &&
        nearlyEqual(first_rect_shadow.pcss_light_radius, 1.35f) &&
        nearlyEqual(first_rect_shadow.pcss_search_radius, 6.25f) &&
        nearlyEqual(first_rect_shadow.pcss_min_filter_radius, 0.75f) &&
        nearlyEqual(first_rect_shadow.pcss_max_filter_radius, 9.5f) &&
        first_rect_shadow.pcss_blocker_taps == 6u &&
        first_rect_shadow.pcss_filter_taps == 12u,
        "RenderSettings smoke failed: rect shadow settings did not reach the read packet.");
    expect(
        !first_rect_light.enabled &&
        first_rect_light.max_lights == 7u &&
        !first_rect_light.ltc_specular_enabled &&
        nearlyEqual(first_rect_light.specular_intensity_scale, 1.75f) &&
        first_rect_light.debug_ltc_only,
        "RenderSettings smoke failed: rect light settings did not reach the read packet.");

    NexAur::applyRenderLightingPreset(settings, NexAur::RenderLightingPreset::Cornell);
    settings.ao.enabled = false;
    settings.ao.radius = 0.8f;
    settings.ao.intensity = 0.25f;
    settings.ao.bias = 0.01f;
    settings.ao.power = 2.0f;
    settings.ao.blur_enabled = true;
    settings.ao.half_resolution = true;
    settings.ibl_debug.mode = NexAur::RenderIblDebugMode::FinalLit;
    settings.ibl_debug.prefilter_mip = 0.0f;
    settings.post_process.color_grading_enabled = true;
    settings.post_process.color_grading_exposure_offset = 0.0f;
    settings.post_process.color_grading_contrast = 1.0f;
    settings.post_process.color_grading_saturation = 1.0f;
    settings.post_process.color_grading_temperature = 0.0f;
    settings.post_process.color_grading_tint = 0.0f;
    settings.post_process.color_grading_black_point = 0.0f;
    settings.post_process.color_grading_white_point = 1.0f;
    settings.post_process.vignette_intensity = 0.0f;
    settings.post_process.vignette_radius = 0.75f;
    settings.post_process.vignette_softness = 0.35f;
    settings.post_process.sharpen_intensity = 0.0f;
    settings.anti_aliasing.mode = NexAur::RenderAntiAliasingMode::SMAA;
    settings.anti_aliasing.smaa_edge_threshold = 0.08f;
    settings.anti_aliasing.smaa_contrast_factor = 2.0f;
    settings.anti_aliasing.smaa_max_search_steps = 12u;
    settings.anti_aliasing.smaa_blend_strength = 0.85f;
    settings.effects_debug.view = NexAur::RenderEffectDebugView::SmaaOutput;
    settings.effects_debug.bloom_mip = 0u;
    settings.effects_debug.shadow_cascade = 1u;
    settings.effects_debug.point_shadow_layer = 2u;
    settings.effects_debug.rect_shadow_layer = 0u;
    settings.shadow.enabled = true;
    settings.shadow.filter_mode = NexAur::RenderShadowFilterMode::PoissonPCF;
    settings.shadow.strength = 0.7f;
    settings.shadow.constant_bias = 0.003f;
    settings.shadow.normal_bias = 0.0f;
    settings.shadow.slope_bias = 0.001f;
    settings.shadow.filter_radius = 1.0f;
    settings.shadow.pcss_light_radius = 0.5f;
    settings.shadow.pcss_search_radius = 3.0f;
    settings.shadow.pcss_min_filter_radius = 0.75f;
    settings.shadow.pcss_max_filter_radius = 6.0f;
    settings.shadow.distance = 35.0f;
    settings.shadow.map_resolution = 2048u;
    settings.shadow.stabilize = true;
    settings.shadow.cascades_enabled = false;
    settings.shadow.cascade_count = 1u;
    settings.shadow.cascade_split_lambda = 0.65f;
    settings.shadow.cascade_debug_overlay = false;
    settings.point_shadow.enabled = true;
    settings.point_shadow.max_shadowed_lights = 1u;
    settings.point_shadow.map_resolution = 512u;
    settings.point_shadow.strength = 0.85f;
    settings.point_shadow.constant_bias = 0.015f;
    settings.point_shadow.normal_bias = 0.02f;
    settings.point_shadow.filter_radius = 1.0f;
    settings.contact_shadow.enabled = false;
    settings.contact_shadow.intensity = 0.35f;
    settings.contact_shadow.max_distance = 0.45f;
    settings.contact_shadow.thickness = 0.08f;
    settings.rect_shadow.enabled = true;
    settings.rect_shadow.max_shadowed_lights = 1u;
    settings.rect_shadow.map_resolution = 1024u;
    settings.rect_shadow.strength = 0.85f;
    settings.rect_shadow.constant_bias = 0.01f;
    settings.rect_shadow.normal_bias = 0.02f;
    settings.rect_shadow.filter_radius = 1.0f;
    settings.rect_shadow.projection_margin = 0.35f;
    settings.rect_shadow.soft_shadow_enabled = true;
    settings.rect_shadow.pcss_light_radius = 0.75f;
    settings.rect_shadow.pcss_search_radius = 3.0f;
    settings.rect_shadow.pcss_min_filter_radius = 0.5f;
    settings.rect_shadow.pcss_max_filter_radius = 5.0f;
    settings.rect_shadow.pcss_blocker_taps = 8u;
    settings.rect_shadow.pcss_filter_taps = 16u;
    settings.rect_light.enabled = true;
    settings.rect_light.max_lights = 1u;
    settings.rect_light.ltc_specular_enabled = true;
    settings.rect_light.specular_intensity_scale = 1.0f;
    settings.rect_light.debug_ltc_only = false;
    render_context.setRenderSettings(settings);
    render_context.getWriteData().render_settings = render_context.getRenderSettings();
    render_context.swapBuffers();

    const NexAur::RenderPostProcessSettings& second_post_process =
        render_context.getReadData().render_settings.post_process;
    const NexAur::RenderAntiAliasingSettings& second_anti_aliasing =
        render_context.getReadData().render_settings.anti_aliasing;
    const NexAur::RenderLightingCalibrationSettings& second_lighting =
        render_context.getReadData().render_settings.lighting;
    const NexAur::RenderAoSettings& second_ao =
        render_context.getReadData().render_settings.ao;
    const NexAur::RenderIblDebugSettings& second_ibl_debug =
        render_context.getReadData().render_settings.ibl_debug;
    const NexAur::RenderEffectDebugSettings& second_effects_debug =
        render_context.getReadData().render_settings.effects_debug;
    const NexAur::RenderShadowSettings& second_shadow =
        render_context.getReadData().render_settings.shadow;
    const NexAur::RenderPointShadowSettings& second_point_shadow =
        render_context.getReadData().render_settings.point_shadow;
    const NexAur::RenderContactShadowSettings& second_contact_shadow =
        render_context.getReadData().render_settings.contact_shadow;
    const NexAur::RenderRectShadowSettings& second_rect_shadow =
        render_context.getReadData().render_settings.rect_shadow;
    const NexAur::RenderRectLightSettings& second_rect_light =
        render_context.getReadData().render_settings.rect_light;
    expect(
        second_post_process.tone_mapping_mode == NexAur::RenderToneMappingMode::ACES,
        "RenderSettings smoke failed: ACES mode did not reach the read packet.");
    expect(
        nearlyEqual(second_post_process.exposure, 1.0f),
        "RenderSettings smoke failed: Cornell exposure did not reach the read packet.");
    expect(second_post_process.bloom_enabled, "RenderSettings smoke failed: updated bloom enabled did not reach the read packet.");
    expect(
        nearlyEqual(second_post_process.bloom_intensity, 0.02f) &&
        nearlyEqual(second_post_process.bloom_scatter, 0.7f) &&
        nearlyEqual(second_post_process.bloom_radius, 1.0f),
        "RenderSettings smoke failed: Cornell bloom parameters did not reach the read packet.");
    expect(
        second_post_process.color_grading_enabled &&
        nearlyEqual(second_post_process.color_grading_exposure_offset, 0.0f) &&
        nearlyEqual(second_post_process.color_grading_contrast, 1.0f) &&
        nearlyEqual(second_post_process.color_grading_saturation, 1.0f) &&
        nearlyEqual(second_post_process.color_grading_temperature, 0.0f) &&
        nearlyEqual(second_post_process.color_grading_tint, 0.0f) &&
        nearlyEqual(second_post_process.color_grading_black_point, 0.0f) &&
        nearlyEqual(second_post_process.color_grading_white_point, 1.0f) &&
        nearlyEqual(second_post_process.vignette_intensity, 0.0f) &&
        nearlyEqual(second_post_process.vignette_radius, 0.75f) &&
        nearlyEqual(second_post_process.vignette_softness, 0.35f) &&
        nearlyEqual(second_post_process.sharpen_intensity, 0.0f),
        "RenderSettings smoke failed: updated color grading parameters did not reach the read packet.");
    expect(
        second_anti_aliasing.mode == NexAur::RenderAntiAliasingMode::SMAA &&
        nearlyEqual(second_anti_aliasing.smaa_edge_threshold, 0.08f) &&
        nearlyEqual(second_anti_aliasing.smaa_contrast_factor, 2.0f) &&
        second_anti_aliasing.smaa_max_search_steps == 12u &&
        nearlyEqual(second_anti_aliasing.smaa_blend_strength, 0.85f),
        "RenderSettings smoke failed: updated anti-aliasing parameters did not reach the read packet.");
    expect(
        second_lighting.preset == NexAur::RenderLightingPreset::Cornell &&
        nearlyEqual(second_lighting.directional_light_intensity_scale, 0.0f) &&
        nearlyEqual(second_lighting.point_light_intensity_scale, 1.0f) &&
        nearlyEqual(second_lighting.rect_light_intensity_scale, 1.0f) &&
        nearlyEqual(second_lighting.skybox_intensity_scale, 0.0f) &&
        nearlyEqual(second_lighting.ibl_intensity_scale, 0.03f),
        "RenderSettings smoke failed: Cornell lighting preset did not reach the read packet.");
    expect(
        !second_ao.enabled &&
        nearlyEqual(second_ao.radius, 0.8f) &&
        nearlyEqual(second_ao.intensity, 0.25f) &&
        nearlyEqual(second_ao.bias, 0.01f) &&
        nearlyEqual(second_ao.power, 2.0f) &&
        second_ao.blur_enabled &&
        second_ao.half_resolution,
        "RenderSettings smoke failed: updated AO settings did not reach the read packet.");
    expect(
        second_ibl_debug.mode == NexAur::RenderIblDebugMode::FinalLit &&
        nearlyEqual(second_ibl_debug.prefilter_mip, 0.0f),
        "RenderSettings smoke failed: updated IBL debug settings did not reach the read packet.");
    expect(
        second_effects_debug.view == NexAur::RenderEffectDebugView::SmaaOutput &&
        second_effects_debug.bloom_mip == 0u &&
        second_effects_debug.shadow_cascade == 1u &&
        second_effects_debug.point_shadow_layer == 2u &&
        second_effects_debug.rect_shadow_layer == 0u,
        "RenderSettings smoke failed: updated effects debug settings did not reach the read packet.");
    expect(
        second_shadow.enabled &&
        second_shadow.filter_mode == NexAur::RenderShadowFilterMode::PoissonPCF &&
        nearlyEqual(second_shadow.strength, 0.7f) &&
        nearlyEqual(second_shadow.constant_bias, 0.003f) &&
        nearlyEqual(second_shadow.normal_bias, 0.0f) &&
        nearlyEqual(second_shadow.slope_bias, 0.001f) &&
        nearlyEqual(second_shadow.filter_radius, 1.0f) &&
        nearlyEqual(second_shadow.pcss_light_radius, 0.5f) &&
        nearlyEqual(second_shadow.pcss_search_radius, 3.0f) &&
        nearlyEqual(second_shadow.pcss_min_filter_radius, 0.75f) &&
        nearlyEqual(second_shadow.pcss_max_filter_radius, 6.0f) &&
        nearlyEqual(second_shadow.distance, 35.0f) &&
        second_shadow.map_resolution == 2048u &&
        second_shadow.stabilize &&
        !second_shadow.cascades_enabled &&
        second_shadow.cascade_count == 1u &&
        nearlyEqual(second_shadow.cascade_split_lambda, 0.65f) &&
        !second_shadow.cascade_debug_overlay,
        "RenderSettings smoke failed: updated shadow settings did not reach the read packet.");
    expect(
        second_point_shadow.enabled &&
        second_point_shadow.max_shadowed_lights == 1u &&
        second_point_shadow.map_resolution == 512u &&
        nearlyEqual(second_point_shadow.strength, 0.85f) &&
        nearlyEqual(second_point_shadow.constant_bias, 0.015f) &&
        nearlyEqual(second_point_shadow.normal_bias, 0.02f) &&
        nearlyEqual(second_point_shadow.filter_radius, 1.0f),
        "RenderSettings smoke failed: updated point shadow settings did not reach the read packet.");
    expect(
        !second_contact_shadow.enabled &&
        nearlyEqual(second_contact_shadow.intensity, 0.35f) &&
        nearlyEqual(second_contact_shadow.max_distance, 0.45f) &&
        nearlyEqual(second_contact_shadow.thickness, 0.08f),
        "RenderSettings smoke failed: updated contact shadow settings did not reach the read packet.");
    expect(
        second_rect_shadow.enabled &&
        second_rect_shadow.max_shadowed_lights == 1u &&
        second_rect_shadow.map_resolution == 1024u &&
        nearlyEqual(second_rect_shadow.strength, 0.85f) &&
        nearlyEqual(second_rect_shadow.constant_bias, 0.01f) &&
        nearlyEqual(second_rect_shadow.normal_bias, 0.02f) &&
        nearlyEqual(second_rect_shadow.filter_radius, 1.0f) &&
        nearlyEqual(second_rect_shadow.projection_margin, 0.35f) &&
        second_rect_shadow.soft_shadow_enabled &&
        nearlyEqual(second_rect_shadow.pcss_light_radius, 0.75f) &&
        nearlyEqual(second_rect_shadow.pcss_search_radius, 3.0f) &&
        nearlyEqual(second_rect_shadow.pcss_min_filter_radius, 0.5f) &&
        nearlyEqual(second_rect_shadow.pcss_max_filter_radius, 5.0f) &&
        second_rect_shadow.pcss_blocker_taps == 8u &&
        second_rect_shadow.pcss_filter_taps == 16u,
        "RenderSettings smoke failed: updated rect shadow settings did not reach the read packet.");
    expect(
        second_rect_light.enabled &&
        second_rect_light.max_lights == 1u &&
        second_rect_light.ltc_specular_enabled &&
        nearlyEqual(second_rect_light.specular_intensity_scale, 1.0f) &&
        !second_rect_light.debug_ltc_only,
        "RenderSettings smoke failed: updated rect light settings did not reach the read packet.");

    NexAur::RenderDataPacket render_data;
    render_data.render_settings = settings;
    render_data.directional_light_data.intensity = 2.0f;
    render_data.directional_light_data.cast_shadow = true;
    render_data.environment_data.skybox_intensity = 0.75f;
    render_data.environment_data.ibl_intensity = 0.65f;

    NexAur::RendererPointLightData cornell_light;
    cornell_light.intensity = 12.0f;
    cornell_light.cast_shadow = true;
    cornell_light.shadow_range = 7.0f;
    cornell_light.shadow_strength = 0.9f;
    render_data.point_lights_data.push_back(cornell_light);
    NexAur::RendererPointLightData budget_clipped_light = cornell_light;
    budget_clipped_light.position = glm::vec3{ 1.0f, 0.0f, 0.0f };
    render_data.point_lights_data.push_back(budget_clipped_light);
    NexAur::RendererRectLightData cornell_rect_light;
    cornell_rect_light.position = glm::vec3{ 0.0f, 2.8f, 0.0f };
    cornell_rect_light.size = glm::vec2{ 1.4f, 1.0f };
    cornell_rect_light.intensity = 8.0f;
    cornell_rect_light.range = 6.5f;
    cornell_rect_light.cast_shadow = true;
    cornell_rect_light.shadow_strength = 0.88f;
    render_data.rect_lights_data.push_back(cornell_rect_light);

    NexAur::RenderView render_view;
    render_view.viewport_width = 1280u;
    render_view.viewport_height = 720u;

    NexAur::RenderSceneFrameBuilder frame_builder;
    const NexAur::RenderSceneFrame cornell_frame =
        frame_builder.buildRenderSceneFrame(render_data, render_view);
    expect(
        nearlyEqual(cornell_frame.directional_light.intensity, 0.0f) &&
        !cornell_frame.directional_light.cast_shadow,
        "RenderSettings smoke failed: Cornell preset did not suppress directional light.");
    expect(
        !cornell_frame.point_lights.empty() &&
        nearlyEqual(cornell_frame.point_lights.front().intensity, 12.0f),
        "RenderSettings smoke failed: Cornell preset should preserve local light intensity.");
    expect(
        cornell_frame.point_lights.size() == 2u &&
        cornell_frame.point_lights[0].shadow_requested &&
        cornell_frame.point_lights[0].cast_shadow &&
        cornell_frame.point_lights[0].shadow_slot == 0 &&
        nearlyEqual(cornell_frame.point_lights[0].shadow_range, 7.0f) &&
        nearlyEqual(cornell_frame.point_lights[0].shadow_strength, 0.9f) &&
        cornell_frame.point_lights[1].shadow_requested &&
        !cornell_frame.point_lights[1].cast_shadow &&
        cornell_frame.point_lights[1].shadow_slot == -1,
        "RenderSettings smoke failed: point shadow budget or slot assignment was incorrect.");
    expect(
        cornell_frame.rect_lights.size() == 1u &&
        nearlyEqual(cornell_frame.rect_lights.front().intensity, 8.0f) &&
        nearlyEqual(cornell_frame.rect_lights.front().size.x, 1.4f) &&
        nearlyEqual(cornell_frame.rect_lights.front().size.y, 1.0f) &&
        nearlyEqual(cornell_frame.rect_lights.front().range, 6.5f) &&
        cornell_frame.rect_lights.front().shadow_requested &&
        cornell_frame.rect_lights.front().cast_shadow &&
        cornell_frame.rect_lights.front().shadow_slot == 0 &&
        nearlyEqual(cornell_frame.rect_lights.front().shadow_strength, 0.88f),
        "RenderSettings smoke failed: rect light budget, calibration, or shadow slot was incorrect.");
    expect(
        nearlyEqual(cornell_frame.skybox_intensity, 0.0f) &&
        nearlyEqual(cornell_frame.ibl_intensity, 0.65f * 0.03f),
        "RenderSettings smoke failed: Cornell preset did not calibrate environment intensity.");

    if (!success) {
        std::cerr << failure << std::endl;
        return 1;
    }

    std::cout << "RenderSettings smoke passed." << std::endl;
    return 0;
}

int runMaterialAssetSmoke() {
    NexAur::AssetManager& asset_manager = NexAur::AssetManager::getInstance();

    bool success = true;
    std::string failure;

    auto expect = [&](bool condition, const std::string& message) {
        if (!success) {
            return;
        }
        success = expectGameplay(condition, message, failure);
    };

    auto expectColorSpace = [&](NexAur::AssetHandle handle, NexAur::TextureColorSpace color_space, const std::string& label) {
        const NexAur::AssetMetadata* metadata = asset_manager.getMetadata(handle);
        expect(metadata != nullptr, label + " metadata should exist.");
        if (metadata) {
            expect(metadata->type == NexAur::AssetType::Texture2D, label + " should register as Texture2D.");
            expect(metadata->texture_color_space == color_space, label + " should register with expected color space.");
        }
    };

    NexAur::MaterialImportData separate_material;
    separate_material.name = "MaterialAssetSmoke.Separate";
    separate_material.base_color_texture_path = NX_ASSET("assets/textures/PBR/gold/albedo.png");
    separate_material.normal_texture_path = NX_ASSET("assets/textures/PBR/gold/normal.png");
    separate_material.metallic_texture_path = NX_ASSET("assets/textures/PBR/gold/metallic.png");
    separate_material.roughness_texture_path = NX_ASSET("assets/textures/PBR/gold/roughness.png");
    separate_material.ao_texture_path = NX_ASSET("assets/textures/PBR/gold/ao.png");
    separate_material.emissive_texture_path = NX_ASSET("assets/textures/PBR/gold/albedo.png");
    separate_material.metallic_factor = 0.8f;
    separate_material.roughness_factor = 0.35f;
    separate_material.emissive_factor = glm::vec3{ 1.5f, 1.0f, 0.5f };
    separate_material.normal_scale = 0.75f;
    separate_material.occlusion_strength = 0.6f;

    std::shared_ptr<NexAur::MaterialAsset> separate = asset_manager.createMaterialFromImportData(separate_material);
    expect(separate != nullptr, "MaterialAsset smoke failed: separate material was not created.");
    if (separate) {
        expect(separate->hasBaseColorTexture(), "Separate material should have base color texture.");
        expect(separate->hasNormalTexture(), "Separate material should have normal texture.");
        expect(separate->hasMetallicTexture(), "Separate material should have metallic texture.");
        expect(separate->hasRoughnessTexture(), "Separate material should have roughness texture.");
        expect(separate->hasAOTexture(), "Separate material should have AO texture.");
        expect(separate->hasEmissiveTexture(), "Separate material should have emissive texture.");
        expect(!separate->usesPackedMetallicRoughness(), "Separate material should not use packed metallic-roughness.");
        expect(nearlyEqual(separate->getMetallicFactor(), 0.8f), "Separate material metallic factor should round-trip.");
        expect(nearlyEqual(separate->getRoughnessFactor(), 0.35f), "Separate material roughness factor should round-trip.");
        expect(nearlyEqualVec3(separate->getEmissiveFactor(), glm::vec3{ 1.5f, 1.0f, 0.5f }), "Separate material emissive factor should round-trip.");
        expect(nearlyEqual(separate->getNormalScale(), 0.75f), "Separate material normal scale should round-trip.");
        expect(nearlyEqual(separate->getOcclusionStrength(), 0.6f), "Separate material AO strength should round-trip.");

        expectColorSpace(separate->getBaseColorTexture(), NexAur::TextureColorSpace::SRGB, "Base color texture");
        expectColorSpace(separate->getNormalTexture(), NexAur::TextureColorSpace::Linear, "Normal texture");
        expectColorSpace(separate->getMetallicTexture(), NexAur::TextureColorSpace::Linear, "Metallic texture");
        expectColorSpace(separate->getRoughnessTexture(), NexAur::TextureColorSpace::Linear, "Roughness texture");
        expectColorSpace(separate->getAOTexture(), NexAur::TextureColorSpace::Linear, "AO texture");
        expectColorSpace(separate->getEmissiveTexture(), NexAur::TextureColorSpace::SRGB, "Emissive texture");
    }

    NexAur::MaterialImportData packed_material;
    packed_material.name = "MaterialAssetSmoke.Packed";
    packed_material.metallic_roughness_texture_path = NX_ASSET("assets/textures/PBR/rusted_iron/roughness.png");
    packed_material.metallic_roughness_mode = NexAur::MaterialMetallicRoughnessTextureMode::PackedGltf;

    std::shared_ptr<NexAur::MaterialAsset> packed = asset_manager.createMaterialFromImportData(packed_material);
    expect(packed != nullptr, "MaterialAsset smoke failed: packed material was not created.");
    if (packed) {
        expect(packed->hasMetallicRoughnessTexture(), "Packed material should have metallic-roughness texture.");
        expect(packed->usesPackedMetallicRoughness(), "Packed material should use packed metallic-roughness.");
        expectColorSpace(packed->getMetallicRoughnessTexture(), NexAur::TextureColorSpace::Linear, "Packed metallic-roughness texture");
    }

    if (!success) {
        std::cerr << failure << std::endl;
        return 1;
    }

    std::cout << "MaterialAsset smoke passed." << std::endl;
    return 0;
}

int runGltfModelImportSmoke() {
    const std::filesystem::path damaged_helmet_path =
        std::filesystem::path(NX_ASSET("assets/models/DamagedHelmet/DamagedHelmet.gltf"));

    if (!std::filesystem::exists(damaged_helmet_path)) {
        std::cout << "GltfModelImport smoke skipped: optional DamagedHelmet asset is missing." << std::endl;
        return 0;
    }

    bool success = true;
    std::string failure;

    auto expect = [&](bool condition, const std::string& message) {
        if (!success) {
            return;
        }
        success = expectGameplay(condition, message, failure);
    };

    auto expectFilename = [&](const std::string& path, const std::string& filename, const std::string& label) {
        expect(!path.empty(), label + " path should not be empty.");
        if (!path.empty()) {
            expect(
                std::filesystem::path(path).filename().string() == filename,
                label + " should resolve to " + filename + ".");
        }
    };

    NexAur::Engine engine;
    engine.startEngine();

    NexAur::AssetManager& asset_manager = NexAur::AssetManager::getInstance();
    NexAur::AssetHandle model_handle = asset_manager.importModelAsset(damaged_helmet_path.string());
    std::shared_ptr<NexAur::Model> model = asset_manager.loadModelCPU(model_handle);
    expect(model != nullptr && model->isLoaded(), "DamagedHelmet glTF should load.");
    expect(model && model->getMeshes().size() == 1, "DamagedHelmet should import one mesh.");

    if (model && model->isLoaded() && !model->getMeshes().empty()) {
        const NexAur::Mesh& mesh = model->getMeshes().front();
        const NexAur::MaterialImportData& material = mesh.getMaterialImportData();

        expectFilename(material.base_color_texture_path, "Default_albedo.jpg", "Base color texture");
        expectFilename(material.normal_texture_path, "Default_normal.jpg", "Normal texture");
        expectFilename(material.metallic_roughness_texture_path, "Default_metalRoughness.jpg", "Metallic-roughness texture");
        expectFilename(material.ao_texture_path, "Default_AO.jpg", "AO texture");
        expectFilename(material.emissive_texture_path, "Default_emissive.jpg", "Emissive texture");
        expect(material.metallic_texture_path.empty(), "DamagedHelmet should not keep a separate metallic texture.");
        expect(material.roughness_texture_path.empty(), "DamagedHelmet should not keep a separate roughness texture.");
        expect(
            material.metallic_roughness_mode == NexAur::MaterialMetallicRoughnessTextureMode::PackedGltf,
            "DamagedHelmet should use packed glTF metallic-roughness.");
        expect(nearlyEqual(material.metallic_factor, 1.0f), "DamagedHelmet metallic factor should follow glTF default.");
        expect(nearlyEqual(material.roughness_factor, 1.0f), "DamagedHelmet roughness factor should follow glTF default.");
        expect(nearlyEqual(material.normal_scale, 1.0f), "DamagedHelmet normal scale should follow glTF default.");
        expect(nearlyEqual(material.occlusion_strength, 1.0f), "DamagedHelmet AO strength should follow glTF default.");
        expect(material.alpha_mode == NexAur::MaterialAlphaMode::Opaque, "DamagedHelmet alpha mode should be opaque.");

        glm::vec3 min_bounds{ std::numeric_limits<float>::max() };
        glm::vec3 max_bounds{ std::numeric_limits<float>::lowest() };
        for (const NexAur::Vertex& vertex : mesh.GetVertices()) {
            min_bounds = glm::min(min_bounds, vertex.position);
            max_bounds = glm::max(max_bounds, vertex.position);
        }

        expect(
            min_bounds.y > -1.05f && max_bounds.y < 1.05f &&
            min_bounds.z < -1.05f && max_bounds.z > 0.75f,
            "DamagedHelmet node transform should be baked into imported vertices.");

        std::shared_ptr<NexAur::MaterialAsset> material_asset =
            asset_manager.createMaterialFromImportData(material);
        expect(material_asset != nullptr, "DamagedHelmet material asset should be created.");
        if (material_asset) {
            expect(material_asset->usesPackedMetallicRoughness(), "DamagedHelmet material asset should use packed MR.");

            const NexAur::AssetMetadata* base_color_metadata =
                asset_manager.getMetadata(material_asset->getBaseColorTexture());
            const NexAur::AssetMetadata* mr_metadata =
                asset_manager.getMetadata(material_asset->getMetallicRoughnessTexture());
            expect(
                base_color_metadata && base_color_metadata->texture_color_space == NexAur::TextureColorSpace::SRGB,
                "DamagedHelmet base color should be registered as sRGB.");
            expect(
                mr_metadata && mr_metadata->texture_color_space == NexAur::TextureColorSpace::Linear,
                "DamagedHelmet metallic-roughness should be registered as linear.");
        }
    }

    if (!success) {
        std::cerr << failure << std::endl;
        engine.shutdownEngine();
        return 1;
    }

    std::cout << "GltfModelImport smoke passed." << std::endl;
    engine.shutdownEngine();
    return 0;
}

int runAssimpFallbackSmoke() {
    const std::filesystem::path obj_path =
        std::filesystem::path(ENGINE_ROOT_DIR) / "build" / "assimp_fallback_smoke.obj";

    bool success = true;
    std::string failure;

    auto expect = [&](bool condition, const std::string& message) {
        if (!success) {
            return;
        }
        success = expectGameplay(condition, message, failure);
    };

    expect(writeAssimpFallbackSmokeObj(obj_path), "AssimpFallback smoke failed: could not write test OBJ.");

    NexAur::Engine engine;
    engine.startEngine();

    NexAur::AssetManager& asset_manager = NexAur::AssetManager::getInstance();
    const NexAur::ModelImporterRegistry& registry = asset_manager.getModelImporterRegistry();
    const NexAur::IModelImporter* gltf_importer = registry.findImporter("default_path_probe.gltf");
    const NexAur::IModelImporter* obj_importer = registry.findImporter(obj_path);
    expect(gltf_importer != nullptr, "AssimpFallback smoke failed: .gltf should still have an importer.");
    expect(obj_importer != nullptr, "AssimpFallback smoke failed: .obj should have an Assimp fallback importer.");
    if (gltf_importer && obj_importer) {
        expect(gltf_importer != obj_importer, "AssimpFallback smoke failed: .gltf and .obj should not use the same importer.");
    }

    const NexAur::ModelImportResult metadata_result =
        asset_manager.inspectModelImportMetadata(obj_path.string());
    expect(metadata_result.success, "AssimpFallback smoke failed: OBJ metadata import should succeed.");
    if (metadata_result.success) {
        expect(metadata_result.metadata.generator == "Assimp", "AssimpFallback metadata should identify Assimp.");
        expect(metadata_result.metadata.mesh_count == 1, "AssimpFallback metadata should report one mesh.");
        expect(metadata_result.metadata.vertex_count == 3, "AssimpFallback metadata should report three vertices.");
        expect(metadata_result.metadata.index_count == 3, "AssimpFallback metadata should report three indices.");
    }

    const NexAur::AssetHandle model_handle = asset_manager.importModelAsset(obj_path.string());
    expect(model_handle.isValid(), "AssimpFallback smoke failed: OBJ should import through default AssetManager path.");
    std::shared_ptr<NexAur::Model> model = asset_manager.loadModelCPU(model_handle);
    expect(model != nullptr && model->isLoaded(), "AssimpFallback smoke failed: OBJ CPU model should load.");
    expect(model && model->getMeshes().size() == 1, "AssimpFallback smoke failed: OBJ should produce one mesh.");
    if (model && !model->getMeshes().empty()) {
        const NexAur::Mesh& mesh = model->getMeshes().front();
        expect(mesh.GetVertices().size() == 3, "AssimpFallback smoke failed: OBJ vertex count should be 3.");
        expect(mesh.GetIndices().size() == 3, "AssimpFallback smoke failed: OBJ index count should be 3.");
        if (!mesh.GetVertices().empty()) {
            expect(
                nearlyEqual(mesh.GetVertices().front().normal.z, 1.0f),
                "AssimpFallback smoke failed: OBJ normal should import.");
        }
    }

    std::error_code remove_error;
    std::filesystem::remove(obj_path, remove_error);

    if (!success) {
        std::cerr << failure << std::endl;
        engine.shutdownEngine();
        return 1;
    }

    std::cout << "AssimpFallback smoke passed." << std::endl;
    engine.shutdownEngine();
    return 0;
}

int runTinyGltfMetadataSmoke() {
    const std::filesystem::path damaged_helmet_path =
        std::filesystem::path(NX_ASSET("assets/models/DamagedHelmet/DamagedHelmet.gltf"));

    if (!std::filesystem::exists(damaged_helmet_path)) {
        std::cout << "TinyGltfMetadata smoke skipped: optional DamagedHelmet asset is missing." << std::endl;
        return 0;
    }

    bool success = true;
    std::string failure;

    auto expect = [&](bool condition, const std::string& message) {
        if (!success) {
            return;
        }
        success = expectGameplay(condition, message, failure);
    };

    auto joinMessages = [](const std::vector<std::string>& messages) {
        std::string joined;
        for (const std::string& message : messages) {
            if (!joined.empty()) {
                joined += " ";
            }
            joined += message;
        }
        return joined;
    };

    NexAur::Engine engine;
    engine.startEngine();

    NexAur::AssetManager& asset_manager = NexAur::AssetManager::getInstance();
    const NexAur::ModelImporterRegistry& registry = asset_manager.getModelImporterRegistry();
    expect(registry.getImporterCount() > 0, "TinyGltfMetadata smoke failed: no importers are registered.");
    expect(
        registry.findImporter(damaged_helmet_path) != nullptr,
        "TinyGltfMetadata smoke failed: .gltf was not recognized by the importer registry.");

    const NexAur::ModelImportResult result =
        asset_manager.inspectModelImportMetadata(damaged_helmet_path.string());
    expect(result.success, "TinyGltfMetadata smoke failed: " + joinMessages(result.errors));
    expect(result.errors.empty(), "TinyGltfMetadata smoke should not produce parse errors.");

    if (result.success) {
        const NexAur::ModelImportMetadata& metadata = result.metadata;
        expect(metadata.asset_version == "2.0", "DamagedHelmet should report glTF asset version 2.0.");
        expect(metadata.scene_count >= 1, "DamagedHelmet should contain at least one scene.");
        expect(metadata.node_count >= 1, "DamagedHelmet should contain nodes.");
        expect(metadata.mesh_count == 1, "DamagedHelmet metadata should report one mesh.");
        expect(metadata.primitive_count == 1, "DamagedHelmet metadata should report one primitive.");
        expect(metadata.material_count == 1, "DamagedHelmet metadata should report one material.");
        expect(metadata.texture_count >= 5, "DamagedHelmet metadata should report its PBR textures.");
        expect(metadata.image_count >= 5, "DamagedHelmet metadata should report its PBR images.");
    }

    if (!success) {
        std::cerr << failure << std::endl;
        engine.shutdownEngine();
        return 1;
    }

    std::cout << "TinyGltfMetadata smoke passed." << std::endl;
    engine.shutdownEngine();
    return 0;
}

int runTinyGltfGeometrySmoke() {
    const std::filesystem::path glb_path =
        std::filesystem::path(ENGINE_ROOT_DIR) / "build" / "tiny_gltf_geometry_smoke.glb";

    bool success = true;
    std::string failure;

    auto expect = [&](bool condition, const std::string& message) {
        if (!success) {
            return;
        }
        success = expectGameplay(condition, message, failure);
    };

    expect(writeTinyGltfSmokeCubeGlb(glb_path), "TinyGltfGeometry smoke failed: could not write test GLB.");

    NexAur::Engine engine;
    engine.startEngine();

    NexAur::AssetManager& asset_manager = NexAur::AssetManager::getInstance();
    NexAur::AssetHandle glb_handle = asset_manager.importModelAssetFromRegistry(glb_path.string());
    std::shared_ptr<NexAur::Model> glb_model = asset_manager.loadModelCPU(glb_handle);
    expect(glb_model != nullptr && glb_model->isLoaded(), "TinyGltfGeometry smoke failed: GLB model did not load.");
    expect(glb_model && glb_model->getMeshes().size() == 1, "TinyGltfGeometry smoke failed: GLB should produce one mesh.");

    if (glb_model && !glb_model->getMeshes().empty()) {
        const NexAur::Mesh& mesh = glb_model->getMeshes().front();
        expect(mesh.GetVertices().size() == 24, "TinyGltfGeometry smoke failed: GLB cube vertex count should be 24.");
        expect(mesh.GetIndices().size() == 36, "TinyGltfGeometry smoke failed: GLB cube index count should be 36.");

        bool has_expected_position = false;
        bool has_expected_normal = false;
        bool has_expected_uv = false;
        for (const NexAur::Vertex& vertex : mesh.GetVertices()) {
            has_expected_position = has_expected_position ||
                (nearlyEqual(vertex.position.x, -0.5f) &&
                 nearlyEqual(vertex.position.y, -0.5f) &&
                 nearlyEqual(vertex.position.z, -0.5f));
            has_expected_normal = has_expected_normal ||
                (nearlyEqual(vertex.normal.x, 0.0f) &&
                 nearlyEqual(vertex.normal.y, 0.0f) &&
                 nearlyEqual(vertex.normal.z, -1.0f));
            has_expected_uv = has_expected_uv ||
                (nearlyEqual(vertex.texCoords.x, 1.0f) &&
                 nearlyEqual(vertex.texCoords.y, 1.0f));
        }
        expect(has_expected_position, "TinyGltfGeometry smoke failed: POSITION accessor data was not imported.");
        expect(has_expected_normal, "TinyGltfGeometry smoke failed: NORMAL accessor data was not imported.");
        expect(has_expected_uv, "TinyGltfGeometry smoke failed: TEXCOORD_0 accessor data was not imported.");
    }

    const std::filesystem::path damaged_helmet_path =
        std::filesystem::path(NX_ASSET("assets/models/DamagedHelmet/DamagedHelmet.gltf"));
    if (std::filesystem::exists(damaged_helmet_path)) {
        NexAur::ModelImportRequest request;
        request.path = damaged_helmet_path;
        request.mode = NexAur::ModelImportMode::FullModel;
        request.tangent_policy = NexAur::TangentGenerationPolicy::PreserveImportedTangents;

        const NexAur::ModelImportResult import_result =
            asset_manager.getModelImporterRegistry().importModel(request);
        expect(import_result.success, "TinyGltfGeometry smoke failed: DamagedHelmet did not import through tinygltf.");
        expect(import_result.model && import_result.model->isLoaded(), "TinyGltfGeometry smoke failed: DamagedHelmet CPU model missing.");
        expect(import_result.metadata.mesh_count == 1, "DamagedHelmet should report one glTF mesh.");
        expect(import_result.metadata.primitive_count == 1, "DamagedHelmet should report one glTF primitive.");
        expect(import_result.metadata.vertex_count > 0, "DamagedHelmet tinygltf import should produce vertices.");
        expect(import_result.metadata.index_count > 0, "DamagedHelmet tinygltf import should produce indices.");
    } else {
        std::cout << "TinyGltfGeometry smoke skipped DamagedHelmet geometry check: optional asset is missing." << std::endl;
    }

    std::error_code remove_error;
    std::filesystem::remove(glb_path, remove_error);

    if (!success) {
        std::cerr << failure << std::endl;
        engine.shutdownEngine();
        return 1;
    }

    std::cout << "TinyGltfGeometry smoke passed." << std::endl;
    engine.shutdownEngine();
    return 0;
}

int runTinyGltfMaterialSmoke() {
    const std::filesystem::path glb_path =
        std::filesystem::path(ENGINE_ROOT_DIR) / "build" / "tiny_gltf_material_smoke.glb";
    const std::filesystem::path data_uri_gltf_path =
        std::filesystem::path(ENGINE_ROOT_DIR) / "build" / "tiny_gltf_material_data_uri_smoke.gltf";
    const std::filesystem::path data_uri_bin_path =
        std::filesystem::path(ENGINE_ROOT_DIR) / "build" / "tiny_gltf_material_data_uri_smoke.bin";

    bool success = true;
    std::string failure;

    auto expect = [&](bool condition, const std::string& message) {
        if (!success) {
            return;
        }
        success = expectGameplay(condition, message, failure);
    };

    auto expectTextureAsset = [&](
        NexAur::AssetManager& asset_manager,
        NexAur::AssetHandle handle,
        NexAur::TextureColorSpace color_space,
        const std::string& label) {
        const NexAur::AssetMetadata* metadata = asset_manager.getMetadata(handle);
        expect(metadata != nullptr, label + " metadata should exist.");
        if (metadata) {
            expect(metadata->type == NexAur::AssetType::Texture2D, label + " should register as Texture2D.");
            expect(metadata->texture_color_space == color_space, label + " should register with expected color space.");
        }

        std::shared_ptr<NexAur::TextureAsset> texture = asset_manager.loadTextureCPU(handle);
        expect(texture != nullptr && texture->isLoaded(), label + " CPU texture should be loaded.");
        if (texture) {
            expect(texture->getWidth() == 1 && texture->getHeight() == 1, label + " should decode the embedded 1x1 PNG.");
            expect(texture->getColorSpace() == color_space, label + " CPU texture should keep expected color space.");
        }
    };

    expect(writeTinyGltfMaterialSmokeGlb(glb_path), "TinyGltfMaterial smoke failed: could not write material GLB.");

    NexAur::Engine engine;
    engine.startEngine();

    NexAur::AssetManager& asset_manager = NexAur::AssetManager::getInstance();

    NexAur::ModelImportRequest request;
    request.path = glb_path;
    request.mode = NexAur::ModelImportMode::FullModel;
    request.tangent_policy = NexAur::TangentGenerationPolicy::GenerateIfMissing;

    const NexAur::ModelImportResult import_result =
        asset_manager.getModelImporterRegistry().importModel(request);
    expect(import_result.success, "TinyGltfMaterial smoke failed: embedded material GLB did not import.");
    expect(import_result.model && import_result.model->isLoaded(), "TinyGltfMaterial smoke failed: CPU model missing.");
    expect(import_result.metadata.texture_count == 5, "TinyGltfMaterial smoke should report five glTF textures.");
    expect(import_result.metadata.image_count == 5, "TinyGltfMaterial smoke should report five glTF images.");

    if (import_result.model && import_result.model->isLoaded() && !import_result.model->getMeshes().empty()) {
        const NexAur::MaterialImportData& material =
            import_result.model->getMeshes().front().getMaterialImportData();

        expect(material.name == "TinyGltfMaterialSmoke", "TinyGltfMaterial smoke material name should round-trip.");
        expect(nearlyEqual(material.base_color_factor.x, 0.2f) &&
               nearlyEqual(material.base_color_factor.y, 0.4f) &&
               nearlyEqual(material.base_color_factor.z, 0.6f) &&
               nearlyEqual(material.base_color_factor.w, 0.8f),
            "TinyGltfMaterial smoke base color factor should round-trip.");
        expect(nearlyEqual(material.metallic_factor, 0.7f), "TinyGltfMaterial smoke metallic factor should round-trip.");
        expect(nearlyEqual(material.roughness_factor, 0.3f), "TinyGltfMaterial smoke roughness factor should round-trip.");
        expect(nearlyEqualVec3(material.emissive_factor, glm::vec3{ 0.1f, 0.2f, 0.3f }), "TinyGltfMaterial smoke emissive factor should round-trip.");
        expect(nearlyEqual(material.normal_scale, 0.5f), "TinyGltfMaterial smoke normal scale should round-trip.");
        expect(nearlyEqual(material.occlusion_strength, 0.25f), "TinyGltfMaterial smoke AO strength should round-trip.");
        expect(material.alpha_mode == NexAur::MaterialAlphaMode::Mask, "TinyGltfMaterial smoke alpha mode should be MASK.");
        expect(nearlyEqual(material.alpha_cutoff, 0.42f), "TinyGltfMaterial smoke alpha cutoff should round-trip.");
        expect(material.double_sided, "TinyGltfMaterial smoke doubleSided should round-trip.");
        expect(material.metallic_roughness_mode == NexAur::MaterialMetallicRoughnessTextureMode::PackedGltf,
            "TinyGltfMaterial smoke should use packed glTF metallic-roughness.");

        expect(material.base_color_texture_path.empty(), "Embedded base color texture should not use a file path.");
        expect(material.normal_texture_path.empty(), "Embedded normal texture should not use a file path.");
        expect(material.metallic_roughness_texture_path.empty(), "Embedded metallic-roughness texture should not use a file path.");
        expect(material.ao_texture_path.empty(), "Embedded AO texture should not use a file path.");
        expect(material.emissive_texture_path.empty(), "Embedded emissive texture should not use a file path.");
        expect(material.base_color_texture_asset && material.base_color_texture_asset->isLoaded(), "Embedded base color texture asset should be decoded.");
        expect(material.normal_texture_asset && material.normal_texture_asset->isLoaded(), "Embedded normal texture asset should be decoded.");
        expect(material.metallic_roughness_texture_asset && material.metallic_roughness_texture_asset->isLoaded(), "Embedded MR texture asset should be decoded.");
        expect(material.ao_texture_asset && material.ao_texture_asset->isLoaded(), "Embedded AO texture asset should be decoded.");
        expect(material.emissive_texture_asset && material.emissive_texture_asset->isLoaded(), "Embedded emissive texture asset should be decoded.");

        std::shared_ptr<NexAur::MaterialAsset> material_asset =
            asset_manager.createMaterialFromImportData(material);
        expect(material_asset != nullptr, "TinyGltfMaterial smoke material asset should be created.");
        if (material_asset) {
            expect(material_asset->usesPackedMetallicRoughness(), "TinyGltfMaterial smoke material asset should use packed MR.");
            expect(material_asset->isDoubleSided(), "TinyGltfMaterial smoke material asset should keep doubleSided.");

            expectTextureAsset(asset_manager, material_asset->getBaseColorTexture(), NexAur::TextureColorSpace::SRGB, "Embedded base color texture");
            expectTextureAsset(asset_manager, material_asset->getMetallicRoughnessTexture(), NexAur::TextureColorSpace::Linear, "Embedded metallic-roughness texture");
            expectTextureAsset(asset_manager, material_asset->getNormalTexture(), NexAur::TextureColorSpace::Linear, "Embedded normal texture");
            expectTextureAsset(asset_manager, material_asset->getAOTexture(), NexAur::TextureColorSpace::Linear, "Embedded AO texture");
            expectTextureAsset(asset_manager, material_asset->getEmissiveTexture(), NexAur::TextureColorSpace::SRGB, "Embedded emissive texture");
        }
    }

    expect(
        writeTinyGltfDataUriMaterialSmokeGltf(data_uri_gltf_path, data_uri_bin_path),
        "TinyGltfMaterial smoke failed: could not write Data URI material glTF.");

    NexAur::ModelImportRequest data_uri_request;
    data_uri_request.path = data_uri_gltf_path;
    data_uri_request.mode = NexAur::ModelImportMode::FullModel;

    const NexAur::ModelImportResult data_uri_result =
        asset_manager.getModelImporterRegistry().importModel(data_uri_request);
    expect(data_uri_result.success, "TinyGltfMaterial smoke failed: Data URI material glTF did not import.");
    if (data_uri_result.model && !data_uri_result.model->getMeshes().empty()) {
        const NexAur::MaterialImportData& data_uri_material =
            data_uri_result.model->getMeshes().front().getMaterialImportData();
        expect(data_uri_material.base_color_texture_path.empty(), "Data URI base color should not use a file path.");
        expect(
            data_uri_material.base_color_texture_asset && data_uri_material.base_color_texture_asset->isLoaded(),
            "Data URI base color should decode into a CPU texture asset.");

        std::shared_ptr<NexAur::MaterialAsset> data_uri_material_asset =
            asset_manager.createMaterialFromImportData(data_uri_material);
        expect(data_uri_material_asset != nullptr, "Data URI material asset should be created.");
        if (data_uri_material_asset) {
            expectTextureAsset(
                asset_manager,
                data_uri_material_asset->getBaseColorTexture(),
                NexAur::TextureColorSpace::SRGB,
                "Data URI base color texture");
        }
    }

    const std::filesystem::path damaged_helmet_path =
        std::filesystem::path(NX_ASSET("assets/models/DamagedHelmet/DamagedHelmet.gltf"));
    if (std::filesystem::exists(damaged_helmet_path)) {
        NexAur::ModelImportRequest helmet_request;
        helmet_request.path = damaged_helmet_path;
        helmet_request.mode = NexAur::ModelImportMode::FullModel;

        const NexAur::ModelImportResult helmet_result =
            asset_manager.getModelImporterRegistry().importModel(helmet_request);
        expect(helmet_result.success, "TinyGltfMaterial smoke failed: DamagedHelmet tinygltf material import failed.");
        if (helmet_result.model && !helmet_result.model->getMeshes().empty()) {
            const NexAur::MaterialImportData& helmet_material =
                helmet_result.model->getMeshes().front().getMaterialImportData();
            expect(std::filesystem::path(helmet_material.base_color_texture_path).filename().string() == "Default_albedo.jpg",
                "DamagedHelmet tinygltf base color texture path should resolve.");
            expect(std::filesystem::path(helmet_material.normal_texture_path).filename().string() == "Default_normal.jpg",
                "DamagedHelmet tinygltf normal texture path should resolve.");
            expect(std::filesystem::path(helmet_material.metallic_roughness_texture_path).filename().string() == "Default_metalRoughness.jpg",
                "DamagedHelmet tinygltf metallic-roughness texture path should resolve.");
            expect(std::filesystem::path(helmet_material.ao_texture_path).filename().string() == "Default_AO.jpg",
                "DamagedHelmet tinygltf AO texture path should resolve.");
            expect(std::filesystem::path(helmet_material.emissive_texture_path).filename().string() == "Default_emissive.jpg",
                "DamagedHelmet tinygltf emissive texture path should resolve.");
            expect(helmet_material.metallic_roughness_mode == NexAur::MaterialMetallicRoughnessTextureMode::PackedGltf,
                "DamagedHelmet tinygltf material should use packed MR.");
            expect(nearlyEqual(helmet_material.metallic_factor, 1.0f), "DamagedHelmet tinygltf metallic factor should follow glTF default.");
            expect(nearlyEqual(helmet_material.roughness_factor, 1.0f), "DamagedHelmet tinygltf roughness factor should follow glTF default.");
        }
    }

    std::error_code remove_error;
    std::filesystem::remove(glb_path, remove_error);
    std::filesystem::remove(data_uri_gltf_path, remove_error);
    std::filesystem::remove(data_uri_bin_path, remove_error);

    if (!success) {
        std::cerr << failure << std::endl;
        engine.shutdownEngine();
        return 1;
    }

    std::cout << "TinyGltfMaterial smoke passed." << std::endl;
    engine.shutdownEngine();
    return 0;
}

int runDamagedHelmetReferenceSmoke() {
    const std::filesystem::path damaged_helmet_path =
        std::filesystem::path(NX_ASSET("assets/models/DamagedHelmet/DamagedHelmet.gltf"));

    if (!std::filesystem::exists(damaged_helmet_path)) {
        std::cout << "DamagedHelmetReference smoke skipped: optional DamagedHelmet asset is missing." << std::endl;
        return 0;
    }

    bool success = true;
    std::string failure;

    auto expect = [&](bool condition, const std::string& message) {
        if (!success) {
            return;
        }
        success = expectGameplay(condition, message, failure);
    };

    auto expectFilename = [&](const std::string& path, const std::string& filename, const std::string& label) {
        expect(!path.empty(), label + " path should not be empty.");
        if (!path.empty()) {
            expect(
                std::filesystem::path(path).filename().string() == filename,
                label + " should resolve to " + filename + ".");
        }
    };

    auto expectTextureMetadata = [&](
        NexAur::AssetManager& asset_manager,
        NexAur::AssetHandle texture,
        NexAur::TextureColorSpace color_space,
        const std::string& filename,
        const std::string& label) {
        expect(texture.isValid(), label + " texture handle should be valid.");
        const NexAur::AssetMetadata* metadata = asset_manager.getMetadata(texture);
        expect(metadata != nullptr, label + " metadata should exist.");
        if (metadata) {
            expect(metadata->type == NexAur::AssetType::Texture2D, label + " should register as Texture2D.");
            expect(metadata->texture_color_space == color_space, label + " color space should match glTF slot policy.");
            expect(
                std::filesystem::path(metadata->path).filename().string() == filename,
                label + " metadata path should resolve to " + filename + ".");
        }
    };

    auto isValidDirection = [](const glm::vec3& value) {
        return std::isfinite(value.x) &&
               std::isfinite(value.y) &&
               std::isfinite(value.z) &&
               glm::dot(value, value) > 0.000001f;
    };

    NexAur::Engine engine;
    engine.startEngine();

    NexAur::AssetManager& asset_manager = NexAur::AssetManager::getInstance();
    const NexAur::AssetHandle default_handle = asset_manager.importModelAsset(damaged_helmet_path.string());
    const NexAur::AssetHandle tinygltf_handle = asset_manager.importModelAssetFromRegistry(damaged_helmet_path.string());
    expect(default_handle.isValid(), "DamagedHelmet should import through the default AssetManager path.");
    expect(tinygltf_handle.isValid(), "DamagedHelmet should import through the tinygltf registry path.");
    if (default_handle && tinygltf_handle) {
        expect(
            default_handle == tinygltf_handle,
            "DamagedHelmet default and explicit registry imports should share the tinygltf asset identity.");
    }

    const NexAur::AssetMetadata* model_metadata = asset_manager.getMetadata(tinygltf_handle);
    expect(model_metadata != nullptr, "DamagedHelmet tinygltf model metadata should exist.");
    if (model_metadata) {
        expect(model_metadata->type == NexAur::AssetType::Model, "DamagedHelmet tinygltf metadata should be Model.");
        expect(
            std::filesystem::path(model_metadata->path).filename().string() == "DamagedHelmet.gltf",
            "DamagedHelmet tinygltf metadata path should keep the glTF source filename.");
    }

    std::shared_ptr<NexAur::Model> model = asset_manager.loadModelCPU(tinygltf_handle);
    expect(model != nullptr && model->isLoaded(), "DamagedHelmet tinygltf CPU model should load.");
    expect(model && model->getMeshes().size() == 1, "DamagedHelmet reference should import one mesh.");

    if (model && model->isLoaded() && !model->getMeshes().empty()) {
        const NexAur::Mesh& mesh = model->getMeshes().front();
        expect(mesh.GetVertices().size() == 14556, "DamagedHelmet reference vertex count should match the glTF accessor.");
        expect(mesh.GetIndices().size() == 46356, "DamagedHelmet reference index count should match the glTF accessor.");

        size_t valid_tangent_frames = 0;
        for (const NexAur::Vertex& vertex : mesh.GetVertices()) {
            if (!isValidDirection(vertex.normal) ||
                !isValidDirection(vertex.tangent) ||
                !isValidDirection(vertex.bitangent)) {
                continue;
            }

            const glm::vec3 normal = glm::normalize(vertex.normal);
            const glm::vec3 tangent = glm::normalize(vertex.tangent);
            const glm::vec3 bitangent = glm::normalize(vertex.bitangent);
            const bool nearly_orthogonal =
                std::abs(glm::dot(normal, tangent)) < 0.25f &&
                std::abs(glm::dot(normal, bitangent)) < 0.25f &&
                std::abs(glm::dot(tangent, bitangent)) < 0.35f;
            if (nearly_orthogonal) {
                ++valid_tangent_frames;
            }
        }
        expect(
            valid_tangent_frames * 100 >= mesh.GetVertices().size() * 95,
            "DamagedHelmet reference should generate stable tangent frames for normal mapping.");

        const NexAur::MaterialImportData& material = mesh.getMaterialImportData();
        expect(material.name == "Material_MR", "DamagedHelmet material name should match the glTF reference.");
        expectFilename(material.base_color_texture_path, "Default_albedo.jpg", "Base color texture");
        expectFilename(material.normal_texture_path, "Default_normal.jpg", "Normal texture");
        expectFilename(material.metallic_roughness_texture_path, "Default_metalRoughness.jpg", "Metallic-roughness texture");
        expectFilename(material.ao_texture_path, "Default_AO.jpg", "AO texture");
        expectFilename(material.emissive_texture_path, "Default_emissive.jpg", "Emissive texture");
        expect(material.metallic_texture_path.empty(), "DamagedHelmet reference should not keep a separate metallic texture.");
        expect(material.roughness_texture_path.empty(), "DamagedHelmet reference should not keep a separate roughness texture.");
        expect(
            material.metallic_roughness_mode == NexAur::MaterialMetallicRoughnessTextureMode::PackedGltf,
            "DamagedHelmet reference should use packed glTF metallic-roughness.");
        expect(nearlyEqual(material.metallic_factor, 1.0f), "DamagedHelmet metallic factor should follow glTF default.");
        expect(nearlyEqual(material.roughness_factor, 1.0f), "DamagedHelmet roughness factor should follow glTF default.");
        expect(nearlyEqual(material.normal_scale, 1.0f), "DamagedHelmet normal scale should follow glTF default.");
        expect(nearlyEqual(material.occlusion_strength, 1.0f), "DamagedHelmet AO strength should follow glTF default.");
        expect(nearlyEqualVec3(material.emissive_factor, glm::vec3{ 1.0f }), "DamagedHelmet emissive factor should match the reference glTF.");
        expect(material.alpha_mode == NexAur::MaterialAlphaMode::Opaque, "DamagedHelmet alpha mode should be opaque.");
        expect(!material.double_sided, "DamagedHelmet doubleSided should stay false.");

        std::shared_ptr<NexAur::MaterialAsset> material_asset =
            asset_manager.createMaterialFromImportData(material);
        expect(material_asset != nullptr, "DamagedHelmet reference material asset should be created.");
        if (material_asset) {
            expect(material_asset->usesPackedMetallicRoughness(), "DamagedHelmet material asset should use packed MR.");
            expectTextureMetadata(
                asset_manager,
                material_asset->getBaseColorTexture(),
                NexAur::TextureColorSpace::SRGB,
                "Default_albedo.jpg",
                "Base color texture");
            expectTextureMetadata(
                asset_manager,
                material_asset->getNormalTexture(),
                NexAur::TextureColorSpace::Linear,
                "Default_normal.jpg",
                "Normal texture");
            expectTextureMetadata(
                asset_manager,
                material_asset->getMetallicRoughnessTexture(),
                NexAur::TextureColorSpace::Linear,
                "Default_metalRoughness.jpg",
                "Metallic-roughness texture");
            expectTextureMetadata(
                asset_manager,
                material_asset->getAOTexture(),
                NexAur::TextureColorSpace::Linear,
                "Default_AO.jpg",
                "AO texture");
            expectTextureMetadata(
                asset_manager,
                material_asset->getEmissiveTexture(),
                NexAur::TextureColorSpace::SRGB,
                "Default_emissive.jpg",
                "Emissive texture");
        }
    }

    if (!success) {
        std::cerr << failure << std::endl;
        engine.shutdownEngine();
        return 1;
    }

    std::cout << "DamagedHelmetReference smoke passed." << std::endl;
    engine.shutdownEngine();
    return 0;
}

int runTinyGltfSceneSmoke() {
    const std::filesystem::path gltf_path =
        std::filesystem::path(ENGINE_ROOT_DIR) / "build" / "tiny_gltf_scene_smoke.gltf";
    const std::filesystem::path bin_path =
        std::filesystem::path(ENGINE_ROOT_DIR) / "build" / "tiny_gltf_scene_smoke.bin";

    bool success = true;
    std::string failure;

    auto expect = [&](bool condition, const std::string& message) {
        if (!success) {
            return;
        }
        success = expectGameplay(condition, message, failure);
    };

    auto hasPosition = [](const NexAur::Mesh& mesh, const glm::vec3& expected) {
        for (const NexAur::Vertex& vertex : mesh.GetVertices()) {
            if (nearlyEqual(vertex.position.x, expected.x) &&
                nearlyEqual(vertex.position.y, expected.y) &&
                nearlyEqual(vertex.position.z, expected.z)) {
                return true;
            }
        }

        return false;
    };

    expect(
        writeTinyGltfSceneSmokeGltf(gltf_path, bin_path),
        "TinyGltfScene smoke failed: could not write scene glTF.");

    NexAur::Engine engine;
    engine.startEngine();

    NexAur::AssetManager& asset_manager = NexAur::AssetManager::getInstance();
    NexAur::ModelImportRequest request;
    request.path = gltf_path;
    request.mode = NexAur::ModelImportMode::FullModel;
    request.tangent_policy = NexAur::TangentGenerationPolicy::GenerateIfMissing;

    const NexAur::ModelImportResult result =
        asset_manager.getModelImporterRegistry().importModel(request);
    expect(result.success, "TinyGltfScene smoke failed: scene glTF did not import.");
    expect(result.metadata.default_scene == 1, "TinyGltfScene smoke should report default scene index 1.");
    expect(result.metadata.scene_count == 2, "TinyGltfScene smoke should report two scenes.");
    expect(result.metadata.primitive_count == 2, "TinyGltfScene smoke should report two mesh primitives.");
    expect(result.metadata.vertex_count == 6, "TinyGltfScene smoke should import six baked vertices.");
    expect(result.metadata.index_count == 6, "TinyGltfScene smoke should import six indices.");
    expect(result.model && result.model->isLoaded(), "TinyGltfScene smoke failed: CPU model missing.");
    expect(result.model && result.model->getMeshes().size() == 2, "TinyGltfScene smoke should produce one Mesh per primitive.");

    if (result.model && result.model->getMeshes().size() == 2) {
        const NexAur::Mesh& first_mesh = result.model->getMeshes()[0];
        const NexAur::Mesh& second_mesh = result.model->getMeshes()[1];

        expect(first_mesh.GetVertices().size() == 3, "First primitive should keep three vertices.");
        expect(second_mesh.GetVertices().size() == 3, "Second primitive should keep three vertices.");
        expect(first_mesh.GetIndices().size() == 3, "First primitive should keep three indices.");
        expect(second_mesh.GetIndices().size() == 3, "Second primitive should keep three indices.");
        expect(first_mesh.getMaterialImportData().name == "SceneSmokeMat0", "First primitive should keep material 0.");
        expect(second_mesh.getMaterialImportData().name == "SceneSmokeMat1", "Second primitive should keep material 1.");

        expect(hasPosition(first_mesh, glm::vec3{ 10.0f, 20.0f, 30.0f }), "First primitive should include baked root+child translation.");
        expect(hasPosition(first_mesh, glm::vec3{ 12.0f, 20.0f, 30.0f }), "First primitive should include baked child scale.");
        expect(hasPosition(second_mesh, glm::vec3{ 10.0f, 20.0f, 31.0f }), "Second primitive should keep local z after transform.");

        for (const NexAur::Vertex& vertex : first_mesh.GetVertices()) {
            expect(
                nearlyEqual(vertex.normal.x, 0.0f) &&
                nearlyEqual(vertex.normal.y, 0.0f) &&
                nearlyEqual(vertex.normal.z, 1.0f),
                "Transformed normals should remain valid.");
        }
    }

    std::error_code remove_error;
    std::filesystem::remove(gltf_path, remove_error);
    std::filesystem::remove(bin_path, remove_error);

    if (!success) {
        std::cerr << failure << std::endl;
        engine.shutdownEngine();
        return 1;
    }

    std::cout << "TinyGltfScene smoke passed." << std::endl;
    engine.shutdownEngine();
    return 0;
}

int runGameplaySystemsSmoke() {
    auto fake_input = std::make_shared<FakeInputService>();
    NexAur::InputActionSystem input_actions(fake_input);
    input_actions.configureDefaultBindings();

    NexAur::SceneV2 scene;
    bool success = true;
    std::string failure;

    auto expect = [&](bool condition, const std::string& message) {
        if (!success) {
            return;
        }
        success = expectGameplay(condition, message, failure);
    };

    NexAur::Entity player = scene.createEntity("Player");
    player.addComponent<NexAur::PlayerComponent>().move_speed = 5.0f;
    player.addComponent<NexAur::VelocityComponent>();

    fake_input->setKey(NexAur::KeyCode::W, true);
    input_actions.update();

    NexAur::PlayerControlSystem player_control_system;
    NexAur::EnemySystem enemy_system;
    NexAur::MovementSystem movement_system;
    NexAur::LifetimeSystem lifetime_system;
    NexAur::HealthSystem health_system;

    player_control_system.update(scene, input_actions, NexAur::TimeStep{ 1.0 });
    movement_system.update(scene, NexAur::TimeStep{ 1.0 });

    const NexAur::TransformComponent& player_transform = player.getComponent<NexAur::TransformComponent>();
    const NexAur::VelocityComponent& player_velocity = player.getComponent<NexAur::VelocityComponent>();
    expect(nearlyEqual(player_velocity.velocity.z, -5.0f), "Player velocity should follow Move input on -Z.");
    expect(nearlyEqual(player_transform.translation.z, -5.0f), "Player should move on -Z after movement update.");

    NexAur::Entity enemy = scene.createEntity("Enemy");
    enemy.addComponent<NexAur::EnemyComponent>().move_speed = 2.0f;
    enemy.addComponent<NexAur::VelocityComponent>();
    enemy.getComponent<NexAur::TransformComponent>().translation = glm::vec3{ 0.0f, 0.0f, 5.0f };

    enemy_system.update(scene, NexAur::TimeStep{ 1.0 });
    const NexAur::VelocityComponent& enemy_velocity = enemy.getComponent<NexAur::VelocityComponent>();
    expect(enemy_velocity.velocity.z < 0.0f, "Enemy should move toward the player on -Z.");

    NexAur::Entity timed_entity = scene.createEntity("Expired");
    timed_entity.addComponent<NexAur::LifetimeComponent>().seconds = 0.25f;
    const entt::entity timed_handle = static_cast<entt::entity>(timed_entity);
    lifetime_system.update(scene, NexAur::TimeStep{ 0.5 });
    expect(!scene.getRegistry().valid(timed_handle), "LifetimeSystem should destroy expired entities.");

    NexAur::Entity dead_entity = scene.createEntity("Dead");
    auto& dead_health = dead_entity.addComponent<NexAur::HealthComponent>();
    dead_health.current = 0;
    dead_health.max = 3;
    const entt::entity dead_handle = static_cast<entt::entity>(dead_entity);
    health_system.update(scene);
    expect(!scene.getRegistry().valid(dead_handle), "HealthSystem should destroy dead entities.");

    NexAur::Entity gameplay_state = scene.createEntity("GameplayState");
    auto& health = gameplay_state.addComponent<NexAur::HealthComponent>();
    health.current = 2;
    health.max = 5;
    gameplay_state.addComponent<NexAur::CollectibleComponent>().score = 10;
    gameplay_state.addComponent<NexAur::LifetimeComponent>().seconds = 3.0f;

    const std::filesystem::path smoke_path =
        std::filesystem::path(ENGINE_ROOT_DIR) / "build" / "gameplay_systems_smoke.nxscene";

    NexAur::SceneSerializer serializer(NexAur::AssetManager::getInstance());
    const NexAur::SceneSerializationResult save_result = serializer.save(scene, smoke_path);
    if (!save_result) {
        success = false;
        failure = save_result.message;
    } else {
        const NexAur::SceneLoadResult load_result = serializer.load(smoke_path);
        if (!load_result) {
            success = false;
            failure = load_result.message;
        } else {
            const NexAur::PlayerComponent* loaded_player =
                findComponentByName<NexAur::PlayerComponent>(load_result.scene, "Player");
            const NexAur::VelocityComponent* loaded_player_velocity =
                findComponentByName<NexAur::VelocityComponent>(load_result.scene, "Player");
            const NexAur::HealthComponent* loaded_health =
                findComponentByName<NexAur::HealthComponent>(load_result.scene, "GameplayState");
            const NexAur::CollectibleComponent* loaded_collectible =
                findComponentByName<NexAur::CollectibleComponent>(load_result.scene, "GameplayState");
            const NexAur::LifetimeComponent* loaded_lifetime =
                findComponentByName<NexAur::LifetimeComponent>(load_result.scene, "GameplayState");

            expect(loaded_player && nearlyEqual(loaded_player->move_speed, 5.0f), "Loaded PlayerComponent is invalid.");
            expect(
                loaded_player_velocity && nearlyEqual(loaded_player_velocity->velocity.z, -5.0f),
                "Loaded VelocityComponent is invalid.");
            expect(loaded_health && loaded_health->current == 2 && loaded_health->max == 5, "Loaded HealthComponent is invalid.");
            expect(loaded_collectible && loaded_collectible->score == 10, "Loaded CollectibleComponent is invalid.");
            expect(loaded_lifetime && nearlyEqual(loaded_lifetime->seconds, 3.0f), "Loaded LifetimeComponent is invalid.");
        }
    }

    std::error_code remove_error;
    std::filesystem::remove(smoke_path, remove_error);

    if (!success) {
        std::cerr << "Gameplay systems smoke failed: " << failure << std::endl;
        return 1;
    }

    std::cout << "Gameplay systems smoke passed." << std::endl;
    return 0;
}

int runPhysicsTriggerSmoke() {
    bool success = true;
    std::string failure;

    auto expect = [&](bool condition, const std::string& message) {
        if (!success) {
            return;
        }
        success = expectGameplay(condition, message, failure);
    };

    auto countOverlaps = [](NexAur::SceneV2& scene) {
        NexAur::TriggerOverlapSystem trigger_system;
        trigger_system.update(scene);
        return trigger_system.getFrame().overlaps.size();
    };

    {
        NexAur::SceneV2 scene;
        NexAur::Entity lhs = scene.createEntity("SphereA");
        lhs.addComponent<NexAur::SphereColliderComponent>().radius = 1.0f;
        lhs.addComponent<NexAur::TriggerComponent>();

        NexAur::Entity rhs = scene.createEntity("SphereB");
        rhs.addComponent<NexAur::SphereColliderComponent>().radius = 1.0f;
        rhs.getComponent<NexAur::TransformComponent>().translation = glm::vec3{ 1.5f, 0.0f, 0.0f };

        expect(countOverlaps(scene) == 1, "Sphere/Sphere should overlap.");
        rhs.getComponent<NexAur::TransformComponent>().translation = glm::vec3{ 3.0f, 0.0f, 0.0f };
        expect(countOverlaps(scene) == 0, "Separated Sphere/Sphere should not overlap.");
    }

    {
        NexAur::SceneV2 scene;
        NexAur::Entity lhs = scene.createEntity("AABBA");
        lhs.addComponent<NexAur::AABBColliderComponent>().half_extents = glm::vec3{ 1.0f };
        lhs.addComponent<NexAur::TriggerComponent>();

        NexAur::Entity rhs = scene.createEntity("AABBB");
        rhs.addComponent<NexAur::AABBColliderComponent>().half_extents = glm::vec3{ 1.0f };
        rhs.getComponent<NexAur::TransformComponent>().translation = glm::vec3{ 1.5f, 0.0f, 0.0f };

        expect(countOverlaps(scene) == 1, "AABB/AABB should overlap.");
    }

    {
        NexAur::SceneV2 scene;
        NexAur::Entity sphere = scene.createEntity("Sphere");
        sphere.addComponent<NexAur::SphereColliderComponent>().radius = 1.0f;
        sphere.addComponent<NexAur::TriggerComponent>();

        NexAur::Entity box = scene.createEntity("Box");
        box.addComponent<NexAur::AABBColliderComponent>().half_extents = glm::vec3{ 0.5f };
        box.getComponent<NexAur::TransformComponent>().translation = glm::vec3{ 1.25f, 0.0f, 0.0f };

        expect(countOverlaps(scene) == 1, "Sphere/AABB should overlap.");
    }

    {
        NexAur::SceneV2 scene;
        NexAur::Entity lhs = scene.createEntity("FilteredA");
        lhs.addComponent<NexAur::SphereColliderComponent>().radius = 1.0f;
        lhs.addComponent<NexAur::TriggerComponent>();
        auto& lhs_filter = lhs.addComponent<NexAur::CollisionFilterComponent>();
        lhs_filter.layer = 1u;
        lhs_filter.mask = 1u;

        NexAur::Entity rhs = scene.createEntity("FilteredB");
        rhs.addComponent<NexAur::SphereColliderComponent>().radius = 1.0f;
        auto& rhs_filter = rhs.addComponent<NexAur::CollisionFilterComponent>();
        rhs_filter.layer = 2u;
        rhs_filter.mask = 2u;

        expect(countOverlaps(scene) == 0, "Layer/mask should filter overlap.");
        lhs_filter.mask = 2u;
        rhs_filter.mask = 1u;
        expect(countOverlaps(scene) == 1, "Layer/mask should allow overlap when masks match.");
    }

    {
        NexAur::SceneV2 scene;
        NexAur::Entity player = scene.createEntity("Player");
        player.addComponent<NexAur::PlayerComponent>();
        player.addComponent<NexAur::SphereColliderComponent>().radius = 1.0f;

        NexAur::Entity collectible = scene.createEntity("Collectible");
        collectible.addComponent<NexAur::CollectibleComponent>().score = 10;
        collectible.addComponent<NexAur::SphereColliderComponent>().radius = 0.5f;
        collectible.addComponent<NexAur::TriggerComponent>();
        collectible.getComponent<NexAur::TransformComponent>().translation = glm::vec3{ 0.75f, 0.0f, 0.0f };

        const entt::entity collectible_handle = static_cast<entt::entity>(collectible);
        NexAur::TriggerOverlapSystem trigger_system;
        NexAur::CollectibleSystem collectible_system;
        trigger_system.update(scene);
        collectible_system.update(scene, trigger_system.getFrame());

        expect(!scene.getRegistry().valid(collectible_handle), "Collectible should be destroyed on player trigger overlap.");
    }

    {
        NexAur::SceneV2 scene;
        NexAur::Entity sphere = scene.createEntity("SerializedSphere");
        auto& sphere_collider = sphere.addComponent<NexAur::SphereColliderComponent>();
        sphere_collider.offset = glm::vec3{ 1.0f, 2.0f, 3.0f };
        sphere_collider.radius = 1.25f;
        sphere.addComponent<NexAur::TriggerComponent>().enabled = true;
        auto& filter = sphere.addComponent<NexAur::CollisionFilterComponent>();
        filter.layer = 4u;
        filter.mask = 8u;

        NexAur::Entity box = scene.createEntity("SerializedBox");
        auto& aabb = box.addComponent<NexAur::AABBColliderComponent>();
        aabb.offset = glm::vec3{ -1.0f, 0.0f, 0.5f };
        aabb.half_extents = glm::vec3{ 0.25f, 0.5f, 0.75f };

        const std::filesystem::path smoke_path =
            std::filesystem::path(ENGINE_ROOT_DIR) / "build" / "physics_trigger_smoke.nxscene";

        NexAur::SceneSerializer serializer(NexAur::AssetManager::getInstance());
        const NexAur::SceneSerializationResult save_result = serializer.save(scene, smoke_path);
        if (!save_result) {
            success = false;
            failure = save_result.message;
        } else {
            const NexAur::SceneLoadResult load_result = serializer.load(smoke_path);
            if (!load_result) {
                success = false;
                failure = load_result.message;
            } else {
                const NexAur::SphereColliderComponent* loaded_sphere =
                    findComponentByName<NexAur::SphereColliderComponent>(load_result.scene, "SerializedSphere");
                const NexAur::TriggerComponent* loaded_trigger =
                    findComponentByName<NexAur::TriggerComponent>(load_result.scene, "SerializedSphere");
                const NexAur::CollisionFilterComponent* loaded_filter =
                    findComponentByName<NexAur::CollisionFilterComponent>(load_result.scene, "SerializedSphere");
                const NexAur::AABBColliderComponent* loaded_aabb =
                    findComponentByName<NexAur::AABBColliderComponent>(load_result.scene, "SerializedBox");

                expect(
                    loaded_sphere
                        && nearlyEqual(loaded_sphere->offset.x, 1.0f)
                        && nearlyEqual(loaded_sphere->offset.y, 2.0f)
                        && nearlyEqual(loaded_sphere->offset.z, 3.0f)
                        && nearlyEqual(loaded_sphere->radius, 1.25f),
                    "Loaded SphereColliderComponent is invalid.");
                expect(loaded_trigger && loaded_trigger->enabled, "Loaded TriggerComponent is invalid.");
                expect(loaded_filter && loaded_filter->layer == 4u && loaded_filter->mask == 8u, "Loaded CollisionFilterComponent is invalid.");
                expect(
                    loaded_aabb
                        && nearlyEqual(loaded_aabb->offset.x, -1.0f)
                        && nearlyEqual(loaded_aabb->half_extents.z, 0.75f),
                    "Loaded AABBColliderComponent is invalid.");
            }
        }

        std::error_code remove_error;
        std::filesystem::remove(smoke_path, remove_error);
    }

    if (!success) {
        std::cerr << "Physics trigger smoke failed: " << failure << std::endl;
        return 1;
    }

    std::cout << "Physics trigger smoke passed." << std::endl;
    return 0;
}

int runRuntimeCameraSmoke() {
    NexAur::SceneV2 scene;
    bool success = true;
    std::string failure;

    auto expect = [&](bool condition, const std::string& message) {
        if (!success) {
            return;
        }
        success = expectGameplay(condition, message, failure);
    };

    NexAur::Entity player = scene.createEntity("Player");
    player.addComponent<NexAur::PlayerComponent>();
    player.getComponent<NexAur::TransformComponent>().translation = glm::vec3{ 2.0f, 0.0f, -3.0f };

    NexAur::Entity camera_entity = scene.createEntity("RuntimeCamera");
    auto& camera = camera_entity.addComponent<NexAur::CameraComponent>();
    camera.position = glm::vec3{ 0.0f };
    camera.nearClip = 0.1f;
    camera.farClip = 100.0f;
    camera_entity.addComponent<NexAur::ActiveCameraComponent>();
    auto& controller = camera_entity.addComponent<NexAur::RuntimeCameraControllerComponent>();
    controller.follow_offset = glm::vec3{ 0.0f, 4.0f, 8.0f };
    controller.target_offset = glm::vec3{ 0.0f, 1.0f, 0.0f };
    controller.follow_speed = 0.0f;
    controller.fov_degrees = 60.0f;
    controller.aspect_ratio = 4.0f / 3.0f;

    NexAur::RuntimeCameraControllerSystem camera_system;
    camera_system.update(scene, NexAur::TimeStep{ 1.0 });

    const glm::vec3 expected_position{ 2.0f, 5.0f, 5.0f };
    expect(nearlyEqualVec3(camera.position, expected_position), "Runtime camera should snap to follow position.");
    expect(
        nearlyEqualVec3(camera_entity.getComponent<NexAur::TransformComponent>().translation, expected_position),
        "Runtime camera transform should mirror camera position.");

    NexAur::RenderDataPacket render_packet;
    scene.extractSceneData(&render_packet);
    expect(
        nearlyEqualVec3(render_packet.camera_data.position, expected_position),
        "Render packet should use active runtime camera position.");
    expect(
        nearlyEqual(render_packet.camera_data.near_clip, 0.1f) && nearlyEqual(render_packet.camera_data.far_clip, 100.0f),
        "Render packet should preserve runtime camera clip planes.");

    const std::filesystem::path smoke_path =
        std::filesystem::path(ENGINE_ROOT_DIR) / "build" / "runtime_camera_smoke.nxscene";

    NexAur::SceneSerializer serializer(NexAur::AssetManager::getInstance());
    const NexAur::SceneSerializationResult save_result = serializer.save(scene, smoke_path);
    if (!save_result) {
        success = false;
        failure = save_result.message;
    } else {
        const NexAur::SceneLoadResult load_result = serializer.load(smoke_path);
        if (!load_result) {
            success = false;
            failure = load_result.message;
        } else {
            const NexAur::RuntimeCameraControllerComponent* loaded_controller =
                findComponentByName<NexAur::RuntimeCameraControllerComponent>(load_result.scene, "RuntimeCamera");
            expect(
                loaded_controller
                    && loaded_controller->mode == NexAur::RuntimeCameraMode::ThirdPersonFollow
                    && nearlyEqualVec3(loaded_controller->follow_offset, glm::vec3{ 0.0f, 4.0f, 8.0f })
                    && nearlyEqual(loaded_controller->fov_degrees, 60.0f)
                    && nearlyEqual(loaded_controller->aspect_ratio, 4.0f / 3.0f),
                "RuntimeCameraControllerComponent should round-trip through SceneSerializer.");
        }
    }

    std::error_code remove_error;
    std::filesystem::remove(smoke_path, remove_error);

    if (!success) {
        std::cerr << "Runtime camera smoke failed: " << failure << std::endl;
        return 1;
    }

    std::cout << "Runtime camera smoke passed." << std::endl;
    return 0;
}

int runRuntimeGameFlowSmoke() {
    bool success = true;
    std::string failure;

    auto expect = [&](bool condition, const std::string& message) {
        if (!success) {
            return;
        }
        success = expectGameplay(condition, message, failure);
    };

    {
        auto fake_input = std::make_shared<FakeInputService>();
        NexAur::InputActionSystem input_actions(fake_input);
        input_actions.configureDefaultBindings();

        NexAur::SceneV2 scene;
        NexAur::Entity player = scene.createEntity("Player");
        player.addComponent<NexAur::PlayerComponent>();
        player.addComponent<NexAur::VelocityComponent>().velocity = glm::vec3{ 0.0f, 0.0f, -10.0f };
        auto& health = player.addComponent<NexAur::HealthComponent>();
        health.current = 3;
        health.max = 3;

        NexAur::GameStateModel game_state;
        game_state.resetForScene(scene);

        input_actions.update();
        fake_input->setKey(NexAur::KeyCode::Escape, true);
        input_actions.update();
        if (input_actions.wasPressed(NexAur::DefaultInputActions::Pause)) {
            game_state.togglePause();
        }

        expect(game_state.getSnapshot().state == NexAur::GameState::Paused, "Pause action should switch to Paused.");

        NexAur::MovementSystem movement_system;
        const glm::vec3 initial_position = player.getComponent<NexAur::TransformComponent>().translation;
        if (game_state.isGameplayActive()) {
            movement_system.update(scene, NexAur::TimeStep{ 1.0 });
        }
        expect(
            nearlyEqualVec3(player.getComponent<NexAur::TransformComponent>().translation, initial_position),
            "Paused game state should stop movement integration.");

        fake_input->setKey(NexAur::KeyCode::Escape, false);
        input_actions.update();
        fake_input->setKey(NexAur::KeyCode::Escape, true);
        input_actions.update();
        if (input_actions.wasPressed(NexAur::DefaultInputActions::Pause)) {
            game_state.togglePause();
        }

        expect(game_state.getSnapshot().state == NexAur::GameState::Playing, "Pause action should resume Playing.");
    }

    {
        NexAur::SceneV2 scene;
        NexAur::Entity player = scene.createEntity("Player");
        player.addComponent<NexAur::PlayerComponent>();
        player.addComponent<NexAur::SphereColliderComponent>().radius = 1.0f;
        auto& health = player.addComponent<NexAur::HealthComponent>();
        health.current = 2;
        health.max = 5;

        NexAur::Entity collectible = scene.createEntity("Collectible");
        collectible.addComponent<NexAur::CollectibleComponent>().score = 10;
        collectible.addComponent<NexAur::SphereColliderComponent>().radius = 0.5f;
        collectible.addComponent<NexAur::TriggerComponent>();
        collectible.getComponent<NexAur::TransformComponent>().translation = glm::vec3{ 0.75f, 0.0f, 0.0f };

        NexAur::GameStateModel game_state;
        game_state.resetForScene(scene);
        expect(game_state.getSnapshot().collectible_count == 1, "GameState should count scene collectibles.");
        expect(game_state.getSnapshot().remaining_collectibles == 1, "GameState should track remaining collectibles.");

        NexAur::TriggerOverlapSystem trigger_system;
        NexAur::CollectibleSystem collectible_system;
        trigger_system.update(scene);
        const NexAur::CollectibleFrameResult result =
            collectible_system.update(scene, trigger_system.getFrame());
        game_state.addCollected(result.collected_count, result.collected_score);
        game_state.updateRules(scene);

        expect(result.collected_count == 1 && result.collected_score == 10, "CollectibleSystem should report collected score.");
        expect(game_state.getSnapshot().score == 10, "GameState should accumulate collectible score.");
        expect(game_state.getSnapshot().remaining_collectibles == 0, "GameState should update remaining collectibles.");
        expect(game_state.getSnapshot().state == NexAur::GameState::Victory, "Collecting all collectibles should enter Victory.");
    }

    {
        NexAur::SceneV2 scene;
        NexAur::Entity player = scene.createEntity("Player");
        player.addComponent<NexAur::PlayerComponent>();
        auto& health = player.addComponent<NexAur::HealthComponent>();
        health.current = 0;
        health.max = 3;

        NexAur::GameStateModel game_state;
        game_state.resetForScene(scene);
        game_state.updateRules(scene);

        expect(game_state.getSnapshot().state == NexAur::GameState::GameOver, "Dead player should enter GameOver.");
    }

    if (!success) {
        std::cerr << "Runtime game flow smoke failed: " << failure << std::endl;
        return 1;
    }

    std::cout << "Runtime game flow smoke passed." << std::endl;
    return 0;
}

int runPrefabFactorySmoke() {
    bool success = true;
    std::string failure;

    auto expect = [&](bool condition, const std::string& message) {
        if (!success) {
            return;
        }
        success = expectGameplay(condition, message, failure);
    };

    {
        NexAur::SceneV2 scene;

        NexAur::PlayerSpawnDesc player_desc;
        player_desc.name = "FactoryPlayer";
        player_desc.transform.position = glm::vec3{ 1.0f, 0.0f, 2.0f };
        player_desc.move_speed = 7.0f;
        player_desc.health = 5;
        player_desc.collider_radius = 0.75f;
        NexAur::Entity player = NexAur::PrefabFactory::createPlayer(scene, player_desc);

        expect(player && player.hasComponent<NexAur::PlayerComponent>(), "Factory player should have PlayerComponent.");
        expect(player.hasComponent<NexAur::VelocityComponent>(), "Factory player should have VelocityComponent.");
        expect(player.hasComponent<NexAur::HealthComponent>(), "Factory player should have HealthComponent.");
        expect(player.hasComponent<NexAur::SphereColliderComponent>(), "Factory player should have SphereColliderComponent.");
        expect(player.hasComponent<NexAur::CollisionFilterComponent>(), "Factory player should have CollisionFilterComponent.");
        expect(!player.hasComponent<NexAur::MeshRendererComponent>(), "Factory player should not force a mesh without model asset.");
        expect(nearlyEqual(player.getComponent<NexAur::PlayerComponent>().move_speed, 7.0f), "Factory player move speed is invalid.");
        expect(player.getComponent<NexAur::HealthComponent>().current == 5, "Factory player health is invalid.");
        expect(nearlyEqual(player.getComponent<NexAur::SphereColliderComponent>().radius, 0.75f), "Factory player collider radius is invalid.");
        expect(
            nearlyEqualVec3(player.getComponent<NexAur::TransformComponent>().translation, glm::vec3{ 1.0f, 0.0f, 2.0f }),
            "Factory player transform is invalid.");

        NexAur::EnemySpawnDesc enemy_desc;
        enemy_desc.name = "FactoryEnemy";
        enemy_desc.transform.position = glm::vec3{ 10.0f, 0.0f, 0.0f };
        enemy_desc.move_speed = 3.5f;
        enemy_desc.health = 2;
        NexAur::Entity enemy = NexAur::PrefabFactory::createEnemy(scene, enemy_desc);
        expect(enemy && enemy.hasComponent<NexAur::EnemyComponent>(), "Factory enemy should have EnemyComponent.");
        expect(enemy.hasComponent<NexAur::VelocityComponent>(), "Factory enemy should have VelocityComponent.");
        expect(enemy.getComponent<NexAur::HealthComponent>().max == 2, "Factory enemy health is invalid.");
        expect(nearlyEqual(enemy.getComponent<NexAur::EnemyComponent>().move_speed, 3.5f), "Factory enemy move speed is invalid.");

        NexAur::CollectibleSpawnDesc collectible_desc;
        collectible_desc.name = "FactoryCollectible";
        collectible_desc.transform.position = glm::vec3{ 4.0f, 0.0f, 0.0f };
        collectible_desc.score = 12;
        collectible_desc.collider_radius = 0.4f;
        NexAur::Entity collectible = NexAur::PrefabFactory::createCollectible(scene, collectible_desc);
        expect(collectible && collectible.hasComponent<NexAur::CollectibleComponent>(), "Factory collectible should have CollectibleComponent.");
        expect(collectible.hasComponent<NexAur::TriggerComponent>(), "Factory collectible should have TriggerComponent.");
        expect(collectible.getComponent<NexAur::CollectibleComponent>().score == 12, "Factory collectible score is invalid.");

        NexAur::ProjectileSpawnDesc projectile_desc;
        projectile_desc.name = "FactoryProjectile";
        projectile_desc.transform.position = glm::vec3{ 0.0f, 0.0f, -4.0f };
        projectile_desc.velocity = glm::vec3{ 1.0f, 2.0f, -9.0f };
        projectile_desc.lifetime_seconds = 2.5f;
        projectile_desc.damage = 4;
        projectile_desc.collider_radius = 0.25f;
        NexAur::Entity projectile = NexAur::PrefabFactory::createProjectile(scene, projectile_desc);
        expect(projectile && projectile.hasComponent<NexAur::ProjectileComponent>(), "Factory projectile should have ProjectileComponent.");
        expect(projectile.hasComponent<NexAur::VelocityComponent>(), "Factory projectile should have VelocityComponent.");
        expect(projectile.hasComponent<NexAur::LifetimeComponent>(), "Factory projectile should have LifetimeComponent.");
        expect(projectile.hasComponent<NexAur::TriggerComponent>(), "Factory projectile should have TriggerComponent.");
        expect(projectile.getComponent<NexAur::ProjectileComponent>().damage == 4, "Factory projectile damage is invalid.");
        expect(
            nearlyEqualVec3(projectile.getComponent<NexAur::VelocityComponent>().velocity, glm::vec3{ 1.0f, 2.0f, -9.0f }),
            "Factory projectile velocity is invalid.");
        expect(nearlyEqual(projectile.getComponent<NexAur::LifetimeComponent>().seconds, 2.5f), "Factory projectile lifetime is invalid.");

        const std::filesystem::path smoke_path =
            std::filesystem::path(ENGINE_ROOT_DIR) / "build" / "prefab_factory_smoke.nxscene";

        NexAur::SceneSerializer serializer(NexAur::AssetManager::getInstance());
        const NexAur::SceneSerializationResult save_result = serializer.save(scene, smoke_path);
        if (!save_result) {
            success = false;
            failure = save_result.message;
        } else {
            const NexAur::SceneLoadResult load_result = serializer.load(smoke_path);
            if (!load_result) {
                success = false;
                failure = load_result.message;
            } else {
                const NexAur::PlayerComponent* loaded_player =
                    findComponentByName<NexAur::PlayerComponent>(load_result.scene, "FactoryPlayer");
                const NexAur::CollectibleComponent* loaded_collectible =
                    findComponentByName<NexAur::CollectibleComponent>(load_result.scene, "FactoryCollectible");
                const NexAur::ProjectileComponent* loaded_projectile =
                    findComponentByName<NexAur::ProjectileComponent>(load_result.scene, "FactoryProjectile");
                const NexAur::VelocityComponent* loaded_projectile_velocity =
                    findComponentByName<NexAur::VelocityComponent>(load_result.scene, "FactoryProjectile");
                const NexAur::LifetimeComponent* loaded_projectile_lifetime =
                    findComponentByName<NexAur::LifetimeComponent>(load_result.scene, "FactoryProjectile");

                expect(loaded_player && nearlyEqual(loaded_player->move_speed, 7.0f), "Loaded factory player is invalid.");
                expect(loaded_collectible && loaded_collectible->score == 12, "Loaded factory collectible is invalid.");
                expect(loaded_projectile && loaded_projectile->damage == 4, "Loaded factory projectile is invalid.");
                expect(
                    loaded_projectile_velocity
                        && nearlyEqualVec3(loaded_projectile_velocity->velocity, glm::vec3{ 1.0f, 2.0f, -9.0f }),
                    "Loaded factory projectile velocity is invalid.");
                expect(
                    loaded_projectile_lifetime && nearlyEqual(loaded_projectile_lifetime->seconds, 2.5f),
                    "Loaded factory projectile lifetime is invalid.");
            }
        }

        std::error_code remove_error;
        std::filesystem::remove(smoke_path, remove_error);
    }

    {
        NexAur::SceneV2 scene;

        NexAur::PlayerSpawnDesc player_desc;
        player_desc.collider_radius = 1.0f;
        NexAur::PrefabFactory::createPlayer(scene, player_desc);

        NexAur::CollectibleSpawnDesc collectible_desc;
        collectible_desc.score = 10;
        collectible_desc.collider_radius = 0.5f;
        collectible_desc.transform.position = glm::vec3{ 0.75f, 0.0f, 0.0f };
        NexAur::PrefabFactory::createCollectible(scene, collectible_desc);

        NexAur::GameStateModel game_state;
        game_state.resetForScene(scene);

        NexAur::TriggerOverlapSystem trigger_system;
        NexAur::CollectibleSystem collectible_system;
        trigger_system.update(scene);
        const NexAur::CollectibleFrameResult result =
            collectible_system.update(scene, trigger_system.getFrame());
        game_state.addCollected(result.collected_count, result.collected_score);
        game_state.updateRules(scene);

        expect(result.collected_count == 1, "Factory collectible should be collected by factory player overlap.");
        expect(game_state.getSnapshot().score == 10, "Factory collectible score should flow into GameState.");
        expect(game_state.getSnapshot().state == NexAur::GameState::Victory, "Factory collectible pickup should enter Victory.");
    }

    if (!success) {
        std::cerr << "Prefab factory smoke failed: " << failure << std::endl;
        return 1;
    }

    std::cout << "Prefab factory smoke passed." << std::endl;
    return 0;
}

int runEditorConfigSmoke() {
    bool success = true;
    std::string failure;

    auto expect = [&](bool condition, const std::string& message) {
        if (condition || !success) {
            return;
        }

        success = false;
        failure = message;
    };

    const std::filesystem::path smoke_dir = std::filesystem::path("saved") / "editor_config_smoke";
    const std::filesystem::path config_path = smoke_dir / "editor_config.json";

    std::error_code remove_error;
    std::filesystem::remove_all(smoke_dir, remove_error);

    NexAur::EditorCommandRegistry commands;
    commands.registerCommand({
        "test.command",
        "Test Command",
        "Command used by editor config smoke.",
        "Ctrl+S",
        ImGuiMod_Ctrl | ImGuiKey_S,
        {},
        {},
        []() {}
    });

    NexAur::EditorConfigData defaults =
        NexAur::EditorConfigStore::loadOrCreateDefault(commands, config_path);
    expect(std::filesystem::exists(config_path), "Editor config smoke should create a default config file.");
    expect(defaults.shortcuts.size() == 1, "Editor config smoke should capture default shortcuts.");
    expect(defaults.theme_variant == "ModernBlack", "Editor config smoke should default to ModernBlack theme.");

    defaults.theme_variant = "Graphite";
    defaults.viewport_camera_speed = 12.5f;
    defaults.panel_open["Console"] = false;
    defaults.panel_open["Project"] = true;
    defaults.shortcuts = {
        {
            "test.command",
            "Ctrl+Alt+P",
            ImGuiMod_Ctrl | ImGuiMod_Alt | ImGuiKey_P
        }
    };
    expect(NexAur::EditorConfigStore::save(defaults, config_path), "Editor config smoke should save config JSON.");

    NexAur::EditorCommandRegistry reloaded_commands;
    reloaded_commands.registerCommand({
        "test.command",
        "Test Command",
        "Command used by editor config smoke.",
        "Ctrl+S",
        ImGuiMod_Ctrl | ImGuiKey_S,
        {},
        {},
        []() {}
    });
    reloaded_commands.registerCommand({
        "new.command",
        "New Command",
        "Command added after the config file existed.",
        "F5",
        ImGuiKey_F5,
        {},
        {},
        []() {}
    });

    NexAur::EditorConfigData loaded =
        NexAur::EditorConfigStore::loadOrCreateDefault(reloaded_commands, config_path);
    NexAur::EditorConfigStore::applyShortcuts(reloaded_commands, loaded);

    const NexAur::EditorCommand* test_command = reloaded_commands.find("test.command");
    const NexAur::EditorCommand* new_command = reloaded_commands.find("new.command");

    expect(nearlyEqual(loaded.viewport_camera_speed, 12.5f), "Editor config smoke should round-trip camera speed.");
    expect(loaded.theme_variant == "ModernBlack", "Editor config smoke should migrate the legacy Graphite theme.");
    expect(loaded.panel_open["Console"] == false, "Editor config smoke should round-trip closed panel state.");
    expect(loaded.panel_open["Project"] == true, "Editor config smoke should round-trip open panel state.");
    expect(test_command && test_command->shortcut == (ImGuiMod_Ctrl | ImGuiMod_Alt | ImGuiKey_P),
        "Editor config smoke should apply shortcut overrides.");
    expect(test_command && test_command->shortcut_text == "Ctrl+Alt+P",
        "Editor config smoke should apply shortcut text overrides.");
    expect(new_command && new_command->shortcut == ImGuiKey_F5,
        "Editor config smoke should preserve new default command shortcuts.");

    std::error_code cleanup_error;
    std::filesystem::remove_all(smoke_dir, cleanup_error);

    if (!success) {
        std::cerr << "Editor config smoke failed: " << failure << std::endl;
        return 1;
    }

    std::cout << "Editor config smoke passed." << std::endl;
    return 0;
}

namespace {
    struct SmokeTestEntry {
        const char* argument = nullptr;
        const char* name = nullptr;
        int (*run)() = nullptr;
    };

    constexpr SmokeTestEntry kSmokeTests[] = {
        { "--scene-serializer-smoke", "SceneSerializer", runSceneSerializerSmoke },
        { "--audio-smoke", "Audio", runAudioSmoke },
        { "--input-action-smoke", "InputAction", runInputActionSmoke },
        { "--render-settings-smoke", "RenderSettings", runRenderSettingsSmoke },
        { "--editor-config-smoke", "EditorConfig", runEditorConfigSmoke },
        { "--material-asset-smoke", "MaterialAsset", runMaterialAssetSmoke },
        { "--gltf-model-import-smoke", "GltfModelImport", runGltfModelImportSmoke },
        { "--assimp-fallback-smoke", "AssimpFallback", runAssimpFallbackSmoke },
        { "--tiny-gltf-metadata-smoke", "TinyGltfMetadata", runTinyGltfMetadataSmoke },
        { "--tiny-gltf-geometry-smoke", "TinyGltfGeometry", runTinyGltfGeometrySmoke },
        { "--tiny-gltf-material-smoke", "TinyGltfMaterial", runTinyGltfMaterialSmoke },
        { "--damaged-helmet-reference-smoke", "DamagedHelmetReference", runDamagedHelmetReferenceSmoke },
        { "--tiny-gltf-scene-smoke", "TinyGltfScene", runTinyGltfSceneSmoke },
        { "--gameplay-systems-smoke", "GameplaySystems", runGameplaySystemsSmoke },
        { "--physics-trigger-smoke", "PhysicsTrigger", runPhysicsTriggerSmoke },
        { "--runtime-camera-smoke", "RuntimeCamera", runRuntimeCameraSmoke },
        { "--runtime-game-flow-smoke", "RuntimeGameFlow", runRuntimeGameFlowSmoke },
        { "--prefab-factory-smoke", "PrefabFactory", runPrefabFactorySmoke },
    };

    bool isSmokeAllCommand(std::string_view argument) {
        return argument == "--smoke-all";
    }
}

namespace NexAur::Sandbox {
    bool isSmokeCommand(int argc, char** argv) {
        if (argc <= 1 || !argv || !argv[1]) {
            return false;
        }

        const std::string_view argument{ argv[1] };
        if (isSmokeAllCommand(argument)) {
            return true;
        }

        for (const SmokeTestEntry& test : kSmokeTests) {
            if (argument == test.argument) {
                return true;
            }
        }

        return false;
    }

    int runAllSmokeTests() {
        for (const SmokeTestEntry& test : kSmokeTests) {
            std::cout << "[Smoke] " << test.name << std::endl;
            const int result = test.run();
            if (result != 0) {
                std::cerr << "[Smoke] " << test.name << " failed with code " << result << "." << std::endl;
                return result;
            }
        }

        std::cout << "All Sandbox smoke tests passed." << std::endl;
        return 0;
    }

    int runSmokeCommand(int argc, char** argv) {
        if (argc <= 1 || !argv || !argv[1]) {
            return 1;
        }

        const std::string_view argument{ argv[1] };
        if (isSmokeAllCommand(argument)) {
            return runAllSmokeTests();
        }

        for (const SmokeTestEntry& test : kSmokeTests) {
            if (argument == test.argument) {
                return test.run();
            }
        }

        std::cerr << "Unknown Sandbox smoke command: " << argument << std::endl;
        return 1;
    }
} // namespace NexAur::Sandbox
