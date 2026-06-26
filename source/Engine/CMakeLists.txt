# Engine MakeLists
project(NexAurEngine)

# 动态库和静态库选项
option(NEXAUR_BUILD_SHARED "Build engine as shared library" ON)

function(nexaur_select_dependency_target output_name)
    foreach(candidate IN LISTS ARGN)
        if(TARGET ${candidate})
            set(${output_name} ${candidate} PARENT_SCOPE)
            return()
        endif()
    endforeach()

    message(FATAL_ERROR "None of the expected targets exist: ${ARGN}")
endfunction()

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
    Core/Module/builtin_module_names.h
    Core/Module/core_engine_module.h
    Core/Module/core_engine_module.cpp
    Core/Module/engine_module.h
    Core/Module/module_manager.h
    Core/Module/module_manager.cpp
    Core/Time/Clock.h
    Core/Time/Clock.cpp
    Core/Time/TimeStep.h
)

set(NEXAUR_ENGINE_EDITOR_FILES
    Editor/editor_module.h
    Editor/editor_module.cpp
    Editor/editor_context.h
    Editor/editor_layer.h
    Editor/editor_layer.cpp
    Editor/editor_services.h
    Editor/Camera/editor_camera.h
    Editor/Camera/editor_camera.cpp
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
    Function/File/file_system_module.h
    Function/File/file_system_module.cpp
    Function/Global/global_context.h
    Function/Global/global_context.cpp
    Function/Input/input_state.h
    Function/Input/input_system.h
    Function/Input/input_system_glfw.h
    Function/Input/input_system_glfw.cpp
    Function/Input/KeyCode/key_codes.h
    Function/Input/KeyCode/mouse_codes.h
    Function/Platform/platform_module.h
    Function/Platform/platform_module.cpp
    Function/Platform/platform_services.h
    Function/Platform/window_graphics_api.h
    Function/Platform/window_system.h
    Function/Platform/window_system.cpp
    Function/Resource/asset_handle.h
    Function/Resource/asset_manager.h
    Function/Resource/asset_manager.cpp
    Function/Resource/asset_metadata.h
    Function/Resource/resource_module.h
    Function/Resource/resource_module.cpp
    Function/Resource/mesh.h
    Function/Resource/model.h
    Function/Resource/model.cpp
    Function/Resource/procedural_model_builder.h
    Function/Resource/procedural_model_builder.cpp
    Function/Scene/component.h
    Function/Scene/entity.h
    Function/Scene/entity.cpp
    Function/Scene/scene_factory.h
    Function/Scene/scene_factory.cpp
    Function/Scene/scene_manager.h
    Function/Scene/scene_manager.cpp
    Function/Scene/scene_service.h
    Function/Scene/runtime_module.h
    Function/Scene/runtime_module.cpp
    Function/Scene/scene_v2.h
    Function/Scene/scene_v2.cpp
    Function/UI/ui_module.h
    Function/UI/ui_module.cpp
    Function/UI/ui_system.h
    Function/UI/ui_system.cpp
)

set(NEXAUR_ENGINE_RENDERER_FILES
    Function/Renderer/renderer_module.h
    Function/Renderer/renderer_module.cpp
    Function/Renderer/data/render_context.h
    Function/Renderer/data/render_data.h
    Function/Renderer/renderer_service.h
    Function/Renderer/renderer_service_types.h
)

set(NEXAUR_ENGINE_VULKAN_SHADER_FILES
    "${CMAKE_SOURCE_DIR}/assets/shaders/Renderer/Vulkan/vulkan_forward.hlsl"
    "${CMAKE_SOURCE_DIR}/assets/shaders/Renderer/Vulkan/vulkan_object_id.hlsl"
)

set_source_files_properties(
    ${NEXAUR_ENGINE_VULKAN_SHADER_FILES}
    PROPERTIES
        HEADER_FILE_ONLY TRUE
        VS_TOOL_OVERRIDE "None"
)

set(NEXAUR_ENGINE_RENDERER_VULKAN_FILES
    Function/Renderer/data/render_scene_frame.h
    Function/Renderer/data/render_view.h
    Function/Renderer/frontend/render_scene_frame_builder.h
    Function/Renderer/frontend/render_scene_frame_builder.cpp
    Function/Renderer/Vulkan/frontend/vulkan_draw_list.h
    Function/Renderer/Vulkan/frontend/vulkan_draw_list_builder.h
    Function/Renderer/Vulkan/frontend/vulkan_draw_list_builder.cpp
    Function/Renderer/Vulkan/frontend/vulkan_render_data_translator.h
    Function/Renderer/Vulkan/frontend/vulkan_render_data_translator.cpp
    Function/Renderer/Vulkan/passes/vulkan_forward_pass.h
    Function/Renderer/Vulkan/passes/vulkan_forward_pass.cpp
    Function/Renderer/Vulkan/passes/vulkan_object_id_pass.h
    Function/Renderer/Vulkan/passes/vulkan_object_id_pass.cpp
    Function/Renderer/Vulkan/vulkan_render_target.h
    Function/Renderer/Vulkan/vulkan_resource_context.h
    Function/Renderer/Vulkan/resources/vulkan_material_resource.h
    Function/Renderer/Vulkan/resources/vulkan_material_resource.cpp
    Function/Renderer/Vulkan/resources/vulkan_mesh_resource.h
    Function/Renderer/Vulkan/resources/vulkan_mesh_resource.cpp
    Function/Renderer/Vulkan/resources/vulkan_model_resource.h
    Function/Renderer/Vulkan/resources/vulkan_model_resource.cpp
    Function/Renderer/Vulkan/targets/vulkan_viewport_target.h
    Function/Renderer/Vulkan/targets/vulkan_viewport_target.cpp
    Function/Renderer/Vulkan/targets/vulkan_picking_target.h
    Function/Renderer/Vulkan/targets/vulkan_picking_target.cpp
    Function/Renderer/Vulkan/ui/vulkan_imgui_renderer.h
    Function/Renderer/Vulkan/ui/vulkan_imgui_renderer.cpp
    Function/Renderer/Vulkan/vulkan_renderer_system.h
    Function/Renderer/Vulkan/vulkan_renderer_system.cpp
    Function/Renderer/Vulkan/vulkan_render_resource_cache.h
    Function/Renderer/Vulkan/vulkan_render_resource_cache.cpp
)

set(NEXAUR_ENGINE_THIRD_PARTY_FILES
    ThirdParty/stb_image.cpp
)

set(NEXAUR_ENGINE_CODE_FILES
    ${NEXAUR_ENGINE_ROOT_FILES}
    ${NEXAUR_ENGINE_CORE_FILES}
    ${NEXAUR_ENGINE_EDITOR_FILES}
    ${NEXAUR_ENGINE_FUNCTION_FILES}
    ${NEXAUR_ENGINE_RENDERER_FILES}
    ${NEXAUR_ENGINE_RENDERER_VULKAN_FILES}
    ${NEXAUR_ENGINE_THIRD_PARTY_FILES}
)

set(NEXAUR_ENGINE_FILES
    ${NEXAUR_ENGINE_CODE_FILES}
    ${NEXAUR_ENGINE_VULKAN_SHADER_FILES}
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

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${NEXAUR_ENGINE_CODE_FILES})
source_group(TREE "${CMAKE_SOURCE_DIR}/assets" PREFIX "Assets" FILES ${NEXAUR_ENGINE_VULKAN_SHADER_FILES})

target_compile_features(NexAurEngine PUBLIC cxx_std_20)

# 预编译头
target_precompile_headers(NexAurEngine PRIVATE pch.h)

# 设置头文件路径
target_include_directories(NexAurEngine PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

# 链接第三方库
find_package(assimp CONFIG REQUIRED)
find_package(EnTT CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(imguizmo CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(Stb REQUIRED)
find_package(Vulkan REQUIRED)
find_package(directx-dxc CONFIG REQUIRED)
find_package(vk-bootstrap CONFIG REQUIRED)
find_package(volk CONFIG REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)

nexaur_select_dependency_target(NEXAUR_ASSIMP_TARGET assimp::assimp assimp)
nexaur_select_dependency_target(NEXAUR_ENTT_TARGET EnTT::EnTT EnTT)
nexaur_select_dependency_target(NEXAUR_GLFW_TARGET glfw glfw3::glfw glfw3)
nexaur_select_dependency_target(NEXAUR_GLM_TARGET glm::glm glm)
nexaur_select_dependency_target(NEXAUR_IMGUI_TARGET imgui::imgui imgui)
nexaur_select_dependency_target(NEXAUR_IMGUIZMO_TARGET imguizmo::imguizmo imguizmo)
nexaur_select_dependency_target(NEXAUR_SPDLOG_TARGET spdlog::spdlog spdlog)

target_include_directories(NexAurEngine PUBLIC
    "${Stb_INCLUDE_DIR}"
)

target_link_libraries(NexAurEngine PUBLIC
    ${NEXAUR_ASSIMP_TARGET}
    ${NEXAUR_ENTT_TARGET}
    ${NEXAUR_GLFW_TARGET}
    ${NEXAUR_GLM_TARGET}
    ${NEXAUR_IMGUI_TARGET}
    ${NEXAUR_IMGUIZMO_TARGET}
    ${NEXAUR_SPDLOG_TARGET}
)

target_link_libraries(NexAurEngine PRIVATE
    Vulkan::Vulkan
    vk-bootstrap::vk-bootstrap
    volk::volk
    GPUOpen::VulkanMemoryAllocator
)

set(NEXAUR_VULKAN_SHADER_SOURCE_DIR "${CMAKE_SOURCE_DIR}/assets/shaders/Renderer/Vulkan")
set(NEXAUR_VULKAN_SHADER_OUTPUT_DIR "$<TARGET_FILE_DIR:NexAurEngine>/shaders/Renderer/Vulkan")
add_custom_command(TARGET NexAurEngine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${NEXAUR_VULKAN_SHADER_OUTPUT_DIR}"
    COMMAND "${DIRECTX_DXC_TOOL}" -spirv -fspv-target-env=vulkan1.3 -HV 2021 -T vs_6_0 -E VSMain
            -Fo "${NEXAUR_VULKAN_SHADER_OUTPUT_DIR}/vulkan_forward.vert.spv"
            "${NEXAUR_VULKAN_SHADER_SOURCE_DIR}/vulkan_forward.hlsl"
    COMMAND "${DIRECTX_DXC_TOOL}" -spirv -fspv-target-env=vulkan1.3 -HV 2021 -T ps_6_0 -E PSMain
            -Fo "${NEXAUR_VULKAN_SHADER_OUTPUT_DIR}/vulkan_forward.frag.spv"
            "${NEXAUR_VULKAN_SHADER_SOURCE_DIR}/vulkan_forward.hlsl"
    COMMAND "${DIRECTX_DXC_TOOL}" -spirv -fspv-target-env=vulkan1.3 -HV 2021 -T vs_6_0 -E VSMain
            -Fo "${NEXAUR_VULKAN_SHADER_OUTPUT_DIR}/vulkan_object_id.vert.spv"
            "${NEXAUR_VULKAN_SHADER_SOURCE_DIR}/vulkan_object_id.hlsl"
    COMMAND "${DIRECTX_DXC_TOOL}" -spirv -fspv-target-env=vulkan1.3 -HV 2021 -T ps_6_0 -E PSMain
            -Fo "${NEXAUR_VULKAN_SHADER_OUTPUT_DIR}/vulkan_object_id.frag.spv"
            "${NEXAUR_VULKAN_SHADER_SOURCE_DIR}/vulkan_object_id.hlsl"
    COMMENT "Compiling Vulkan renderer HLSL shaders"
    VERBATIM
)
