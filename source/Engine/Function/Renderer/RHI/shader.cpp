#include "pch.h"
#include "shader.h"
#include "Core/Log/log_system.h"
#include "Function/Renderer/Platform/OpenGL/opengl_shader.h"

namespace NexAur {
    std::shared_ptr<Shader> Shader::create(const std::string& filepath) {
        return std::make_shared<OpenGLShader>(filepath);
    }

    std::shared_ptr<Shader> Shader::create(const std::string& name, const std::string& vertexSrc, const std::string& fragmentSrc) {
        return std::make_shared<OpenGLShader>(name, vertexSrc, fragmentSrc);
    }

    /* ---------------------------------- ShaderLibrary -----------------------------------------  */
    void ShaderLibrary::add(const std::string& name, const std::shared_ptr<Shader>& shader) {
        if (exists(name)) {
            NX_CORE_WARN("Shader already exists!");
            return;
        }

        m_Shaders[name] = shader;
    }

    void ShaderLibrary::add(const std::shared_ptr<Shader>& shader) {
        std::string name = shader->getName();
        add(name, shader);
    }

    std::shared_ptr<NexAur::Shader> ShaderLibrary::load(const std::string& filepath) {
        std::shared_ptr<Shader> shader = Shader::create(filepath);
        add(shader);
        return shader;
    }

    std::shared_ptr<NexAur::Shader> ShaderLibrary::load(const std::string& name, const std::string& filepath) {
        std::shared_ptr<Shader> shader = Shader::create(filepath);
        add(name, shader);
        return shader;
    }

    std::shared_ptr<NexAur::Shader> ShaderLibrary::get(const std::string& name) {
        if (!exists(name)) {
            NX_CORE_WARN("Shader not found!");
            return nullptr;
        }

        return m_Shaders[name];
    }

    bool ShaderLibrary::exists(const std::string& name) const {
        return m_Shaders.find(name) != m_Shaders.end();
    }
} // namespace NexAur
