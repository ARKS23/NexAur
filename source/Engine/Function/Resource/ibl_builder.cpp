#include "pch.h"
#include "ibl_builder.h"
#include "Function/Global/global_context.h"
#include "Function/File/file_system.h"
#include "Function/Renderer/window_system.h"
#include "Function/Renderer/RHI/renderer_system.h"
#include "Function/Renderer/RHI/renderer_command.h"

namespace NexAur {
    static std::shared_ptr<VertexArray> createCubeMesh() {
        // 用于绘制天空盒的立方体网格顶点
        std::shared_ptr<VertexArray> vertex_array = RendererFactory::createVertexArray();

        float vertices[] = {
		-0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 0.0f,
		 0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 0.0f,
		 0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 1.0f,
		 0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 1.0f,
		-0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 1.0f,
		-0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 0.0f,

		-0.5f, -0.5f,  0.5f,  0.0f,  0.0f, 1.0f,   0.0f, 0.0f,
		 0.5f, -0.5f,  0.5f,  0.0f,  0.0f, 1.0f,   1.0f, 0.0f,
		 0.5f,  0.5f,  0.5f,  0.0f,  0.0f, 1.0f,   1.0f, 1.0f,
		 0.5f,  0.5f,  0.5f,  0.0f,  0.0f, 1.0f,   1.0f, 1.0f,
		-0.5f,  0.5f,  0.5f,  0.0f,  0.0f, 1.0f,   0.0f, 1.0f,
		-0.5f, -0.5f,  0.5f,  0.0f,  0.0f, 1.0f,   0.0f, 0.0f,

		-0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 0.0f,
		-0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 1.0f,
		-0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
		-0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
		-0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  0.0f, 0.0f,
		-0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 0.0f,

		 0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f,
		 0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f,
		 0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
		 0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
		 0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  0.0f, 0.0f,
		 0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f,

		-0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 1.0f,
		 0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 1.0f,
		 0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 0.0f,
		 0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 0.0f,
		-0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 0.0f,
		-0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 1.0f,

		-0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 1.0f,
		 0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 1.0f,
		 0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 0.0f,
		 0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 0.0f,
		-0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 0.0f,
		-0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 1.0f};

        unsigned int indexArray[36];
        for (int i = 0; i < 36; ++i) indexArray[i] = i;

        auto vbo = RendererFactory::createVertexBuffer(vertices, sizeof(vertices));
        vbo->setLayout({
            { ShaderDataType::Float3, "a_Pos" },
            { ShaderDataType::Float3, "a_Normal" },
            { ShaderDataType::Float2, "a_TexCoord" }
        });

        auto ebo = RendererFactory::createIndexBuffer(indexArray, 36);

        vertex_array->addVertexBuffer(vbo);
        vertex_array->setIndexBuffer(ebo);

        return vertex_array;
    }

    static std::shared_ptr<VertexArray> createQuadMesh() {
        std::shared_ptr<VertexArray> vertex_array = RendererFactory::createVertexArray();

        // 顶点：位置 (x,y,z), 法线 (nx,ny,nz), UV (u,v)
        float quadVertices[] = {
            -1.0f,  1.0f, 0.0f,  0.0f, 0.0f, 1.0f,  0.0f, 1.0f,
            -1.0f, -1.0f, 0.0f,  0.0f, 0.0f, 1.0f,  0.0f, 0.0f,
             1.0f,  1.0f, 0.0f,  0.0f, 0.0f, 1.0f,  1.0f, 1.0f,
             1.0f, -1.0f, 0.0f,  0.0f, 0.0f, 1.0f,  1.0f, 0.0f,
        };
        unsigned int indices[] = { 0, 1, 2, 1, 3, 2 };

        auto vbo = RendererFactory::createVertexBuffer(quadVertices, sizeof(quadVertices));
        vbo->setLayout({
            { ShaderDataType::Float3, "a_Pos" },
            { ShaderDataType::Float3, "a_Normal" },
            { ShaderDataType::Float2, "a_TexCoord" }
        });
        vertex_array->addVertexBuffer(vbo);

        auto ebo = RendererFactory::createIndexBuffer(indices, 6);
        vertex_array->setIndexBuffer(ebo);

        return vertex_array;
    }

    // 辅助数组
    static glm::mat4 capture_projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
    static glm::mat4 capture_views[] = {
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)), // Right
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)), // Left
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)), // Top
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)), // Bottom
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)), // Front
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))  // Back
    };

    std::shared_ptr<Texture2D> IBLBuilder::s_brdf_lut_map = nullptr;

    std::shared_ptr<EnvironmentMap> IBLBuilder::bakeIBLFromHDR(const std::string& hdr_path) {
        std::shared_ptr<EnvironmentMap> env_map = std::make_shared<EnvironmentMap>();

        std::shared_ptr<Texture2D> hdr_texture = RendererFactory::createTexture2D(NX_ASSET(hdr_path));
        env_map->skybox_texture = generateSkybox(hdr_texture);
        env_map->irradiance_map = generateIrradianceMap(env_map->skybox_texture);
        env_map->prefilter_map = generatePrefilterMap(env_map->skybox_texture);
        if (!s_brdf_lut_map) s_brdf_lut_map = generateBRDFLUT();
        env_map->brdf_lut_map = s_brdf_lut_map;

        auto [width, height] = g_runtime_global_context.m_window_system->getWindowSize();
        RendererCommand::setViewport(0, 0, width, height);

        return env_map;
    }

    std::shared_ptr<TextureCubeMap> IBLBuilder::generateSkybox(std::shared_ptr<Texture2D> hdr_texture) {
        // 天空盒着色器
        std::shared_ptr<Shader> equi_to_cube_shader = RendererFactory::createShaderByPaths("equiToCube", 
                        NX_ASSET("assets/shaders/pbr/equirectangular_to_cubemap.vs"), 
                        NX_ASSET("assets/shaders/pbr/equirectangular_to_cubemap.fs"));

        // 创建CubeMap写入天空盒纹理
        TextureSpecification skybox_cube_spec;
        skybox_cube_spec.width = 1024;
        skybox_cube_spec.height = 1024;
        skybox_cube_spec.format = ImageFormat::RGB16F;
        skybox_cube_spec.generate_mips = true;
        skybox_cube_spec.filter = TextureFilter::LinearMipmapLinear;
        std::shared_ptr<TextureCubeMap> skybox_texture = RendererFactory::createTextureCube(skybox_cube_spec);

        // 离屏FBO
        FramebufferSpecification captureFBO_spec;
        captureFBO_spec.width = 1024;
        captureFBO_spec.height = 1024;
        captureFBO_spec.Attachments = { FramebufferTextureFormat::DEPTH24STENCIL8 };
        std::shared_ptr<Framebuffer> captureFBO = Framebuffer::create(captureFBO_spec);

        // 渲染到6个面
        equi_to_cube_shader->bind();
        equi_to_cube_shader->setInt("u_EquirectangularMap", 0);
        hdr_texture->bind(0);
        std::shared_ptr<VertexArray> cube_mesh = createCubeMesh();

        RendererCommand::setViewport(0, 0, 1024, 1024);

        for (unsigned int i = 0; i < 6; ++i) {
            equi_to_cube_shader->setMat4("u_ViewProjection", capture_projection * capture_views[i]);
            
            captureFBO->attachTextureCubeFace(0, skybox_texture, i, 0);
            captureFBO->bind();
            
            RendererCommand::clear(ClearBufferFlag::ColorDepth);
            
            cube_mesh->bind();
            RendererCommand::drawIndexed(cube_mesh);
        }

        captureFBO->unbind();
        skybox_texture->generateMips(); // 生成Mipmaps供后续预过滤使用
        return skybox_texture;
    }

    std::shared_ptr<TextureCubeMap> IBLBuilder::generateIrradianceMap(std::shared_ptr<TextureCubeMap> skybox_texture) {
        TextureSpecification irradiance_spec;
        irradiance_spec.width = 32;
        irradiance_spec.height = 32;
        irradiance_spec.format = ImageFormat::RGB16F;
        irradiance_spec.generate_mips = false; // Irradiance 不需要 Mipmap
        irradiance_spec.filter = TextureFilter::Linear;
        std::shared_ptr<TextureCubeMap> irradiance_map = RendererFactory::createTextureCube(irradiance_spec);

        FramebufferSpecification irradianceFBO_spec;
        irradianceFBO_spec.width = 32;
        irradianceFBO_spec.height = 32;
        irradianceFBO_spec.Attachments = { FramebufferTextureFormat::DEPTH24STENCIL8 };
        std::shared_ptr<Framebuffer> irradianceFBO = Framebuffer::create(irradianceFBO_spec);

        std::shared_ptr<Shader> irradiance_shader = RendererFactory::createShaderByPaths("irradiance_conv", 
                NX_ASSET("assets/shaders/pbr/irradiance_convolution.vs"), 
                NX_ASSET("assets/shaders/pbr/irradiance_convolution.fs"));

        irradiance_shader->bind();
        irradiance_shader->setInt("u_EnvironmentMap", 0);
        skybox_texture->bind(0);

        std::shared_ptr<VertexArray> cube_mesh = createCubeMesh();
        RendererCommand::setViewport(0, 0, 32, 32); 

        for (unsigned int i = 0; i < 6; ++i) {
            irradiance_shader->setMat4("u_ViewProjection", capture_projection * capture_views[i]);
            
            irradianceFBO->attachTextureCubeFace(0, irradiance_map, i, 0);
            irradianceFBO->bind();

            RendererCommand::clear(ClearBufferFlag::ColorDepth);
            
            cube_mesh->bind();
            RendererCommand::drawIndexed(cube_mesh);
        }
        irradianceFBO->unbind();

        return irradiance_map;
    }

    std::shared_ptr<TextureCubeMap> IBLBuilder::generatePrefilterMap(std::shared_ptr<TextureCubeMap> skybox_texture) {
        TextureSpecification prefilter_spec;
        prefilter_spec.width = 128; 
        prefilter_spec.height = 128;
        prefilter_spec.format = ImageFormat::RGB16F;
        prefilter_spec.generate_mips = true;
        prefilter_spec.filter = TextureFilter::LinearMipmapLinear; // 三线性插值
        std::shared_ptr<TextureCubeMap> prefilter_map = RendererFactory::createTextureCube(prefilter_spec);

        std::shared_ptr<Shader> prefilter_shader = RendererFactory::createShaderByPaths("prefilter", 
                NX_ASSET("assets/shaders/pbr/prefilter.vs"), 
                NX_ASSET("assets/shaders/pbr/prefilter.fs"));

        prefilter_shader->bind();
        prefilter_shader->setInt("u_EnvironmentMap", 0);
        skybox_texture->bind(0); // 采用原始的高清天空盒做输入

        std::shared_ptr<VertexArray> cube_mesh = createCubeMesh();

        unsigned int max_mip_levels = 5;
        for (unsigned int mip = 0; mip < max_mip_levels; ++mip) {
            // 每下一层 Mip，宽高缩小一半 (128 -> 64 -> 32 -> 16 -> 8)
            unsigned int mipWidth  = static_cast<unsigned int>(128 * std::pow(0.5, mip));
            unsigned int mipHeight = static_cast<unsigned int>(128 * std::pow(0.5, mip));

            FramebufferSpecification mipFBOSpec;
            mipFBOSpec.width = mipWidth;
            mipFBOSpec.height = mipHeight;
            mipFBOSpec.Attachments = { FramebufferTextureFormat::DEPTH24STENCIL8 };
            std::shared_ptr<Framebuffer> mipFBO = Framebuffer::create(mipFBOSpec);
            
            RendererCommand::setViewport(0, 0, mipWidth, mipHeight);

            // 越模糊的层，传入的粗糙度越大
            float roughness = (float)mip / (float)(max_mip_levels - 1);
            prefilter_shader->setFloat("u_Roughness", roughness);

            for (unsigned int i = 0; i < 6; ++i) {
                prefilter_shader->setMat4("u_ViewProjection", capture_projection * capture_views[i]);
                
                // 之前设计的 attach 接口带有 mip 参数，在此发挥作用
                mipFBO->attachTextureCubeFace(0, prefilter_map, i, mip);
                mipFBO->bind();

                RendererCommand::clear(ClearBufferFlag::ColorDepth);
                
                cube_mesh->bind(); // 绑定盒子模型
                RendererCommand::drawIndexed(cube_mesh);
            }
            mipFBO->unbind();
        }

        return prefilter_map;
    }

    std::shared_ptr<Texture2D> IBLBuilder::generateBRDFLUT() {
        FramebufferSpecification brdf_spec;
        brdf_spec.width = 512;
        brdf_spec.height = 512;
        // BRDFLUT 是2D的红绿色图像，使用普通的颜色附件
        brdf_spec.Attachments = { FramebufferTextureSpecification(FramebufferTextureFormat::RGBA16F) }; 
        std::shared_ptr<Framebuffer> brdfFBO = Framebuffer::create(brdf_spec);

        std::shared_ptr<Shader> brdf_shader = RendererFactory::createShaderByPaths("brdf_shader", 
                NX_ASSET("assets/shaders/pbr/brdf.vs"), 
                NX_ASSET("assets/shaders/pbr/brdf.fs"));

        brdfFBO->bind();
        RendererCommand::setViewport(0, 0, 512, 512);
        RendererCommand::clear(ClearBufferFlag::ColorDepth);

        brdf_shader->bind();
        std::shared_ptr<VertexArray> quadMesh = createQuadMesh();
        quadMesh->bind();
        RendererCommand::drawIndexed(quadMesh);

        brdfFBO->unbind();

        std::shared_ptr<Texture2D> brdf_lut_map = RendererFactory::createTexture2D(brdfFBO->getColorAttachmentRendererID(0), 512, 512);
        return brdf_lut_map;
    }
} // namespace NexAur
