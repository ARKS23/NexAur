#pragma once

#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

#include "Core/Base.h"
#include "Function/Input/KeyCode/key_codes.h"
#include "Function/Input/KeyCode/mouse_codes.h"
#include "Function/Input/input_state.h"
#include "Function/Platform/window_graphics_api.h"

namespace NexAur {
    class Event;

    // PlatformModule 对外暴露的窗口能力，屏蔽 GLFW/平台窗口细节。
    class NEXAUR_API WindowService {
    public:
        using EventCallbackFn = std::function<void(Event&)>;

        virtual ~WindowService() = default;

        virtual void* getNativeWindow() const = 0;
        virtual WindowGraphicsAPI getGraphicsAPI() const = 0;
        virtual std::vector<const char*> getRequiredVulkanInstanceExtensions() const = 0;
        virtual std::pair<uint32_t, uint32_t> getSize() const = 0;
        virtual void setEventCallback(const EventCallbackFn& callback) = 0;
        virtual void setTitle(const char* title) = 0;
        virtual void present() = 0;
        virtual void pollEvents() = 0;
    };

    // 输入只暴露快照读取接口。EditorCamera 等使用者不再直接依赖 GLFW。
    class NEXAUR_API InputService {
    public:
        virtual ~InputService() = default;

        virtual void update() = 0;
        virtual const InputState& getState() const = 0;

        virtual bool isKeyPressed(KeyCode key_code) const {
            return getState().isKeyPressed(key_code);
        }

        virtual bool isMouseButtonPressed(MouseCode mouse_code) const {
            return getState().isMouseButtonPressed(mouse_code);
        }

        virtual std::pair<float, float> getMousePosition() const {
            return getState().getMousePosition();
        }
    };
} // namespace NexAur
