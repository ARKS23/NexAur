#pragma once

#include "Core/Base.h"
#include "definitions.h"

namespace NexAur {
    // 帧缓冲纹理对象信息描述结构体
    struct FramebufferTextureSpecification {
        FramebufferTextureSpecification() = default;

        FramebufferTextureSpecification(FramebufferTextureFormat format, 
                                    TextureFilter filter = TextureFilter::Linear, 
                                    TextureWrap wrap = TextureWrap::Repeat) 
        : framebuffer_texture_format(format), texture_filiter(filter), texture_wrap(wrap) {}

        FramebufferTextureFormat framebuffer_texture_format;
        TextureFilter texture_filiter;
        TextureWrap texture_wrap;
        float AnisotropyLevel = 1.0f;   // 各向异性过滤,1.0表示关闭，16.0最高
    };


    // 配置结构体
	struct FramebufferSpecification {
		uint32_t width = 0, height = 0;					// 帧缓冲的分辨率
		std::vector<FramebufferTextureSpecification> Attachments;	// 附件列表, MRT多目标渲染
		uint32_t samples = 1;							// MSAA

		bool SwapChainTarget = false;					// 是否直接画到屏幕上，用来控制离屏渲染
	};


    class NEXAUR_API Framebuffer {
    public:
        virtual ~Framebuffer() = default;

		virtual void bind() = 0;
		virtual void unbind() = 0;

		virtual void resize(uint32_t width, uint32_t height) = 0; // 编辑器中拖动视口改变分辨率时调用,让纹理跟随分辨率
		virtual int readPixel(uint32_t attachmentIndex, int x, int y) = 0; // 参数：要读取的纹理，x、y鼠标坐标，返回物体ID

		virtual void clearAttachment(uint32_t attachmentIndex, int value) = 0; // 清空某个特定的附件

		virtual uint32_t getColorAttachmentRendererID(uint32_t index = 0) const = 0; // 返回某个纹理ID
        virtual uint32_t getDepthAttachmentRendererID() const = 0;

		virtual const FramebufferSpecification& getSpecification() const = 0;	// 返回配置结构体

		static std::shared_ptr<Framebuffer> create(const FramebufferSpecification& spec);
    };
} // namespace NexAur
