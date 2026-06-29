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
} // namespace

void setupScene() {
    NexAur::SceneTestClass scene_test;
    // 材质测试
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

    // DamagedHelmet 是本地可选样例资产，缺失时跳过，避免新克隆仓库启动 Sandbox 时报错。
    const std::string damaged_helmet_path = NX_ASSET("assets/models/DamagedHelmet/DamagedHelmet.gltf");
    if (std::filesystem::exists(damaged_helmet_path)) {
        scene_test.addModelEntity("DamagedHelmet", damaged_helmet_path, glm::vec3(5.0f, 0.0f, 4.0f));
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
                !hasEntityNamed(load_result.scene, "PointLight")) {
                success = false;
                failure = "SceneSerializer smoke failed: loaded scene is missing default entities.";
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
    settings.post_process.tone_mapping_mode = NexAur::RenderToneMappingMode::None;
    settings.post_process.exposure = 1.75f;
    settings.post_process.bloom_enabled = false;
    settings.post_process.bloom_intensity = 0.25f;
    settings.post_process.bloom_scatter = 0.5f;
    settings.post_process.bloom_radius = 1.25f;
    settings.ibl_debug.mode = NexAur::RenderIblDebugMode::SpecularIbl;
    settings.ibl_debug.prefilter_mip = 3.0f;
    render_context.setRenderSettings(settings);
    render_context.getWriteData().render_settings = render_context.getRenderSettings();
    render_context.swapBuffers();

    const NexAur::RenderPostProcessSettings& first_post_process =
        render_context.getReadData().render_settings.post_process;
    const NexAur::RenderIblDebugSettings& first_ibl_debug =
        render_context.getReadData().render_settings.ibl_debug;
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
        first_ibl_debug.mode == NexAur::RenderIblDebugMode::SpecularIbl &&
        nearlyEqual(first_ibl_debug.prefilter_mip, 3.0f),
        "RenderSettings smoke failed: IBL debug settings did not reach the read packet.");

    settings.post_process.tone_mapping_mode = NexAur::RenderToneMappingMode::ACES;
    settings.post_process.exposure = 0.5f;
    settings.post_process.bloom_enabled = true;
    settings.post_process.bloom_intensity = 0.08f;
    settings.post_process.bloom_scatter = 0.7f;
    settings.post_process.bloom_radius = 1.0f;
    settings.ibl_debug.mode = NexAur::RenderIblDebugMode::FinalLit;
    settings.ibl_debug.prefilter_mip = 0.0f;
    render_context.setRenderSettings(settings);
    render_context.getWriteData().render_settings = render_context.getRenderSettings();
    render_context.swapBuffers();

    const NexAur::RenderPostProcessSettings& second_post_process =
        render_context.getReadData().render_settings.post_process;
    const NexAur::RenderIblDebugSettings& second_ibl_debug =
        render_context.getReadData().render_settings.ibl_debug;
    expect(
        second_post_process.tone_mapping_mode == NexAur::RenderToneMappingMode::ACES,
        "RenderSettings smoke failed: ACES mode did not reach the read packet.");
    expect(
        nearlyEqual(second_post_process.exposure, 0.5f),
        "RenderSettings smoke failed: updated exposure did not reach the read packet.");
    expect(second_post_process.bloom_enabled, "RenderSettings smoke failed: updated bloom enabled did not reach the read packet.");
    expect(
        nearlyEqual(second_post_process.bloom_intensity, 0.08f) &&
        nearlyEqual(second_post_process.bloom_scatter, 0.7f) &&
        nearlyEqual(second_post_process.bloom_radius, 1.0f),
        "RenderSettings smoke failed: updated bloom parameters did not reach the read packet.");
    expect(
        second_ibl_debug.mode == NexAur::RenderIblDebugMode::FinalLit &&
        nearlyEqual(second_ibl_debug.prefilter_mip, 0.0f),
        "RenderSettings smoke failed: updated IBL debug settings did not reach the read packet.");

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
        { "--material-asset-smoke", "MaterialAsset", runMaterialAssetSmoke },
        { "--gltf-model-import-smoke", "GltfModelImport", runGltfModelImportSmoke },
        { "--tiny-gltf-metadata-smoke", "TinyGltfMetadata", runTinyGltfMetadataSmoke },
        { "--tiny-gltf-geometry-smoke", "TinyGltfGeometry", runTinyGltfGeometrySmoke },
        { "--tiny-gltf-material-smoke", "TinyGltfMaterial", runTinyGltfMaterialSmoke },
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
