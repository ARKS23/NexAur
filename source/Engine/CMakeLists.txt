# Engine MakeLists
project(NexAurEngine)

# 动态库和静态库选项
option(NEXAUR_BUILD_SHARED "Build engine as shared library" ON)

# 显式维护 Engine 主线源码，避免 legacy/实验文件因目录位置被自动编入主库。
set(NEXAUR_ENGINE_ROOT_FILES
    NexAur.h
    pch.h
    pch.cpp
    engine.h
    engine.cpp
)

set(NEXAUR_ENGINE_CORE_FILES
    Core/Base.h
    Core/UUID.h
    Core/UUID.cpp
    Core/Events/event.h
    Core/Events/key_event.h
    Core/Events/mouse_event.h
    Core/Events/window_event.h
    Core/Log/log_system.h
    Core/Log/log_system.cpp
    Core/Time/Clock.h
    Core/Time/Clock.cpp
    Core/Time/TimeStep.h
)

set(NEXAUR_ENGINE_EDITOR_FILES
    Editor/editor_context.h
    Editor/editor_layer.h
    Editor/editor_layer.cpp
    Editor/Panels/editor_panel.h
    Editor/Panels/properties_panel.h
    Editor/Panels/properties_panel.cpp
    Editor/Panels/scene_hierarchy_panel.h
    Editor/Panels/scene_hierarchy_panel.cpp
    Editor/Panels/viewport_panel.h
    Editor/Panels/viewport_panel.cpp
)

set(NEXAUR_ENGINE_FUNCTION_FILES
    Function/File/file_system.h
    Function/File/file_system.cpp
    Function/Global/global_context.h
    Function/Global/global_context.cpp
    Function/Input/input_system.h
    Function/Input/input_system_glfw.h
    Function/Input/input_system_glfw.cpp
    Function/Input/KeyCode/key_codes.h
    Function/Input/KeyCode/mouse_codes.h
    Function/Resource/asset_handle.h
    Function/Resource/asset_manager.h
    Function/Resource/asset_manager.cpp
    Function/Resource/asset_metadata.h
    Function/Resource/ibl_builder.h
    Function/Resource/ibl_builder.cpp
    Function/Resource/mesh.h
    Function/Resource/model.h
    Function/Resource/model.cpp
    Function/Resource/procedural_model_factory.h
    Function/Resource/procedural_model_factory.cpp
    Function/Scene/component.h
    Function/Scene/entity.h
    Function/Scene/entity.cpp
    Function/Scene/scene_manager.h
    Function/Scene/scene_manager.cpp
    Function/Scene/scene_v2.h
    Function/Scene/scene_v2.cpp
    Function/UI/ui_system.h
    Function/UI/ui_system.cpp
)

set(NEXAUR_ENGINE_RENDERER_FILES
    Function/Renderer/camera.h
    Function/Renderer/editor_camera.h
    Function/Renderer/editor_camera.cpp
    Function/Renderer/window_system.h
    Function/Renderer/window_system.cpp
    Function/Renderer/data/render_context.h
    Function/Renderer/data/render_data.h
    Function/Renderer/data/render_uploader.h
    Function/Renderer/data/render_uploader.cpp
    Function/Renderer/Passes/interface_render_pass.h
    Function/Renderer/Passes/interface_render_pass.cpp
    Function/Renderer/Passes/shadow_pass_v2.h
    Function/Renderer/Passes/shadow_pass_v2.cpp
    Function/Renderer/Passes/skybox_pass_v2.h
    Function/Renderer/Passes/skybox_pass_v2.cpp
    Function/Renderer/RHI/buffer.h
    Function/Renderer/RHI/buffer.cpp
    Function/Renderer/RHI/definitions.h
    Function/Renderer/RHI/framebuffer.h
    Function/Renderer/RHI/framebuffer.cpp
    Function/Renderer/RHI/graphics_context.h
    Function/Renderer/RHI/graphics_context.cpp
    Function/Renderer/RHI/material.h
    Function/Renderer/RHI/material.cpp
    Function/Renderer/RHI/render_forward_pipeline.h
    Function/Renderer/RHI/render_forward_pipeline.cpp
    Function/Renderer/RHI/render_pipeline.h
    Function/Renderer/RHI/renderer.h
    Function/Renderer/RHI/renderer.cpp
    Function/Renderer/RHI/renderer_API.h
    Function/Renderer/RHI/renderer_API.cpp
    Function/Renderer/RHI/renderer_command.h
    Function/Renderer/RHI/renderer_command.cpp
    Function/Renderer/RHI/renderer_system.h
    Function/Renderer/RHI/renderer_system.cpp
    Function/Renderer/RHI/shader.h
    Function/Renderer/RHI/shader.cpp
    Function/Renderer/RHI/texture.h
    Function/Renderer/RHI/texture.cpp
    Function/Renderer/RHI/uniform_buffer.h
    Function/Renderer/RHI/uniform_buffer.cpp
    Function/Renderer/RHI/vertex_array.h
    Function/Renderer/RHI/vertex_array.cpp
    Function/Renderer/Platform/OpenGL/opengl_buffer.h
    Function/Renderer/Platform/OpenGL/opengl_buffer.cpp
    Function/Renderer/Platform/OpenGL/opengl_framebuffer.h
    Function/Renderer/Platform/OpenGL/opengl_framebuffer.cpp
    Function/Renderer/Platform/OpenGL/opengl_graphics_context.h
    Function/Renderer/Platform/OpenGL/opengl_graphics_context.cpp
    Function/Renderer/Platform/OpenGL/opengl_renderer_api.h
    Function/Renderer/Platform/OpenGL/opengl_renderer_api.cpp
    Function/Renderer/Platform/OpenGL/opengl_shader.h
    Function/Renderer/Platform/OpenGL/opengl_shader.cpp
    Function/Renderer/Platform/OpenGL/opengl_texture.h
    Function/Renderer/Platform/OpenGL/opengl_texture.cpp
    Function/Renderer/Platform/OpenGL/opengl_uniform_buffer.h
    Function/Renderer/Platform/OpenGL/opengl_uniform_buffer.cpp
    Function/Renderer/Platform/OpenGL/opengl_vertex_array.h
    Function/Renderer/Platform/OpenGL/opengl_vertex_array.cpp
)

set(NEXAUR_ENGINE_FILES
    ${NEXAUR_ENGINE_ROOT_FILES}
    ${NEXAUR_ENGINE_CORE_FILES}
    ${NEXAUR_ENGINE_EDITOR_FILES}
    ${NEXAUR_ENGINE_FUNCTION_FILES}
    ${NEXAUR_ENGINE_RENDERER_FILES}
)

# 创建对应的库类型
if (NEXAUR_BUILD_SHARED)
    add_library(NexAurEngine SHARED ${NEXAUR_ENGINE_FILES})
    target_compile_definitions(NexAurEngine PUBLIC NEXAUR_DYNAMIC_LINK) # 定义宏,Base中会用到
    target_compile_definitions(NexAurEngine PRIVATE NX_BUILD_DLL)
    message(STATUS "NexAur Engine: Building SHARED Library (DLL)")
else()
    add_library(NexAurEngine STATIC ${NEXAUR_ENGINE_FILES})
    target_compile_definitions(NexAurEngine PUBLIC NEXAUR_STATIC_LINK)
    message(STATUS "NexAur Engine: Building STATIC Library (LIB)")
endif()

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${NEXAUR_ENGINE_FILES})

# 预编译头
target_precompile_headers(NexAurEngine PRIVATE pch.h)

# 设置头文件路径
target_include_directories(NexAurEngine PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/external/entt/src"
    "${CMAKE_SOURCE_DIR}/external/glm"
    "${CMAKE_SOURCE_DIR}/external/spdlog/include"
    "${CMAKE_SOURCE_DIR}/external/glfw/include"
    "${CMAKE_SOURCE_DIR}/external/glad/include"
    "${CMAKE_SOURCE_DIR}/external/stb"
    "${CMAKE_SOURCE_DIR}/external/assimp/include"
    "${CMAKE_SOURCE_DIR}/external/imgui"
    "${CMAKE_SOURCE_DIR}/external/imgui/backends"
)

# 链接第三方库
target_link_libraries(NexAurEngine PUBLIC
    EnTT
    glad
    glfw
    glm
    spdlog
    stb
    assimp
    imgui
)
