#pragma once
#include "Core/Base.h"
#include "Function/Renderer/data/render_data.h"

namespace NexAur {
    class RenderContext {
    public:
        void swapBuffers() {
            m_write_index = 1 - m_write_index;
            getWriteData().clear();
        }

        RenderDataPacket& getWriteData() { return render_data[m_write_index]; }
        const RenderDataPacket& getReadData() const { return render_data[1 - m_write_index]; }

    private:
        RenderDataPacket render_data[2]; // 双缓冲渲染数据包
        int m_write_index = 0;
    };
} // namespace NexAur
