#pragma once
#include<cstdint>

#include<string>

#include"Core/Base.h"
#include "definitions.h"

namespace NexAur {
    // 参数对象化，减少函数的参数量
    struct TextureSpecification {
        uint32_t width = 1;
        uint32_t height = 1;
        ImageFormat format = ImageFormat::RGBA8;
        bool generate_mips = true;	// 是否开启Mipmaps
    };


    // 纹理对象封装
    class NEXAUR_API Texture {
    public:
        virtual ~Texture() = default;

        /* 获取纹理属性的接口 */
        virtual uint32_t getWidth() const = 0;
        virtual uint32_t getHeight() const = 0;
        virtual uint32_t getRendererID() const = 0;
        virtual const std::string& getPath() const = 0; // 获取路径，引擎通过路径去重防止重复加载
        virtual const TextureSpecification& getSpecification() const = 0;

        virtual void setData(void* data, uint32_t size) = 0; // 程序化生成纹理需要用到的接口
        virtual void bind(uint32_t slot = 0) const = 0;
        virtual bool isLoaded() const = 0;

        virtual bool operator==(const Texture& other) const = 0; // 重载相等运算符判断是不是同一个纹理
    };


    // 2D纹理对象
    class NEXAUR_API Texture2D : public Texture {
	public:
		static std::shared_ptr<Texture2D> create(const TextureSpecification& specification); // 创建指定参数的纹理，然后通过SetData程序化填充
		static std::shared_ptr<Texture2D> create(const std::string& path); // 从系统加载纹理
	};


    // 立方纹理对象
	class NEXAUR_API TextureCubeMap : public Texture {
	public:
		static std::shared_ptr<TextureCubeMap> create(const TextureSpecification& specification);
		static std::shared_ptr<TextureCubeMap> create(const std::string& path);
	};
} // namespace NexAur
