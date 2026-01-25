#pragma once
#include <unordered_map>

#include "glm/glm.hpp"

#include "Core/Base.h"
#include "texture.h"
#include "shader.h"

namespace NexAur {
    class Material {
    public:
        Material(const std::shared_ptr<Shader>& shader);
        ~Material() = default;

        void bind();

        void setInt(const std::string& name, int value);
        void setFloat(const std::string& name, float value);
        void setFloat3(const std::string& name, const glm::vec3& value);
        void setFloat4(const std::string& name, const glm::vec4& value);
        void setMat4(const std::string& name, const glm::mat4& value);
        void setTexture(const std::string& name, const std::shared_ptr<Texture>& texture);

        std::shared_ptr<Shader> getShader() const { return m_shader; }

    private:
        void bindTextures();    // 绑定纹理辅助函数

    private:
        std::shared_ptr<Shader> m_shader;

        std::unordered_map<std::string, int> m_int_uniform;
        std::unordered_map<std::string, float> m_float_uniform;
        std::unordered_map<std::string, glm::vec3> m_float3_uniform;
        std::unordered_map<std::string, glm::vec4> m_float4_uniform;
        std::unordered_map<std::string, glm::mat4> m_mat4_uniform;
        std::unordered_map<std::string, std::shared_ptr<Texture>> m_textures;
    };
} // namespace NexAur
