#pragma once

#include "Core/Base.h"
#include "Function/Renderer/RHI/framebuffer.h"

namespace NexAur {
    class OpenGLFramebuffer : public Framebuffer {
    public:
        OpenGLFramebuffer(const FramebufferSpecification& spec);
        virtual ~OpenGLFramebuffer() override;

        virtual void bind() override;
        virtual void unbind() override;

        void invalidate(); // 创建或重新创建帧缓冲

        virtual void resize(uint32_t width, uint32_t height) override; // 编辑器中拖动视口改变分辨率时调用,让纹理跟随分辨率
        virtual int readPixel(uint32_t attachmentIndex, int x, int y) override; // 参数：要读取的纹理，x、y鼠标坐标，返回物体ID

        virtual void clearAttachment(uint32_t attachmentIndex, int value) override; // 清空某个特定的附件

        virtual uint32_t getColorAttachmentRendererID(uint32_t index = 0) const override; // 返回某个纹理ID

        virtual const FramebufferSpecification& getSpecification() const override;	// 返回配置结构体

    private:
        uint32_t m_ID;
        FramebufferSpecification m_Specification;   // 帧缓冲配置结构体
        std::vector<FramebufferTextureSpecification> m_ColorAttachmentSpecifications; // 颜色附件规格
        std::vector<uint32_t> m_ColorAttachments; // 颜色附件纹理ID列表(该列表的纹理可能由于重新创建改变id)
        FramebufferTextureSpecification m_DepthAttachmentSpecification; // 深度附件规格
        uint32_t m_DepthAttachment = 0; // 深度附件纹理ID(该纹理可能由于重新创建改变id)
    };
} // namespace NexAur
