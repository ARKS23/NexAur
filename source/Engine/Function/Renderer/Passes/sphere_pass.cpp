#include "pch.h"
#include "sphere_pass.h"
#include "Function/Renderer/RHI/renderer_system.h"
#include "Function/Renderer/RHI/renderer_command.h"

namespace NexAur {
    static const float M_PI = 3.14159265358979323846f;

    void SpherePass::execute() {
        if (m_shader && m_vertex_array) {
            m_shader->bind();
            m_vertex_array->bind();
            RendererCommand::drawIndexed(m_vertex_array);
        }
    }

    void SpherePass::initResources() {
         m_vertex_array = RendererFactory::createVertexArray();

        // 球体参数
        const float radius = 1.0f;
        const int sectors = 36; // 经度切分 (竖着切)
        const int stacks = 18;  // 纬度切分 (横着切)

        std::vector<float> vertices;
        std::vector<uint32_t> indices;

        // 1. 生成顶点 (Pos + Normal + UV)
        // x = r * cos(u) * cos(v)
        // y = r * sin(v)
        // z = r * sin(u) * cos(v)
        
        float sectorStep = 2 * M_PI / sectors;
        float stackStep = M_PI / stacks;

        for (int i = 0; i <= stacks; ++i) {
            float stackAngle = M_PI / 2 - i * stackStep;        // from pi/2 to -pi/2
            float xy = radius * cosf(stackAngle);             // r * cos(u)
            float z = radius * sinf(stackAngle);              // r * sin(u)

            // 每一层的点
            for (int j = 0; j <= sectors; ++j) {
                float sectorAngle = j * sectorStep;           // from 0 to 2pi

                // 顶点坐标 (Position)
                float x = xy * cosf(sectorAngle);
                float y = xy * sinf(sectorAngle);
                
                // 添加 Position (Layout: 0) -> Float3
                vertices.push_back(x);
                vertices.push_back(y);
                vertices.push_back(z);

                // 添加 Normal (法线，球心的法线就是归一化的坐标) -> Float3
                // (对于球心在原点的球，法线就是 pos / radius)
                vertices.push_back(x / radius);
                vertices.push_back(y / radius);
                vertices.push_back(z / radius);

                // 可以通过 Normal 简单当做颜色来通过测试： Color = Normal * 0.5 + 0.5
            }
        }

        // 2. 生成索引 (CCW)
        // k1--k1+1
        // |  / |
        // | /  |
        // k2--k2+1
        for (int i = 0; i < stacks; ++i) {
            int k1 = i * (sectors + 1);     // 当前层起始索引
            int k2 = k1 + sectors + 1;      // 下一层起始索引

            for (int j = 0; j < sectors; ++j, ++k1, ++k2) {
                // 两个三角形构成一个四边形面
                
                // k1 != 0 顶端不需要画第一个三角形
                if (i != 0) {
                    indices.push_back(k1);
                    indices.push_back(k2);
                    indices.push_back(k1 + 1);
                }

                // k1 != stacks-1 底端不需要画第二个三角形
                if (i != (stacks - 1)) {
                    indices.push_back(k1 + 1);
                    indices.push_back(k2);
                    indices.push_back(k2 + 1);
                }
            }
        }

        // 3. 上传数据
        auto vbo = RendererFactory::createVertexBuffer(vertices.data(), vertices.size() * sizeof(float));
        // Layout: Pos(3) + Normal(3)
        vbo->setLayout({ 
            { ShaderDataType::Float3, "a_Pos" }, 
            { ShaderDataType::Float3, "a_Normal" } 
        });
        m_vertex_array->addVertexBuffer(vbo);

        auto ebo = RendererFactory::createIndexBuffer(indices.data(), indices.size());
        m_vertex_array->setIndexBuffer(ebo);

        // 4. 创建 Shader (简单的法线可视化 Shader)
        std::string vertSrc = R"(
            #version 450 core
            layout(location = 0) in vec3 a_Pos;
            layout(location = 1) in vec3 a_Normal;

            layout(std140, binding = 0) uniform Camera {
                mat4 u_ViewProjection;
            };

            out vec3 v_Normal;
            
            void main() { 
                v_Normal = a_Normal;
                gl_Position = u_ViewProjection * vec4(a_Pos, 1.0); 
            }
        )";
        std::string fragSrc = R"(
            #version 450 core
            in vec3 v_Normal;
            out vec4 color;
            void main() { 
                //把法线映射到 0-1 范围显示颜色
                color = vec4(v_Normal * 0.5 + 0.5, 1.0); 
            }
        )";
        m_shader = RendererFactory::createShader("SphereShader", vertSrc, fragSrc);
    }
} // namespace NexAur
