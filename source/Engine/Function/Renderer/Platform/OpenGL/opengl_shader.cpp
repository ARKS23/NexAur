#include "pch.h"

#include <fstream>

#include "opengl_shader.h"

namespace NexAur {
    static GLenum shaderTypeFromString(const std::string& type) {
        if (type == "vertex")
            return GL_VERTEX_SHADER;
        if (type == "fragment" || type == "pixel")
            return GL_FRAGMENT_SHADER;

        NX_CORE_ERROR("shaderTypeFromString: Unknown shader type!");
        return 0;
    }

    OpenGLShader::OpenGLShader(const std::string& filepath) {
        std::string source = readFile(filepath);
        m_openGLShaderSources = preProcess(source);
        createProgram();

        // 从文件路径中提取名称
        size_t last_slash = filepath.find_last_of("/\\");
        last_slash = last_slash == std::string::npos ? 0 : last_slash + 1;
        size_t last_dot = filepath.rfind('.');
        size_t count = last_dot == std::string::npos ? filepath.size() - last_slash : last_dot - last_slash;
        m_name = filepath.substr(last_slash, count);
    }

    OpenGLShader::OpenGLShader(const std::string& name, const std::string& vertexSrc, const std::string& fragmentSrc) : m_name(name) {
        m_openGLShaderSources[GL_VERTEX_SHADER] = vertexSrc;
        m_openGLShaderSources[GL_FRAGMENT_SHADER] = fragmentSrc;
        createProgram();
    }

    OpenGLShader::~OpenGLShader() {
        glDeleteProgram(m_ID);
    }

    void OpenGLShader::bind() const {
        glUseProgram(m_ID);
    }

    void OpenGLShader::unbind() const {
        glUseProgram(0);
    }

    void OpenGLShader::setInt(const std::string& name, int value) {
        GLint location = glGetUniformLocation(m_ID, name.c_str());
        glUniform1i(location, value);
    }

    void OpenGLShader::setIntArray(const std::string& name, int* values, uint32_t count) {
        GLint location = glGetUniformLocation(m_ID, name.c_str());
        glUniform1iv(location, count, values);
    }

    void OpenGLShader::setFloat(const std::string& name, float value) {
        GLint location = glGetUniformLocation(m_ID, name.c_str());
        glUniform1f(location, value);
    }

    void OpenGLShader::setFloat2(const std::string& name, const glm::vec2& value) {
        GLint location = glGetUniformLocation(m_ID, name.c_str());
        glUniform2f(location, value.x, value.y);
    }

    void OpenGLShader::setFloat3(const std::string& name, const glm::vec3& value) {
        GLint location = glGetUniformLocation(m_ID, name.c_str());
        glUniform3f(location, value.x, value.y, value.z);
    }

    void OpenGLShader::setFloat4(const std::string& name, const glm::vec4& value) {
        GLint location = glGetUniformLocation(m_ID, name.c_str());
        glUniform4f(location, value.x, value.y, value.z, value.w);
    }

    void OpenGLShader::setMat4(const std::string& name, const glm::mat4& value) {
        GLint location = glGetUniformLocation(m_ID, name.c_str());
        glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(value));
    }

    std::string OpenGLShader::readFile(const std::string& filepath) {
        std::string result;
        std::ifstream in(filepath, std::ios::in | std::ios::binary);

        if (in) {
            in.seekg(0, std::ios::end);
            size_t size = in.tellg();   // 获取文件大小
            
            if (size != -1) {
                result.resize(size);
                in.seekg(0, std::ios::beg); // 回到开头
                in.read(&result[0], size);  // 读取文件内容
                in.close();
            }
            else {
                NX_CORE_ERROR("Could not read file {0}", filepath);
            }
        }
        else {
            NX_CORE_ERROR("Could not open file {0}", filepath);
        }

        return result;
    }

    void OpenGLShader::createProgram() {
        unsigned int vertex_shader, fragment_shader;

        const char* vertexSource = m_openGLShaderSources[GL_VERTEX_SHADER].c_str();
        vertex_shader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertex_shader, 1, &vertexSource, nullptr);
        glCompileShader(vertex_shader);
        checkShaderCompile(vertex_shader, "Vertex Shader");

        const char* fragmentSource = m_openGLShaderSources[GL_FRAGMENT_SHADER].c_str();
        fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragment_shader, 1, &fragmentSource, nullptr);
        glCompileShader(fragment_shader);
        checkShaderCompile(fragment_shader, "Fragment Shader");

        m_ID = glCreateProgram();
        glAttachShader(m_ID, vertex_shader);
        glAttachShader(m_ID, fragment_shader);
        glLinkProgram(m_ID);
        int link_success;
        glGetProgramiv(m_ID, GL_LINK_STATUS, &link_success);
        if (!link_success) {
            char infoLog[512];
            glGetProgramInfoLog(m_ID, 512, nullptr, infoLog);
            NX_CORE_ERROR("Shader Program Linking Failed: {0}", infoLog);
        }

        glDeleteShader(vertex_shader);
        glDeleteShader(fragment_shader);
    }

    void OpenGLShader::checkShaderCompile(unsigned int id, const std::string& msg) {
        int success;
        glGetShaderiv(id, GL_COMPILE_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetShaderInfoLog(id, 512, nullptr, infoLog);
            NX_CORE_ERROR("{0} Compilation Failed: {1}", msg, infoLog);
        }
    }

    std::unordered_map<GLenum, std::string> OpenGLShader::preProcess(const std::string& source) {
        std::unordered_map<GLenum, std::string> shader_sources;

        const char* type_token = "#type";
        size_t type_token_length = strlen(type_token);
        size_t pos = source.find(type_token, 0); // 查找第一个#type

        if (pos == std::string::npos) {
            NX_CORE_ERROR("Shader PreProcess failed: No '#type' token found in shader file! Check spelling?");
            return shader_sources;
        }

        while (pos != std::string::npos) {
            // 找到目前行的末尾
            size_t eol = source.find_first_of("\r\n", pos);
            NX_CORE_ASSERT(eol != std::string::npos, "Syntax error");

            // 提取类型名称
            size_t begin = pos + type_token_length + 1; // 跳过#type
            std::string type = source.substr(begin, eol - begin);
            NX_CORE_ASSERT(shaderTypeFromString(type), "Invalid shader type specification");

            // 查找下一个#type的位置或文件末尾
            size_t nextLinePos = source.find_first_not_of("\r\n", eol); // 跳过换行符
            NX_CORE_ASSERT(nextLinePos != std::string::npos, "Syntax error");
            pos = source.find(type_token, nextLinePos); // 找下一个 #type

            // 获取源码片段
            shader_sources[shaderTypeFromString(type)] = (pos == std::string::npos) ? source.substr(nextLinePos) : source.substr(nextLinePos, pos - nextLinePos);
        }

        return shader_sources;
    }
} // namespace NexAur
