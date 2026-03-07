#include "pch.h"
#include <string>
#include "scene.h"
#include "Function/Renderer/RHI/renderer_system.h"
#include "Function/Renderer/RHI/renderer_command.h"
#include "Function/Global/global_context.h"
#include "Function/Renderer/window_system.h"
#include "Function/File/file_system.h"
#include "Function/Input/input_system.h"

namespace NexAur {
    static RenderEntity createLightCube(glm::vec3 position, glm::vec3 color) {
        RenderEntity light_cube;

        // 光源立方体
        std::shared_ptr<VertexArray> cube_vertex_array = MeshFactory::createCubeMesh();
        std::shared_ptr<Shader> light_shader = RendererFactory::createShaderByPaths("light cube shader", 
        NX_ASSET("assets/shaders/lightCube/light_cube.vs"), NX_ASSET("assets/shaders/lightCube/light_cube.fs"));
        
        std::shared_ptr<Material> light_material = RendererFactory::createMaterial(light_shader);
        light_material->setFloat3("u_Color", color);
        light_cube.mesh = cube_vertex_array;
        light_cube.material = light_material;
        light_cube.name = "light_cube";
        light_cube.transform = glm::scale(light_cube.transform, glm::vec3(0.2f));
        light_cube.transform = glm::translate(light_cube.transform, position);

        return light_cube;
    }

    static RenderEntity createPhongCubeEntity() {
        RenderEntity cube_mesh_phong;

        // blling-phong Cube
        std::shared_ptr<VertexArray> cube_vertex_array = MeshFactory::createCubeMesh();
        std::shared_ptr<Shader> phong_shader = RendererFactory::createShaderByPaths("container phong shader", 
        NX_ASSET("assets/shaders/phong/phong.vs"), NX_ASSET("assets/shaders/phong/phong.fs"));
        
        std::shared_ptr<Material> phong_material = RendererFactory::createMaterial(phong_shader);
        std::shared_ptr<Texture2D> diffuse_texture = RendererFactory::createTexture2D(NX_ASSET("assets/textures/container/container2.png"));
        std::shared_ptr<Texture2D> specular_texture = RendererFactory::createTexture2D(NX_ASSET("assets/textures/container/container2_specular.png"));
        phong_material->setTexture("u_Material.diffuse", diffuse_texture);
        phong_material->setTexture("u_Material.specular", specular_texture);
        phong_material->setFloat("u_Material.shininess", 32.0f);
        cube_mesh_phong.mesh = cube_vertex_array;
        cube_mesh_phong.material = phong_material;
        cube_mesh_phong.name = "cube_test_phong";
        cube_mesh_phong.transform = glm::translate(cube_mesh_phong.transform, glm::vec3(1.5f, 0.0f, 0.0f));

        return cube_mesh_phong;
    }

    static RenderEntity createFloorEntity() {
        RenderEntity floor_entity;

        // 地面
        std::shared_ptr<VertexArray> floor_vertex_array = MeshFactory::createCubeMesh();
        std::shared_ptr<Shader> floor_shader = RendererFactory::createShaderByPaths("floor shader", 
        NX_ASSET("assets/shaders/phong/phong.vs"), NX_ASSET("assets/shaders/phong/phong.fs"));
        std::shared_ptr<Material> floor_material = RendererFactory::createMaterial(floor_shader);
        std::shared_ptr<Texture2D> floor_texture = RendererFactory::createTexture2D(NX_ASSET("assets/textures/wood/wood.png"));
        std::shared_ptr<Texture2D> specular_texture = RendererFactory::createTexture2D(NX_ASSET("assets/textures/PBR/gold/roughness.png"));
        floor_material->setTexture("u_Material.diffuse", floor_texture);
        floor_material->setTexture("u_Material.specular", specular_texture);
        floor_entity.mesh = floor_vertex_array;
        floor_entity.material = floor_material;
        floor_entity.name = "floor";
        floor_entity.transform = glm::translate(floor_entity.transform, glm::vec3(0.0f, -5.0f, 0.0f));
        floor_entity.transform = glm::rotate(floor_entity.transform, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        floor_entity.transform = glm::scale(floor_entity.transform, glm::vec3(50.0f, 50.f, 1.0f));

        return floor_entity;
    }

    static RenderEntity createPhongSphereEntity() {
        RenderEntity sphere_mesh_phong;

        // blling-phong Sphere
        std::shared_ptr<VertexArray> sphere_vertex_array = MeshFactory::createSphereMesh();
        std::shared_ptr<Shader> phong_shader = RendererFactory::createShaderByPaths("sphere phong shader", 
        NX_ASSET("assets/shaders/phong/phong.vs"), NX_ASSET("assets/shaders/phong/phong.fs"));
        std::shared_ptr<Material> phong_material = RendererFactory::createMaterial(phong_shader);

        std::shared_ptr<Texture2D> diffuse_texture = RendererFactory::createTexture2D(NX_ASSET("assets/textures/PBR/plastic/albedo.png"));
        std::shared_ptr<Texture2D> specular_texture = RendererFactory::createTexture2D(NX_ASSET("assets/textures/PBR/plastic/roughness.png"));
        phong_material->setTexture("u_Material.diffuse", diffuse_texture);
        phong_material->setTexture("u_Material.specular", specular_texture);
        phong_material->setFloat("u_Material.shininess", 32.0f);

        sphere_mesh_phong.mesh = sphere_vertex_array;
        sphere_mesh_phong.material = phong_material;
        sphere_mesh_phong.name = "sphere_test_phong";
        sphere_mesh_phong.transform = glm::translate(sphere_mesh_phong.transform, glm::vec3(1.5f, 0.0f, 2.0f));
        sphere_mesh_phong.transform = glm::scale(sphere_mesh_phong.transform, glm::vec3(0.7f));

        return sphere_mesh_phong;
    }

    static RenderEntity createPBRSphereEntity(std::string material_type) {
        RenderEntity sphere_mesh_pbr;
        static float position_offset = 0.0f;

        std::shared_ptr<VertexArray> sphere_vertex_array = MeshFactory::createSphereMesh();
        std::shared_ptr<Shader> pbr_shader = RendererFactory::createShaderByPaths("sphere pbr shader",
            NX_ASSET("assets/shaders/pbr/pbr.vs"), NX_ASSET("assets/shaders/pbr/pbr.fs"));
        std::shared_ptr<Material> pbr_material = RendererFactory::createMaterial(pbr_shader);

        // 材质设置
        std::shared_ptr<Texture2D> albedo_texture = RendererFactory::createTexture2D(NX_ASSET("assets/textures/PBR/" + material_type + "/albedo.png"));
        std::shared_ptr<Texture2D> metallic_texture = RendererFactory::createTexture2D(NX_ASSET("assets/textures/PBR/" + material_type + "/metallic.png"));
        std::shared_ptr<Texture2D> roughness_texture = RendererFactory::createTexture2D(NX_ASSET("assets/textures/PBR/" + material_type + "/roughness.png"));
        std::shared_ptr<Texture2D> ao_texture = RendererFactory::createTexture2D(NX_ASSET("assets/textures/PBR/" + material_type + "/ao.png"));
        std::shared_ptr<Texture2D> normal_texture = RendererFactory::createTexture2D(NX_ASSET("assets/textures/PBR/" + material_type + "/normal.png"));
        pbr_material->setTexture("u_Material.albedoMap", albedo_texture);
        pbr_material->setTexture("u_Material.metallicMap", metallic_texture);
        pbr_material->setTexture("u_Material.roughnessMap", roughness_texture);
        pbr_material->setTexture("u_Material.aoMap", ao_texture);
        pbr_material->setTexture("u_Material.normalMap", normal_texture);

        sphere_mesh_pbr.mesh = sphere_vertex_array;
        sphere_mesh_pbr.material = pbr_material;
        sphere_mesh_pbr.name = "sphere_test_pbr_" + material_type;
        sphere_mesh_pbr.transform = glm::translate(sphere_mesh_pbr.transform, glm::vec3(1.5f + position_offset, 0.0f, -2.0f));
        sphere_mesh_pbr.transform = glm::scale(sphere_mesh_pbr.transform, glm::vec3(0.7f));
        position_offset += 2.0f; // 每次创建一个PBR球体时，增加位置偏移，避免重叠

        return sphere_mesh_pbr;
    }

    Scene::Scene() {
        // 初始化光源
        initLight();

        // 初始化天空盒
        initSkybox();

        // 初始化场景
        RenderEntity cube_mesh_phong = createPhongCubeEntity();
        m_entities.push_back(cube_mesh_phong);
        RenderEntity sphere_mesh_phong = createPhongSphereEntity();
        m_entities.push_back(sphere_mesh_phong);
        RenderEntity floor_entity = createFloorEntity();
        m_entities.push_back(floor_entity);
        // PBR材质球体
        for (std::string material_type : {"gold", "grass", "plastic", "wall", "rusted_iron"}) {
            RenderEntity sphere_mesh_pbr = createPBRSphereEntity(material_type);
            // IBL测试绑定
            sphere_mesh_pbr.material->setTexture("u_IrradianceMap", this->getIrradianceMap());
            sphere_mesh_pbr.material->setTexture("u_PrefilterMap", this->getPrefilterMap());
            sphere_mesh_pbr.material->setTexture("u_BrdfLUT", this->getBRDFLUTMap());
            m_entities.push_back(sphere_mesh_pbr);
            for (int i = 3; i <= 12; i += 3) {
                RenderEntity sphere_mesh_pbr_cpy = sphere_mesh_pbr;
                sphere_mesh_pbr_cpy.transform = glm::translate(sphere_mesh_pbr_cpy.transform, glm::vec3(0.0f, i + 0.0f, 0.0f));
                m_entities.push_back(sphere_mesh_pbr_cpy);
            }
        }

        for (int i = 0; i < 5; ++i) {
            RenderEntity entity = cube_mesh_phong;
            entity.transform = glm::translate(entity.transform, glm::vec3(i * 2.0f, 0.0f, 0.0f));
            entity.name = "cube_" + std::to_string(i);
            m_entities.push_back(entity);
        }
    }

    void Scene::addEntity(const RenderEntity& entity) {
        m_entities.push_back(entity);
    }

    void Scene::onUpdate(float deltaTime) {
        // 天空盒查询更新(临时方案)
        std::shared_ptr<InputSystem> input_system = g_runtime_global_context.m_input_system;
        static bool z_key_pressed_last_frame = false;
        bool z_key_pressed = input_system->isKeyPressed(KeyCode::Z);
        if (z_key_pressed && !z_key_pressed_last_frame) {
            m_skybox_enabled = !m_skybox_enabled;
            m_directional_light.intensity = m_skybox_enabled ? 3.0f : 0.0f; // 启用天空盒时增加定向光强度，模拟白天效果
        }
        z_key_pressed_last_frame = z_key_pressed;

        // 定向光绕Y轴旋转
        float rotationSpeed = glm::radians(30.0f * deltaTime);
        // glm::mat4 lightRotation = glm::rotate(glm::mat4(1.0f), rotationSpeed, glm::vec3(0.0f, 1.0f, 0.0f));
        // m_directional_light.direction = glm::vec3(lightRotation * glm::vec4(m_directional_light.direction, 0.0f));
        // m_directional_light.direction = glm::normalize(m_directional_light.direction);
        // NX_CORE_INFO("Directional Light Direction: ({:.2f}, {:.2f}, {:.2f})", m_directional_light.direction.x, m_directional_light.direction.y, m_directional_light.direction.z);

        // 点光源绕Y轴旋转，模拟灯光旋转效果
        float deltaAngle = glm::radians(30.0f * deltaTime); 
        glm::vec3 orbitCenter = glm::vec3(5.0f, 1.0f, 0.0f);
        float targetRadius = 3.0f; 

        for (int i = 0; i < m_point_lights.size(); ++i) {
            PointLight& point_light = m_point_lights[i];

            // 1. 计算光源相对于“圆心”的相对坐标
            float relX = point_light.position.x - orbitCenter.x;
            float relZ = point_light.position.z - orbitCenter.z;

            // 2. 根据相对坐标反推当前角度
            float currentAngle = std::atan2(relZ, relX);
            float newAngle = currentAngle + deltaAngle;

            // 3. 计算新的相对位置
            point_light.position.x = orbitCenter.x + targetRadius * std::cos(newAngle);
            point_light.position.y = orbitCenter.y;
            point_light.position.z = orbitCenter.z + targetRadius * std::sin(newAngle);

            if (i < m_lights_entities.size()) {
                m_lights_entities[i].transform = glm::translate(glm::mat4(1.0f), point_light.position);
                m_lights_entities[i].transform = glm::scale(m_lights_entities[i].transform, glm::vec3(0.1f));
            }
        }
    }

    void Scene::initLight() {
        // 定向光
        m_directional_light = DirectionalLight();
        m_directional_light.direction = glm::normalize(glm::vec3(-0.6f, -0.6f, 0.6f));
        m_directional_light.color = glm::vec3(1.0f, 1.0f, 1.0f);
        m_directional_light.intensity = 0.0f;

        // 点光源
        PointLight point_light1;
        point_light1.position = glm::vec3(-5.0f, 0.0f, 0.0f);
        point_light1.color = glm::vec3(0.3f, 0.2f, 0.66f);
        point_light1.intensity = 20.0f;
        m_point_lights.push_back(point_light1);
        RenderEntity light_cube1 = createLightCube(point_light1.position, point_light1.color);
        m_lights_entities.push_back(light_cube1);
    }

    void Scene::initSkybox() {
        // 1. 加载 2D HDR 贴图
        std::shared_ptr<Texture2D> hdrTexture = RendererFactory::createTexture2D(NX_ASSET("assets/textures/HDR/warm_restaurant_8k.hdr"));
        std::shared_ptr<Shader> equiToCubeShader = RendererFactory::createShaderByPaths("equiToCube", 
                NX_ASSET("assets/shaders/pbr/equirectangular_to_cubemap.vs"), 
                NX_ASSET("assets/shaders/pbr/equirectangular_to_cubemap.fs"));

        // 2. 创建一个作为目标的空 Cubemap
        TextureSpecification envCubeSpec;
        envCubeSpec.width = 1024;
        envCubeSpec.height = 1024;
        envCubeSpec.format = ImageFormat::RGB16F;
        envCubeSpec.generate_mips = true;
        envCubeSpec.filter = TextureFilter::LinearMipmapLinear;
        m_skybox_texture = RendererFactory::createTextureCube(envCubeSpec);

        // 3. 创建预处理用的离屏 FBO (仅需要深度附件)
        FramebufferSpecification captureFBO_spec;
        captureFBO_spec.width = 1024;
        captureFBO_spec.height = 1024;
        captureFBO_spec.Attachments = { FramebufferTextureFormat::DEPTH24STENCIL8 };
        std::shared_ptr<Framebuffer> captureFBO = Framebuffer::create(captureFBO_spec);

        // 4. 设置 6 个面对应的视图投影矩阵
        glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
        glm::mat4 captureViews[] = {
            glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)), // Right
            glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)), // Left
            glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)), // Top
            glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)), // Bottom
            glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)), // Front
            glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))  // Back
        };

        // 5. 开始渲染：将HDR图像渲染到Cubemap的6个面上
        equiToCubeShader->bind();
        equiToCubeShader->setInt("u_EquirectangularMap", 0);
        hdrTexture->bind(0);

        // 立方网格题当底布
        std::shared_ptr<VertexArray> cubeMesh = MeshFactory::createCubeMesh(); 

        RendererCommand::setViewport(0, 0, 1024, 1024);

        for (unsigned int i = 0; i < 6; ++i) {
            equiToCubeShader->setMat4("u_ViewProjection", captureProjection * captureViews[i]);
            
            captureFBO->attachTextureCubeFace(0, m_skybox_texture, i, 0);
            captureFBO->bind();
            
            RendererCommand::clear(ClearBufferFlag::ColorDepth);
            
            // 渲染立方体
            cubeMesh->bind();
            RendererCommand::drawIndexed(cubeMesh);
        }

        captureFBO->unbind();
        m_skybox_texture->generateMips();

        // =========================== 生成Irradiance Map ============================
        TextureSpecification irradianceSpec;
        irradianceSpec.width = 32;
        irradianceSpec.height = 32;
        irradianceSpec.format = ImageFormat::RGB16F;
        irradianceSpec.generate_mips = false; // Irradiance 不需要 Mipmap
        irradianceSpec.filter = TextureFilter::Linear;
        m_irradiance_map = RendererFactory::createTextureCube(irradianceSpec);

        // 专门创建一个 32x32 的离屏 FBO，杜绝尺寸不匹配引发的黑屏
        FramebufferSpecification irradianceFBOSpec;
        irradianceFBOSpec.width = 32;
        irradianceFBOSpec.height = 32;
        irradianceFBOSpec.Attachments = { FramebufferTextureFormat::DEPTH24STENCIL8 };
        std::shared_ptr<Framebuffer> irradianceFBO = Framebuffer::create(irradianceFBOSpec);

        std::shared_ptr<Shader> irradianceShader = RendererFactory::createShaderByPaths("irradiance_conv", 
                NX_ASSET("assets/shaders/pbr/irradiance_convolution.vs"), 
                NX_ASSET("assets/shaders/pbr/irradiance_convolution.fs"));

        irradianceShader->bind();
        irradianceShader->setInt("u_EnvironmentMap", 0);
        m_skybox_texture->bind(0);

        RendererCommand::setViewport(0, 0, 32, 32); 

        for (unsigned int i = 0; i < 6; ++i) {
            irradianceShader->setMat4("u_ViewProjection", captureProjection * captureViews[i]);
            
            irradianceFBO->attachTextureCubeFace(0, m_irradiance_map, i, 0);
            irradianceFBO->bind();

            RendererCommand::clear(ClearBufferFlag::ColorDepth);
            
            cubeMesh->bind();
            RendererCommand::drawIndexed(cubeMesh);
        }
        irradianceFBO->unbind();

        // ============================= Prefilter Map =============================
        TextureSpecification prefilterSpec;
        prefilterSpec.width = 128; // IBL 高光通常 128x128 足够起步
        prefilterSpec.height = 128;
        prefilterSpec.format = ImageFormat::RGB16F;
        prefilterSpec.generate_mips = true; // 【极其关键】必须由显卡提前划出多个级别的空间！
        prefilterSpec.filter = TextureFilter::LinearMipmapLinear; // 三线性插值
        m_prefilter_map = RendererFactory::createTextureCube(prefilterSpec);

        std::shared_ptr<Shader> prefilterShader = RendererFactory::createShaderByPaths("prefilter", 
                NX_ASSET("assets/shaders/pbr/prefilter.vs"), 
                NX_ASSET("assets/shaders/pbr/prefilter.fs"));

        prefilterShader->bind();
        prefilterShader->setInt("u_EnvironmentMap", 0);
        m_skybox_texture->bind(0); // 同样采用最原始的高清天空盒做输入

        unsigned int maxMipLevels = 5;
        for (unsigned int mip = 0; mip < maxMipLevels; ++mip) {
            // 每下一层 Mip，宽高缩小一半 (128 -> 64 -> 32 -> 16 -> 8)
            unsigned int mipWidth  = static_cast<unsigned int>(128 * std::pow(0.5, mip));
            unsigned int mipHeight = static_cast<unsigned int>(128 * std::pow(0.5, mip));

            // 【防止黑屏】：为当前的尺寸专属创建一个 FBO
            FramebufferSpecification mipFBOSpec;
            mipFBOSpec.width = mipWidth;
            mipFBOSpec.height = mipHeight;
            mipFBOSpec.Attachments = { FramebufferTextureFormat::DEPTH24STENCIL8 };
            std::shared_ptr<Framebuffer> mipFBO = Framebuffer::create(mipFBOSpec);
            
            RendererCommand::setViewport(0, 0, mipWidth, mipHeight);

            // 越模糊的层，传入的粗糙度越大
            float roughness = (float)mip / (float)(maxMipLevels - 1);
            prefilterShader->setFloat("u_Roughness", roughness);

            for (unsigned int i = 0; i < 6; ++i) {
                prefilterShader->setMat4("u_ViewProjection", captureProjection * captureViews[i]);
                
                // 之前设计的 attach 接口带有 mip 参数，在此发挥作用
                mipFBO->attachTextureCubeFace(0, m_prefilter_map, i, mip);
                mipFBO->bind();

                RendererCommand::clear(ClearBufferFlag::ColorDepth);
                
                cubeMesh->bind(); // 绑定盒子模型
                RendererCommand::drawIndexed(cubeMesh);
            }
            mipFBO->unbind();
        }

        // ============================= BRDF LUT =============================
        FramebufferSpecification brdfSpec;
        brdfSpec.width = 512;
        brdfSpec.height = 512;
        // BRDFLUT 是2D的红绿色图像，使用普通的颜色附件
        brdfSpec.Attachments = { FramebufferTextureSpecification(FramebufferTextureFormat::RGBA16F) }; 
        std::shared_ptr<Framebuffer> brdfFBO = Framebuffer::create(brdfSpec);

        std::shared_ptr<Shader> brdfShader = RendererFactory::createShaderByPaths("brdf_shader", 
                NX_ASSET("assets/shaders/pbr/brdf.vs"), 
                NX_ASSET("assets/shaders/pbr/brdf.fs"));

        brdfFBO->bind();
        RendererCommand::setViewport(0, 0, 512, 512);
        RendererCommand::clear(ClearBufferFlag::ColorDepth);

        brdfShader->bind();
        std::shared_ptr<VertexArray> quadMesh = MeshFactory::createQuadMesh();
        quadMesh->bind();
        RendererCommand::drawIndexed(quadMesh);

        brdfFBO->unbind();

        m_brdf_lut_map = RendererFactory::createTexture2D(brdfFBO->getColorAttachmentRendererID(0), 512, 512);


        auto [width, height] = g_runtime_global_context.m_window_system->getWindowSize();
        RendererCommand::setViewport(0, 0, width, height);
    }

    std::shared_ptr<VertexArray> MeshFactory::createCubeMesh() {
        auto vertex_array = RendererFactory::createVertexArray();

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

    std::shared_ptr<VertexArray> MeshFactory::createSphereMesh() {
        auto vertex_array = RendererFactory::createVertexArray();

        std::vector<float> data;
        std::vector<unsigned int> indices;

        const unsigned int X_SEGMENTS = 64;
        const unsigned int Y_SEGMENTS = 64;
        const float PI = 3.14159265359f;

        // 1. 生成顶点数据 (交错排列: Position, Normal, UV)
        for (unsigned int y = 0; y <= Y_SEGMENTS; ++y) {
            for (unsigned int x = 0; x <= X_SEGMENTS; ++x) {
                // 计算归一化的比例 (0.0 到 1.0)
                float xSegment = (float)x / (float)X_SEGMENTS;
                float ySegment = (float)y / (float)Y_SEGMENTS;

                // 球坐标系 -> 笛卡尔坐标系转换
                float xPos = std::cos(xSegment * 2.0f * PI) * std::sin(ySegment * PI);
                float yPos = std::cos(ySegment * PI);
                float zPos = std::sin(xSegment * 2.0f * PI) * std::sin(ySegment * PI);

                // Position (3 floats)
                data.push_back(xPos);
                data.push_back(yPos);
                data.push_back(zPos);

                // Normal (3 floats) - 对于原点在(0,0,0)的单位球，法线就是位置本身
                data.push_back(xPos);
                data.push_back(yPos);
                data.push_back(zPos);

                // TexCoord / UV (2 floats)
                data.push_back(xSegment);
                data.push_back(ySegment);
            }
        }

        // 2. 生成索引 (使用标准的 GL_TRIANGLES 生成面，兼容引擎架构)
        for (unsigned int y = 0; y < Y_SEGMENTS; ++y) {
            for (unsigned int x = 0; x < X_SEGMENTS; ++x) {
                unsigned int current = y * (X_SEGMENTS + 1) + x;
                unsigned int next    = current + 1;
                unsigned int below   = (y + 1) * (X_SEGMENTS + 1) + x;
                unsigned int belowNext = below + 1;

                // 第一个三角形
                indices.push_back(current);
                indices.push_back(below);
                indices.push_back(next);

                // 第二个三角形
                indices.push_back(next);
                indices.push_back(below);
                indices.push_back(belowNext);
            }
        }

        // 3. 上传数据到 GPU
        auto vbo = RendererFactory::createVertexBuffer(data.data(), data.size() * sizeof(float));
        
        vbo->setLayout({
            { ShaderDataType::Float3, "a_Pos" },
            { ShaderDataType::Float3, "a_Normal" },
            { ShaderDataType::Float2, "a_TexCoord" } 
        });
        vertex_array->addVertexBuffer(vbo);

        auto ebo = RendererFactory::createIndexBuffer(indices.data(), indices.size());
        vertex_array->setIndexBuffer(ebo);

        return vertex_array;
    }

    std::shared_ptr<VertexArray> MeshFactory::createQuadMesh() {
        auto vertex_array = RendererFactory::createVertexArray();

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
} // namespace NexAur

