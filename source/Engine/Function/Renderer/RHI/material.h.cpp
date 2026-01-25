#include "material.h"

namespace NexAur {
    Material::Material(const std::shared_ptr<Shader>& shader)
        : m_shader(shader) {}

    void Material::setInt(const std::string& name, int value) {
        m_int_uniform[name] = value;
    }

    void Material::setFloat(const std::string& name, float value) {
        m_float_uniform[name] = value;
    }

    void Material::setFloat3(const std::string& name, const glm::vec3& value) {
        m_float3_uniform[name] = value;
    }

    void Material::setFloat4(const std::string& name, const glm::vec4& value) {
        m_float4_uniform[name] = value;
    }

    void Material::setMat4(const std::string& name, const glm::mat4& value) {
        m_mat4_uniform[name] = value;
    }

    void Material::setTexture(const std::string& name, const std::shared_ptr<Texture>& texture) {
        m_textures[name] = texture;
    }

    void Material::bind() {
        // 启用着色器
        m_shader->bind();

        // unifrom绑定
        for (const auto& [name, value] : m_int_uniform)  m_shader->setInt(name, value);
        for (const auto& [name, value] : m_float_uniform) m_shader->setFloat(name, value);
        for (const auto& [name, value] : m_float3_uniform) m_shader->setFloat3(name, value);
        for (const auto& [name, value] : m_float4_uniform) m_shader->setFloat4(name, value);
        for (const auto& [name, value] : m_mat4_uniform) m_shader->setMat4(name, value);
        
        // 纹理绑定
        bindTextures();
    }

    void Material::bindTextures() {
        uint32_t slot = 0;
        for (const auto& [name, texture] : m_textures) {
            texture->bind(slot);            // 激活纹理单元
            m_shader->setInt(name, slot);   // 绑定纹理单元
            slot++;
        }
    }

} // namespace NexAur