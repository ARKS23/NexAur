#pragma once
#include "Core/Base.h"
#include "vertex_array.h"
#include "uniform_buffer.h"
#include "framebuffer.h"
#include "texture.h"
#include "material.h"
#include "buffer.h"

namespace NexAur {
    // 渲染模块资源工厂
    class NEXAUR_API RendererFactory {
    public:
        // Framebuffer
        static std::shared_ptr<Framebuffer> createFramebuffer(const FramebufferSpecification& spec);

        // Material
        static std::shared_ptr<Material> createMaterial(const std::shared_ptr<Shader>& shader);

        // Shader
        static std::shared_ptr<Shader> createShader(const std::string& filepath);
        static std::shared_ptr<Shader> createShader(const std::string& name, const std::string& vertexSrc, const std::string& fragmentSrc);

        // TexutreCubeMap
        static std::shared_ptr<TextureCubeMap> createTextureCube(const TextureSpecification& specification);
		static std::shared_ptr<TextureCubeMap> createTextureCube(const std::string& path);

        // Texutre2D
        static std::shared_ptr<Texture2D> createTexture2D(const TextureSpecification& specification); 
		static std::shared_ptr<Texture2D> createTexture2D(const std::string& path);

        // VAO
        static std::shared_ptr<VertexArray> createVertexArray();

        // VBO
        static std::shared_ptr<VertexBuffer> createVertexBuffer(uint32_t size);
        static std::shared_ptr<VertexBuffer> createVertexBuffer(float* vertices, uint32_t size);

        // EBO
        static std::shared_ptr<VertexBuffer> createIndexBuffer(uint32_t* indices, uint32_t count);

        // Unifrom Buffer
        static std::shared_ptr<UniformBuffer> createUniformBuffer(uint32_t size, uint32_t binding);
    };

    
    // 渲染模块总线
    class NEXAUR_API RendererSystem {
    public:

    private:
        
    };
} // namespace NexAur
