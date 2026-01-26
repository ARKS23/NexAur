#include "pch.h"

#include "stb_image.h"

#include "opengl_texture.h"

namespace NexAur {
    // 辅助函数：将引擎的 ImageFormat 转换为 OpenGL 的格式枚举 (显存格式)
    static GLenum GLInternalFormatFromImageFormat(ImageFormat format) {
        switch (format) {
            case ImageFormat::R8:          return GL_RED;
            case ImageFormat::RGB8:        return GL_RGB;
            case ImageFormat::RGBA8:       return GL_RGBA;
            case ImageFormat::RGB16F:      return GL_RGB16F;
            case ImageFormat::RGBA16F:     return GL_RGBA16F;
            case ImageFormat::RGBA32F:     return GL_RGBA32F;
            case ImageFormat::Depth24Stencil8: return GL_DEPTH24_STENCIL8;
            case ImageFormat::Depth32F:    return GL_DEPTH_COMPONENT32F;
            default:
                NX_CORE_ERROR("Unsupported ImageFormat!");
                return 0;
        }
    }

    // 辅助函数：将引擎的 ImageFormat 转换为 OpenGL 的数据格式枚举 (内存格式)
    static GLenum GLDataFormatFromImageFormat(ImageFormat format) {
        switch (format) {
            case ImageFormat::R8:          return GL_RED;
            case ImageFormat::RGB8:        return GL_RGB;
            case ImageFormat::RGBA8:       return GL_RGBA;
            case ImageFormat::RGB16F:      return GL_RGB;
            case ImageFormat::RGBA16F:     return GL_RGBA;
            case ImageFormat::RGBA32F:     return GL_RGBA;
            case ImageFormat::Depth24Stencil8: return GL_DEPTH_STENCIL;
            case ImageFormat::Depth32F:    return GL_DEPTH_COMPONENT;
            default:
                NX_CORE_ERROR("Unsupported ImageFormat!");
                return 0;
        }
    }

    // 辅助函数：将引擎的 TextureFilter 转换为 OpenGL 的过滤方式枚举
    static GLenum GLFilterFromTextureFilter(TextureFilter filter) {
        switch (filter) {
            case TextureFilter::None:                 return GL_NEAREST;
            case TextureFilter::Linear:               return GL_LINEAR;
            case TextureFilter::Nearest:              return GL_NEAREST;
            case TextureFilter::NearestMipmapNearest: return GL_NEAREST_MIPMAP_NEAREST;
            case TextureFilter::LinearMipmapNearest:  return GL_LINEAR_MIPMAP_NEAREST;
            case TextureFilter::NearestMipmapLinear:  return GL_NEAREST_MIPMAP_LINEAR;
            case TextureFilter::LinearMipmapLinear:   return GL_LINEAR_MIPMAP_LINEAR;
            default:
                NX_CORE_ERROR("Unsupported TextureFilter!");
                return GL_NEAREST;
        }
    }

    // 辅助函数：将引擎的 TextureWrap 转换为 OpenGL 的环绕方式枚举
    static GLenum GLWrapFromTextureWrap(TextureWrap wrap) {
        switch (wrap) {
            case TextureWrap::None:          return GL_CLAMP_TO_EDGE;
            case TextureWrap::Repeat:        return GL_REPEAT;
            case TextureWrap::ClampToEdge:   return GL_CLAMP_TO_EDGE;
            case TextureWrap::ClampToBorder: return GL_CLAMP_TO_BORDER;
            case TextureWrap::MirroredRepeat:return GL_MIRRORED_REPEAT;
            default:
                NX_CORE_ERROR("Unsupported TextureWrap!");
                return GL_REPEAT;
        }
    }

    OpenGLTexture2D::OpenGLTexture2D(const TextureSpecification& specification)
        : m_Specification(specification), m_width(specification.width), m_height(specification.height), m_Path(""), m_IsLoaded(false) {
        // 数据格式设置  
        m_internal_format = GLInternalFormatFromImageFormat(specification.format);
        m_data_format = GLDataFormatFromImageFormat(specification.format);

        // 创建 OpenGL 纹理对象
        glCreateTextures(GL_TEXTURE_2D, 1, &m_RendererID);
        GLsizei levels = specification.generate_mips ? static_cast<GLsizei>(std::floor(std::log2(std::max(m_width, m_height)))) + 1 : 1;
        glTextureStorage2D(m_RendererID, levels, m_internal_format, m_width, m_height);

        // 过滤方式
        glTextureParameteri(m_RendererID, GL_TEXTURE_MIN_FILTER, GLFilterFromTextureFilter(specification.filter));
        glTextureParameteri(m_RendererID, GL_TEXTURE_MAG_FILTER, GLFilterFromTextureFilter(specification.filter));

        // 环绕方式
        glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_S, GLWrapFromTextureWrap(specification.wrap));
        glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_T, GLWrapFromTextureWrap(specification.wrap));
    }

    OpenGLTexture2D::OpenGLTexture2D(const std::string& path)
        : m_Path(path), m_IsLoaded(false) { 
        int width, height, channels;

        stbi_set_flip_vertically_on_load(true); // OpenGL的纹理坐标系是左下角为原点，而图片通常是右上角为原点，所以需要翻转

        stbi_uc* data = stbi_load(path.c_str(), &width, &height, &channels, 0);

        if (data) {
            m_width = width;
            m_height = height;

            // 根据通道数设置格式
            if (channels == 4) {
                m_internal_format = GL_RGBA8;
                m_data_format = GL_RGBA;
                m_Specification.format = ImageFormat::RGBA8;
            }
            else if (channels == 3) {
                m_internal_format = GL_RGB8;
                m_data_format = GL_RGB;
                m_Specification.format = ImageFormat::RGB8;
            }
            else {
                NX_CORE_ERROR("Unsupported number of channels: {0} in texture: {1}", channels, path);
                stbi_set_flip_vertically_on_load(false);
                stbi_image_free(data);
                return;
            }

            m_Specification.width = m_width;
            m_Specification.height = m_height;

            // 创建 OpenGL 纹理对象
            glCreateTextures(GL_TEXTURE_2D, 1, &m_RendererID);
            glTextureStorage2D(m_RendererID, 1, m_internal_format, m_width, m_height);

            // 默认过滤和环绕方式
            glTextureParameteri(m_RendererID, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTextureParameteri(m_RendererID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_T, GL_REPEAT);

            // 上传数据
            glTextureSubImage2D(m_RendererID, 0, 0, 0, m_width, m_height, m_data_format, GL_UNSIGNED_BYTE, data);

            stbi_image_free(data);
            m_IsLoaded = true;
        }
        else {
            NX_CORE_ERROR("Failed to load texture image from path: {0}", path);
        }

        stbi_set_flip_vertically_on_load(false);
    }

    OpenGLTexture2D::~OpenGLTexture2D() {
        glDeleteTextures(1, &m_RendererID);
    }

    uint32_t OpenGLTexture2D::getWidth() const {
        return m_width;
    }

    uint32_t OpenGLTexture2D::getHeight() const {
        return m_height;
    }

    uint32_t OpenGLTexture2D::getRendererID() const {
        return m_RendererID;
    }

    const std::string& OpenGLTexture2D::getPath() const {
        return m_Path;
    }

    const TextureSpecification& OpenGLTexture2D::getSpecification() const {
        return m_Specification;
    }

    void OpenGLTexture2D::setData(void* data, uint32_t size) {
        uint32_t bpp = m_data_format == GL_RGBA ? 4 : (m_data_format == GL_RGB ? 3 : 1);
        if (size != m_width * m_height * bpp) {
            NX_CORE_ERROR("Data size does not match texture size!");
            return;
        }

        glTextureSubImage2D(m_RendererID, 0, 0, 0, m_width, m_height, m_data_format, GL_UNSIGNED_BYTE, data);
    }

    void OpenGLTexture2D::bind(uint32_t slot) const {
        glBindTextureUnit(slot, m_RendererID);
    }

    bool OpenGLTexture2D::isLoaded() const {
        return m_IsLoaded;
    }

    bool OpenGLTexture2D::operator==(const Texture& other) const {
        return m_RendererID == other.getRendererID();
    }

    /* -------------------------- OpenGLTextureCubeMap --------------------------*/
    OpenGLTextureCubeMap::OpenGLTextureCubeMap(const TextureSpecification& specification)
        : m_Specification(specification), m_width(specification.width), m_height(specification.height), m_Path(""), m_IsLoaded(false) {
        m_internal_format = GLInternalFormatFromImageFormat(specification.format);
        m_data_format = GLDataFormatFromImageFormat(specification.format);

        glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &m_RendererID);
        glTextureStorage2D(m_RendererID, 1, m_internal_format, m_width, m_height);

        // 设置过滤和环绕方式
        glTextureParameteri(m_RendererID, GL_TEXTURE_MIN_FILTER, GLFilterFromTextureFilter(specification.filter));
        glTextureParameteri(m_RendererID, GL_TEXTURE_MAG_FILTER, GLFilterFromTextureFilter(specification.filter));
        glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_S, GLWrapFromTextureWrap(specification.wrap));
        glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_T, GLWrapFromTextureWrap(specification.wrap));
        glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_R, GLWrapFromTextureWrap(specification.wrap));
    }

    OpenGLTextureCubeMap::OpenGLTextureCubeMap(const std::string& path)
        : m_Path(path), m_IsLoaded(false) {
        std::vector<std::string> faces = createFromDirectory(path);
        loadFromPaths(faces);
    }

    OpenGLTextureCubeMap::~OpenGLTextureCubeMap() {
        glDeleteTextures(1, &m_RendererID);
    }

    uint32_t OpenGLTextureCubeMap::getWidth() const {
        return m_width;
    }

    uint32_t OpenGLTextureCubeMap::getHeight() const {
        return m_height;
    }

    uint32_t OpenGLTextureCubeMap::getRendererID() const {
        return m_RendererID;
    }

    const std::string& OpenGLTextureCubeMap::getPath() const {
        return m_Path;
    }

    const TextureSpecification& OpenGLTextureCubeMap::getSpecification() const {
        return m_Specification;
    }

    void OpenGLTextureCubeMap::setData(CubeMapFace face, void* data, uint32_t size) {
        uint32_t bpp = m_data_format == GL_RGBA ? 4 : (m_data_format == GL_RGB ? 3 : 1);
        if (size != m_width * m_height * bpp) {
            NX_CORE_ERROR("Data size does not match cubemap face size!");
            return;
        }

        int layerIndex = static_cast<int>(face);
        glTextureSubImage3D(m_RendererID, 0, 0, 0, layerIndex, m_width, m_height, 1, m_data_format, GL_UNSIGNED_BYTE, data);
    }

    void OpenGLTextureCubeMap::setData(void* data, uint32_t size) {
        // 立方体贴图通常不通过单次调用设置数据，空函数
    }

    void OpenGLTextureCubeMap::bind(uint32_t slot) const {
        glBindTextureUnit(slot, m_RendererID);
    }

    bool OpenGLTextureCubeMap::isLoaded() const {
        return m_IsLoaded;
    }

    bool OpenGLTextureCubeMap::operator==(const Texture& other) const {
        return m_RendererID == other.getRendererID();
    }

    void OpenGLTextureCubeMap::loadFromPaths(std::vector<std::string> paths) {
        int width, height, channels;
        stbi_set_flip_vertically_on_load(false);
        stbi_uc* data = stbi_load(paths[0].c_str(), &width, &height, &channels, 0);
        if (data) {
            m_width = width;
            m_height = height;
            stbi_image_free(data);
            m_IsLoaded = true;
        }
        else {
            NX_CORE_ERROR("Failed to load cubemap texture from path: {0}", paths[0]);
            return;
        }

        if (channels == 4) {
            m_internal_format = GL_RGBA8;
            m_data_format = GL_RGBA;
            m_Specification.format = ImageFormat::RGBA8;
        }
        else if (channels == 3) {
            m_internal_format = GL_RGB8;
            m_data_format = GL_RGB;
            m_Specification.format = ImageFormat::RGB8;
        }
        else {
            NX_CORE_ERROR("Unsupported number of channels: {0} in cubemap texture: {1}", channels, paths[0]);
            return;
        }

        glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &m_RendererID);
        glTextureStorage2D(m_RendererID, 1, m_internal_format, m_width, m_height);
        glTextureParameteri(m_RendererID, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(m_RendererID, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTextureParameteri(m_RendererID, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

        // 加载每个面的数据
        for (uint32_t i = 0; i < paths.size(); i++) {
            stbi_uc* faceData = stbi_load(paths[i].c_str(), &width, &height, &channels, 0);
            if (faceData) {
                glTextureSubImage3D(m_RendererID, 0, 0, 0, i, m_width, m_height, 1, m_data_format, GL_UNSIGNED_BYTE, faceData);
                stbi_image_free(faceData);
            }
            else {
                NX_CORE_ERROR("Failed to load cubemap face from path: {0}", paths[i]);
            }
        }

        stbi_set_flip_vertically_on_load(true);
    }

    std::vector<std::string> OpenGLTextureCubeMap::createFromDirectory(const std::string& dir) {
        std::vector<std::string> faces;
        faces.push_back(dir + "/right.jpg");
        faces.push_back(dir + "/left.jpg");
        faces.push_back(dir + "/top.jpg");
        faces.push_back(dir + "/bottom.jpg");
        faces.push_back(dir + "/front.jpg");
        faces.push_back(dir + "/back.jpg");
        return faces;
    }
} // namespace NexAur