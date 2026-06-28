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

        const RenderDebugVisualizationOptions& getDebugVisualizationOptions() const {
            return m_debug_visualization_options;
        }

        void setDebugVisualizationOptions(const RenderDebugVisualizationOptions& options) {
            m_debug_visualization_options = options;
        }

        const RenderSettings& getRenderSettings() const {
            return m_render_settings;
        }

        void setRenderSettings(const RenderSettings& settings) {
            m_render_settings = settings;
        }

    private:
        RenderDataPacket render_data[2]; // 双缓冲渲染数据包
        RenderSettings m_render_settings;
        RenderDebugVisualizationOptions m_debug_visualization_options;
        int m_write_index = 0;
    };
} // namespace NexAur
