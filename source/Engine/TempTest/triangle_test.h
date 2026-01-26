#pragma once
#include "Core/Base.h"
#include "Core/Time/TimeStep.h"
#include "Function/Renderer/RHI/vertex_array.h"
#include "Function/Renderer/RHI/shader.h"

namespace NexAur {
    class NEXAUR_API TriangleTest {
    public:
        void init();  // 创建 VBO, VAO, Shader
        void onUpdate(TimeStep ts); // 执行绘制命令
        
    private:
        std::shared_ptr<VertexArray> m_VertexArray;
        std::shared_ptr<Shader> m_Shader;
    };

}