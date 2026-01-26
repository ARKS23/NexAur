#include "pch.h"
#include "opengl_framebuffer.h"

namespace NexAur {
    static const uint32_t MAX_WIDTH = 8192;
    static const uint32_t MAX_HEIGHT = 8192;

    static GLenum framebufferTextureFormatToGLInternalFormat(FramebufferTextureFormat format) {
        switch (format) {
            case FramebufferTextureFormat::RGBA8:          return GL_RGBA8;
            case FramebufferTextureFormat::RED_INTEGER:    return GL_R32I;
            case FramebufferTextureFormat::RGBA16F:        return GL_RGBA16F;
            case FramebufferTextureFormat::RGBA32F:        return GL_RGBA32F;
            case FramebufferTextureFormat::DEPTH24STENCIL8:return GL_DEPTH24_STENCIL8;
        }

        NX_CORE_ASSERT(false, "Unknown FramebufferTextureFormat!");
        return 0;
    }

    static GLenum framebufferTextureFormatToGLDataFormat(FramebufferTextureFormat format) {
        switch (format) {
            case FramebufferTextureFormat::RGBA8:          return GL_RGBA;
            case FramebufferTextureFormat::RED_INTEGER:    return GL_RED_INTEGER;
            case FramebufferTextureFormat::RGBA16F:        return GL_RGBA;
            case FramebufferTextureFormat::RGBA32F:        return GL_RGBA;
        }

        NX_CORE_ASSERT(false, "Unknown FramebufferTextureFormat!");
        return 0;
    }

    static GLenum GLWarpFromTextureWrap(TextureWrap wrap) {
        switch (wrap) {
            case TextureWrap::Repeat:        return GL_REPEAT;
            case TextureWrap::MirroredRepeat:return GL_MIRRORED_REPEAT;
            case TextureWrap::ClampToEdge:   return GL_CLAMP_TO_EDGE;
            case TextureWrap::ClampToBorder: return GL_CLAMP_TO_BORDER;
        }

        NX_CORE_ASSERT(false, "Unknown TextureWrap!");
        return GL_REPEAT;
    }

    static GLenum GLFilterFromTextureFilter(TextureFilter filter) {
        switch (filter) {
            case TextureFilter::Linear:               return GL_LINEAR;
            case TextureFilter::Nearest:              return GL_NEAREST;
            case TextureFilter::NearestMipmapNearest: return GL_NEAREST_MIPMAP_NEAREST;
            case TextureFilter::LinearMipmapNearest:  return GL_LINEAR_MIPMAP_NEAREST;
            case TextureFilter::NearestMipmapLinear:  return GL_NEAREST_MIPMAP_LINEAR;
            case TextureFilter::LinearMipmapLinear:   return GL_LINEAR_MIPMAP_LINEAR;
        }

        NX_CORE_ASSERT(false, "Unknown TextureFilter!");
        return GL_NEAREST;
    }

    static GLenum textureTarget(bool multisampled) {
        return multisampled ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
    }

    static void createTextures(bool multisampled, uint32_t* outID, uint32_t count) {
        glCreateTextures(textureTarget(multisampled), count, outID);
    }

    static void bindTexture(bool multisampled, uint32_t id) {
        glBindTexture(textureTarget(multisampled), id);
    }

    static void attachColorTexture(uint32_t id, int samples, uint32_t width, uint32_t height, int index, const FramebufferTextureSpecification& fb_format_spec) {
        bool multisampled = samples > 1;
        GLenum internalFormat = framebufferTextureFormatToGLInternalFormat(fb_format_spec.framebuffer_texture_format);
        GLenum format = framebufferTextureFormatToGLDataFormat(fb_format_spec.framebuffer_texture_format);
        if (multisampled) {
            glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, samples, internalFormat, width, height, GL_FALSE);
        }
        else {
            glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, GL_UNSIGNED_BYTE, nullptr);

            // 设置过滤和环绕方式
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GLFilterFromTextureFilter(fb_format_spec.texture_filiter));
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GLFilterFromTextureFilter(fb_format_spec.texture_filiter));
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GLWarpFromTextureWrap(fb_format_spec.texture_wrap));
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GLWarpFromTextureWrap(fb_format_spec.texture_wrap));
        }

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + index, textureTarget(multisampled), id, 0);
    }

    static void attachDepthTexture(uint32_t id, int samples, GLenum format, GLenum attachmentType, uint32_t width, uint32_t height, const FramebufferTextureSpecification& fb_format_spec) {
        bool multisampled = samples > 1;
        if (multisampled) {
            glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, samples, format, width, height, GL_FALSE);
        }
        else {
            glTexStorage2D(GL_TEXTURE_2D, 1, format, width, height);

            // 设置默认过滤和环绕方式
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GLFilterFromTextureFilter(fb_format_spec.texture_filiter));
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GLFilterFromTextureFilter(fb_format_spec.texture_filiter));
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GLWarpFromTextureWrap(fb_format_spec.texture_wrap));
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GLWarpFromTextureWrap(fb_format_spec.texture_wrap));
        }

        glFramebufferTexture2D(GL_FRAMEBUFFER, attachmentType, textureTarget(multisampled), id, 0);
    }

    static bool isDepthFormat(FramebufferTextureFormat format) {
        switch (format) {
            case FramebufferTextureFormat::DEPTH24STENCIL8: return true;
        }
        return false;
    }

    void OpenGLFramebuffer::invalidate() {
        // 纹理大小固定，一旦窗口发生改变，就需要重新创建帧缓冲和附件
        if (m_ID) {
            glDeleteFramebuffers(1, &m_ID);
            glDeleteTextures((GLsizei)m_ColorAttachments.size(), m_ColorAttachments.data());
            m_ColorAttachments.clear();
            if (m_DepthAttachment) {
                glDeleteTextures(1, &m_DepthAttachment);
                m_DepthAttachment = 0;
            }
        }

        // 创建新的帧缓冲
        glCreateFramebuffers(1, &m_ID);
        glBindFramebuffer(GL_FRAMEBUFFER, m_ID);

        // 多重采样
        bool multisample = m_Specification.samples > 1;

        // 创建颜色附件
        if (!m_ColorAttachmentSpecifications.empty()) {
            m_ColorAttachments.resize(m_ColorAttachmentSpecifications.size());
            createTextures(multisample, m_ColorAttachments.data(), (uint32_t)m_ColorAttachments.size());

            for (size_t i = 0; i < m_ColorAttachments.size(); i++) {
                bindTexture(multisample, m_ColorAttachments[i]);

                FramebufferTextureFormat format = m_ColorAttachmentSpecifications[i].framebuffer_texture_format;
                GLenum internalFormat = framebufferTextureFormatToGLInternalFormat(format);
                GLenum dataFormat = framebufferTextureFormatToGLDataFormat(format);

                attachColorTexture(m_ColorAttachments[i], m_Specification.samples, m_Specification.width, m_Specification.height, (int)i, m_ColorAttachmentSpecifications[i]);
            }
        }

        // 创建深度附件
        if (m_DepthAttachmentSpecification.framebuffer_texture_format != FramebufferTextureFormat::None) {
            createTextures(multisample, &m_DepthAttachment, 1);
            bindTexture(multisample, m_DepthAttachment);

            FramebufferTextureFormat format = m_DepthAttachmentSpecification.framebuffer_texture_format;
            GLenum glFormat = framebufferTextureFormatToGLInternalFormat(format);

            attachDepthTexture(m_DepthAttachment, m_Specification.samples, glFormat, GL_DEPTH_STENCIL_ATTACHMENT, m_Specification.width, m_Specification.height, m_DepthAttachmentSpecification);
        }

        // 设置绘制缓冲
        if (m_ColorAttachments.size() > 1) {
            NX_CORE_ASSERT(m_ColorAttachments.size() <= 4, "OpenGL supports up to 4 color attachments!");
            GLenum buffers[4] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3 };
            glDrawBuffers((GLsizei)m_ColorAttachments.size(), buffers);
        }
        else if (m_ColorAttachments.empty()) {
            // 没有颜色附件时禁用绘制缓冲
            glDrawBuffer(GL_NONE);
        }
        else {
            glDrawBuffer(GL_COLOR_ATTACHMENT0);
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    OpenGLFramebuffer::OpenGLFramebuffer(const FramebufferSpecification& spec)
        : m_Specification(spec), m_ID(0), m_DepthAttachment(0) {
        // 分离颜色和深度附件规格
        for (const auto& attachmentSpec : m_Specification.Attachments) {
            if (isDepthFormat(attachmentSpec.framebuffer_texture_format)) {
                m_DepthAttachmentSpecification = attachmentSpec;
            }
            else {
                m_ColorAttachmentSpecifications.push_back(attachmentSpec);
            }
        }

        invalidate();
    }

    OpenGLFramebuffer::~OpenGLFramebuffer() {
        glDeleteFramebuffers(1, &m_ID);
        glDeleteTextures((GLsizei)m_ColorAttachments.size(), m_ColorAttachments.data());
        if (m_DepthAttachment) glDeleteTextures(1, &m_DepthAttachment);
    }

    void OpenGLFramebuffer::bind() {
        glBindFramebuffer(GL_FRAMEBUFFER, m_ID);
        glViewport(0, 0, m_Specification.width, m_Specification.height);
    }

    void OpenGLFramebuffer::unbind() {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void OpenGLFramebuffer::resize(uint32_t width, uint32_t height) {
        if (width == 0 || height == 0 || width > MAX_WIDTH || height > MAX_HEIGHT) {
            NX_CORE_WARN("Attempted to resize framebuffer to invalid size: {0}, {1}", width, height);
            return;
        }

        m_Specification.width = width;
        m_Specification.height = height;
        invalidate();
    }

    int OpenGLFramebuffer::readPixel(uint32_t attachmentIndex, int x, int y) {
        NX_CORE_ASSERT(attachmentIndex < m_ColorAttachments.size(), "Attachment index out of bounds!");

        glReadBuffer(GL_COLOR_ATTACHMENT0 + attachmentIndex); // // 告诉OpenGL读哪张图
        int pixelData;
        glReadPixels(x, y, 1, 1, GL_RED_INTEGER, GL_INT, &pixelData); // 读取像素数据
        return pixelData;
    }

    void OpenGLFramebuffer::clearAttachment(uint32_t attachmentIndex, int value) {
        NX_CORE_ASSERT(attachmentIndex < m_ColorAttachments.size(), "Attachment index out of bounds!");

        FramebufferTextureFormat format = m_ColorAttachmentSpecifications[attachmentIndex].framebuffer_texture_format;
        GLenum glDataFormat = framebufferTextureFormatToGLDataFormat(format);

        glClearTexImage(m_ColorAttachments[attachmentIndex], 0, glDataFormat, GL_INT, &value);
    }

    uint32_t OpenGLFramebuffer::getColorAttachmentRendererID(uint32_t index) const {
        NX_CORE_ASSERT(index < m_ColorAttachments.size(), "Attachment index out of bounds!");
        return m_ColorAttachments[index];
    }

    const FramebufferSpecification& OpenGLFramebuffer::getSpecification() const {
        return m_Specification;
    }
} // namespace NexAur