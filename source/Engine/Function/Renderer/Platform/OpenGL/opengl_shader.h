#pragma once

#include "glfw/glfw3.h"

#include "Core/Base.h"
#include "Function/Renderer/RHI/shader.h"

namespace NexAur {
    class OpenGLShader : public Shader {
    public:
        OpenGLShader(const std::string& filepath);
        OpenGLShader(const std::string& name, const std::string& vertexSrc, const std::string& fragmentSrc);
        virtual ~OpenGLShader() override;

        virtual void bind() const override;
        virtual void unbind() const override;
        virtual const std::string& getName() const override { return m_name; }

        virtual void setInt(const std::string& name, int value) override;
        virtual void setIntArray(const std::string& name, int* values, uint32_t count) override;
        virtual void setFloat(const std::string& name, float value) override;
        virtual void setFloat2(const std::string& name, const glm::vec2& value) override;
        virtual void setFloat3(const std::string& name, const glm::vec3& value) override;
        virtual void setFloat4(const std::string& name, const glm::vec4& value) override;
        virtual void setMat4(const std::string& name, const glm::mat4&  value) override;

    private:
        std::string readFile(const std::string& filepath);
        std::unordered_map<GLenum, std::string> preProcess(const std::string& source);
        void createProgram();
        void checkShaderCompile(unsigned int id, const std::string& msg);

    private:
        uint32_t m_ID;
        std::string m_name;
        std::unordered_map<GLenum, std::string> m_openGLShaderSources;
    };
} // namespace NexAur
