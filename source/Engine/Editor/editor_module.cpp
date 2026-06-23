#include "pch.h"
#include "editor_module.h"

#include "Core/Log/log_system.h"
#include "Core/Module/builtin_module_names.h"
#include "Core/Module/engine_module.h"
#include "Editor/editor_context.h"
#include "Editor/editor_layer.h"
#include "Editor/editor_services.h"
#include "Function/Input/input_system.h"
#include "Function/Platform/platform_services.h"
#include "Function/Renderer/RHI/renderer_service.h"
#include "Function/Renderer/data/render_context.h"
#include "Function/Scene/scene_service.h"
#include "Function/UI/ui_system.h"

namespace NexAur {
    namespace {
        class EditorModule final : public EngineModule {
        public:
            ModuleInfo getInfo() const override {
                return {
                    BuiltinModuleNames::Editor,
                    ModuleStage::Editor,
                    {
                        BuiltinModuleNames::UI,
                        BuiltinModuleNames::Runtime,
                        BuiltinModuleNames::Renderer,
                        BuiltinModuleNames::Platform
                    }
                };
            }

            void initialize(ModuleContext& context) override {
                // EditorContext 是编辑器层的小上下文，只保存编辑器实际需要的窄服务。
                // 这样 Panel/EditorLayer 不需要直接访问 RunTimeGlobalContext。
                m_context = std::make_shared<EditorContext>();
                m_context->scene_service = context.registry.getService<SceneService>();
                m_context->renderer_service = context.registry.getService<RendererService>();
                m_context->input_service = context.registry.getService<InputService>();
                m_context->render_context = context.registry.getService<RenderContext>();
                m_context->ui_service = context.registry.getService<UIService>();
                m_context->active_scene = m_context->scene_service ? m_context->scene_service->getActiveScene() : nullptr;

                m_editor_layer = std::make_shared<EditorLayer>(m_context);
                // SelectionService 由 EditorLayer 实现，Panel 通过服务写选择状态。
                m_context->selection_service = std::static_pointer_cast<SelectionService>(m_editor_layer);

                context.registry.registerService<EditorService>(std::static_pointer_cast<EditorService>(m_editor_layer));
                context.registry.registerService<SelectionService>(std::static_pointer_cast<SelectionService>(m_editor_layer));
                context.registry.registerService<ViewportService>(std::static_pointer_cast<ViewportService>(m_editor_layer));

                NX_CORE_INFO("Editor module initialized.");
            }

            void postUpdate(const TickContext& tick_context) override {
                // 放在 postUpdate 是为了让 Scene 先提取渲染数据，
                // 然后 EditorCamera 再覆盖 viewport 预览相机。
                if (m_editor_layer && m_editor_layer->isEnabled()) {
                    m_editor_layer->update(tick_context.delta_time);
                }
            }

            void renderUI(const TickContext& tick_context) override {
                (void)tick_context;
                if (m_editor_layer && m_editor_layer->isEnabled()) {
                    m_editor_layer->renderUI();
                }
            }

            void onEvent(Event& event) override {
                if (m_editor_layer && m_editor_layer->isEnabled()) {
                    m_editor_layer->onEvent(event);
                }
            }

            void shutdown(ModuleContext& context) override {
                context.registry.resetService<ViewportService>();
                context.registry.resetService<SelectionService>();
                context.registry.resetService<EditorService>();
                m_editor_layer.reset();
                m_context.reset();
            }

        private:
            std::shared_ptr<EditorContext> m_context;
            std::shared_ptr<EditorLayer> m_editor_layer;
        };
    } // namespace

    std::unique_ptr<EngineModule> createEditorModule() {
        return std::make_unique<EditorModule>();
    }
} // namespace NexAur
