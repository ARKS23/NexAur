#include "pch.h"
#include "ui_module.h"

#include "Core/Log/log_system.h"
#include "Core/Module/builtin_module_names.h"
#include "Core/Module/engine_module.h"
#include "Editor/editor_services.h"
#include "Function/Platform/platform_services.h"
#include "Function/UI/ui_system.h"

namespace NexAur {
    namespace {
        class UIModule final : public EngineModule {
        public:
            ModuleInfo getInfo() const override {
                return {
                    BuiltinModuleNames::UI,
                    ModuleStage::UI,
                    { BuiltinModuleNames::Platform, BuiltinModuleNames::Renderer }
                };
            }

            void initialize(ModuleContext& context) override {
                // UI 需要在事件路由中查询 ViewportService，因此保存 registry 指针。
                // 这里仍是模块顶层使用，不把 registry 继续传入深层业务对象。
                m_registry = &context.registry;
                m_ui_system = std::make_shared<UISystem>();
                m_ui_system->init(context.registry.getService<WindowService>());

                context.registry.registerService<UIService>(std::static_pointer_cast<UIService>(m_ui_system));
                context.registry.registerService<UISystem>(m_ui_system);

                NX_CORE_INFO("UI module initialized.");
            }

            void onEvent(Event& event) override {
                if (!m_ui_system) {
                    return;
                }

                m_ui_system->onEvent(event);
                // ImGui 捕获输入时，在这里截断后续 Editor/Runtime 输入响应。
                if (shouldBlockInputForUICapture(event)) {
                    event.handled = true;
                }
            }

            void shutdown(ModuleContext& context) override {
                if (m_ui_system) {
                    m_ui_system->shutdown();
                }

                context.registry.resetService<UIService>();
                context.registry.resetService<UISystem>();
                m_ui_system.reset();
                m_registry = nullptr;
            }

        private:
            bool shouldBlockInputForUICapture(Event& event) const {
                const bool keyboard_event = event.isInCategory(EventCategoryKeyboard);
                const bool mouse_event =
                    event.isInCategory(EventCategoryMouse) ||
                    event.isInCategory(EventCategoryMouseButton);

                std::shared_ptr<ViewportService> viewport_service =
                    m_registry ? m_registry->getService<ViewportService>() : nullptr;
                const bool mouse_over_viewport = viewport_service && viewport_service->isViewportHovered();
                const bool keyboard_targets_viewport =
                    viewport_service && (viewport_service->isViewportFocused() || viewport_service->isViewportHovered());

                if (keyboard_event) {
                    if (m_ui_system->wantsTextInput()) {
                        return true;
                    }
                    if (m_ui_system->wantsCaptureKeyboard() && !keyboard_targets_viewport) {
                        return true;
                    }
                }

                if (mouse_event && m_ui_system->wantsCaptureMouse()) {
                    // 鼠标在 viewport 上时允许编辑器视口继续处理 picking/滚轮等交互。
                    return !mouse_over_viewport;
                }

                return false;
            }

        private:
            ModuleRegistry* m_registry = nullptr;
            std::shared_ptr<UISystem> m_ui_system;
        };
    } // namespace

    std::unique_ptr<EngineModule> createUIModule() {
        return std::make_unique<UIModule>();
    }
} // namespace NexAur
