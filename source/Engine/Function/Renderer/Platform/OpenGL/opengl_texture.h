#pragma once

#include "glfw/glfw3.h"

#include "Core/Base.h"
#include "Function/Renderer/RHI/texture.h"

namespace NexAur {
    class OpenGLTexture2D : public Texture2D {
    public:
        OpenGLTexture2D(const TextureSpecification& specification);
        OpenGLTexture2D(const std::string& path);
        virtual ~OpenGLTexture2D() override;

        virtual uint32_t getWidth() const override;
        virtual uint32_t getHeight() const override;
        virtual uint32_t getRendererID() const override;
        virtual const std::string& getPath() const override;
        virtual const TextureSpecification& getSpecification() const override;

        virtual void setData(void* data, uint32_t size) override;
        virtual void bind(uint32_t slot = 0) const override;
        virtual bool isLoaded() const override;

        virtual bool operator==(const Texture& other) const override;

    private:
        TextureSpecification m_Specification;
        uint32_t m_RendererID;

        uint32_t m_width;
        uint32_t m_height;

        std::string m_Path;

        GLenum m_internal_format;   // 显存中的格式
        GLenum m_data_format;       // 内存中的格式

        bool m_IsLoaded;
    };


    class OpenGLTextureCubeMap : public TextureCubeMap {
    public:
        OpenGLTextureCubeMap(const TextureSpecification& specification);
        OpenGLTextureCubeMap(const std::string& path);
        virtual ~OpenGLTextureCubeMap() override;

        virtual uint32_t getWidth() const override;
        virtual uint32_t getHeight() const override;
        virtual uint32_t getRendererID() const override;
        virtual const std::string& getPath() const override;
        virtual const TextureSpecification& getSpecification() const override;

        void setData(CubeMapFace face, void* data, uint32_t size);
        virtual void setData(void* data, uint32_t size) override;
        virtual void bind(uint32_t slot = 0) const override;
        virtual bool isLoaded() const override;

        virtual bool operator==(const Texture& other) const override;

    private:
        void loadFromPaths(std::vector<std::string> paths);
        std::vector<std::string> createFromDirectory(const std::string& dir);

    private:
        TextureSpecification m_Specification;
        uint32_t m_RendererID;

        uint32_t m_width;
        uint32_t m_height;

        std::string m_Path;

        GLenum m_internal_format;   // 显存中的格式
        GLenum m_data_format;       // 内存中的格式

        bool m_IsLoaded;
    };
} // namespace NexAur
