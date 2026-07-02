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
    Editor/editor_config.h
    Editor/editor_config.cpp
    Editor/editor_context.h
    Editor/editor_tool_state.h
    Editor/editor_layer.h
    Editor/editor_layer.cpp
    Editor/editor_services.h
    Editor/Commands/editor_command.h
    Editor/Commands/editor_command.cpp
    Editor/Style/editor_icons.h
    Editor/Style/editor_fonts.h
    Editor/Style/editor_fonts.cpp
    Editor/Style/editor_style.h
    Editor/Style/editor_style.cpp
    Editor/Style/editor_theme.h
    Editor/Style/editor_theme.cpp
    Editor/Widgets/editor_widgets.h
    Editor/Widgets/editor_widgets.cpp
    Editor/Widgets/editor_property_drawer.h
    Editor/Widgets/editor_property_drawer.cpp
    Editor/Camera/editor_camera.h
    Editor/Camera/editor_camera.cpp
    Editor/Panels/console_panel.h
    Editor/Panels/console_panel.cpp
    Editor/Panels/editor_panel.h
    Editor/Panels/placeholder_panel.h
    Editor/Panels/placeholder_panel.cpp
    Editor/Panels/project_panel.h
    Editor/Panels/project_panel.cpp
    Editor/Panels/properties_panel.h
    Editor/Panels/properties_panel.cpp
    Editor/Panels/render_settings_panel.h
    Editor/Panels/render_settings_panel.cpp
    Editor/Panels/renderer_debug_panel.h
    Editor/Panels/renderer_debug_panel.cpp
    Editor/Panels/scene_hierarchy_panel.h
    Editor/Panels/scene_hierarchy_panel.cpp
    Editor/Panels/viewport_panel.h
    Editor/Panels/viewport_panel.cpp
)

set(NEXAUR_ENGINE_FUNCTION_FILES
    Function/Audio/audio_module.h
    Function/Audio/audio_module.cpp
    Function/Audio/audio_service.h
    Function/Audio/audio_system.h
    Function/Audio/audio_system_miniaudio.cpp
    Function/File/file_system.h
    Function/File/file_system.cpp
    Function/File/file_system_module.h
    Function/File/file_system_module.cpp
    Function/Game/game_module.h
    Function/Game/game_module.cpp
    Function/Game/game_state.h
    Function/Game/game_state.cpp
    Function/Game/gameplay_systems.h
    Function/Game/gameplay_systems.cpp
    Function/Game/prefab_factory.h
    Function/Game/prefab_factory.cpp
    Function/Game/runtime_camera_controller_system.h
    Function/Game/runtime_camera_controller_system.cpp
    Function/Game/runtime_hud.h
    Function/Game/runtime_hud.cpp
    Function/Global/global_context.h
    Function/Global/global_context.cpp
    Function/Input/input_action_module.h
    Function/Input/input_action_module.cpp
    Function/Input/input_action_system.h
    Function/Input/input_action_system.cpp
    Function/Input/input_state.h
    Function/Input/input_system.h
    Function/Input/input_system_glfw.h
    Function/Input/input_system_glfw.cpp
    Function/Input/KeyCode/key_codes.h
    Function/Input/KeyCode/mouse_codes.h
    Function/Physics/collision_query.h
    Function/Physics/collision_query.cpp
    Function/Physics/physics_types.h
    Function/Physics/trigger_overlap_system.h
    Function/Physics/trigger_overlap_system.cpp
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
    Function/Resource/Import/assimp/assimp_model_importer.h
    Function/Resource/Import/assimp/assimp_model_importer.cpp
    Function/Resource/Import/gltf/tiny_gltf_importer.h
    Function/Resource/Import/gltf/tiny_gltf_importer.cpp
    Function/Resource/Import/model_importer.h
    Function/Resource/Import/model_importer_registry.h
    Function/Resource/Import/model_importer_registry.cpp
    Function/Resource/Import/model_import_types.h
    Function/Resource/environment_map_asset.h
    Function/Resource/environment_map_asset.cpp
    Function/Resource/material_types.h
    Function/Resource/material_asset.h
    Function/Resource/material_asset.cpp
    Function/Resource/resource_module.h
    Function/Resource/resource_module.cpp
    Function/Resource/mesh.h
    Function/Resource/model.h
    Function/Resource/model.cpp
    Function/Resource/procedural_model_builder.h
    Function/Resource/procedural_model_builder.cpp
    Function/Resource/texture_types.h
    Function/Resource/texture_asset.h
    Function/Resource/texture_asset.cpp
    Function/Resource/texture_loader.h
    Function/Resource/texture_loader.cpp
    Function/Scene/component.h
    Function/Scene/collision_component.h
    Function/Scene/entity.h
    Function/Scene/entity.cpp
    Function/Scene/gameplay_component.h
    Function/Scene/scene_factory.h
    Function/Scene/scene_factory.cpp
    Function/Scene/scene_manager.h
    Function/Scene/scene_manager.cpp
    Function/Scene/scene_serializer.h
    Function/Scene/scene_serializer.cpp
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
    Function/Renderer/renderer_debug_service.h
    Function/Renderer/data/render_context.h
    Function/Renderer/data/render_data.h
    Function/Renderer/data/render_debug_draw_builder.h
    Function/Renderer/data/render_debug_draw_builder.cpp
    Function/Renderer/data/render_debug_draw.h
    Function/Renderer/data/render_settings.h
    Function/Renderer/data/render_shadow_cascade.h
    Function/Renderer/renderer_service.h
    Function/Renderer/renderer_service_types.h
)

set(NEXAUR_ENGINE_VULKAN_SHADER_FILES
    "${CMAKE_SOURCE_DIR}/assets/shaders/Renderer/Vulkan/vulkan_forward.hlsl"
    "${CMAKE_SOURCE_DIR}/assets/shaders/Renderer/Vulkan/vulkan_object_id.hlsl"
    "${CMAKE_SOURCE_DIR}/assets/shaders/Renderer/Vulkan/vulkan_skybox.hlsl"
    "${CMAKE_SOURCE_DIR}/assets/shaders/Renderer/Vulkan/vulkan_shadow_depth.hlsl"
    "${CMAKE_SOURCE_DIR}/assets/shaders/Renderer/Vulkan/vulkan_debug_draw.hlsl"
    "${CMAKE_SOURCE_DIR}/assets/shaders/Renderer/Vulkan/vulkan_post_process.hlsl"
    "${CMAKE_SOURCE_DIR}/assets/shaders/Renderer/Vulkan/common/color_grading.hlsli"
    "${CMAKE_SOURCE_DIR}/assets/shaders/Renderer/Vulkan/common/pbr_brdf.hlsli"
    "${CMAKE_SOURCE_DIR}/assets/shaders/Renderer/Vulkan/common/pbr_ibl.hlsli"
    "${CMAKE_SOURCE_DIR}/assets/shaders/Renderer/Vulkan/common/pbr_material.hlsli"
    "${CMAKE_SOURCE_DIR}/assets/shaders/Renderer/Vulkan/common/pbr_reflection_probe.hlsli"
    "${CMAKE_SOURCE_DIR}/assets/shaders/Renderer/Vulkan/common/shadow_sampling.hlsli"
    "${CMAKE_SOURCE_DIR}/assets/shaders/Renderer/Vulkan/common/vulkan_fullscreen_triangle.hlsli"
    "${CMAKE_SOURCE_DIR}/assets/shaders/Renderer/Vulkan/ao/vulkan_ssao.hlsl"
    "${CMAKE_SOURCE_DIR}/assets/shaders/Renderer/Vulkan/ao/vulkan_ao_blur.hlsl"
    "${CMAKE_SOURCE_DIR}/assets/shaders/Renderer/Vulkan/bloom/vulkan_bloom_downsample.hlsl"
    "${CMAKE_SOURCE_DIR}/assets/shaders/Renderer/Vulkan/bloom/vulkan_bloom_upsample.hlsl"
    "${CMAKE_SOURCE_DIR}/assets/shaders/Renderer/Vulkan/bloom/vulkan_bloom_composite.hlsl"
    "${CMAKE_SOURCE_DIR}/assets/shaders/Renderer/Vulkan/reflection/vulkan_ssr_trace.hlsl"
    "${CMAKE_SOURCE_DIR}/assets/shaders/Renderer/Vulkan/post/vulkan_smaa_edge.hlsl"
    "${CMAKE_SOURCE_DIR}/assets/shaders/Renderer/Vulkan/post/vulkan_smaa_blend.hlsl"
    "${CMAKE_SOURCE_DIR}/assets/shaders/Renderer/Vulkan/post/vulkan_smaa_neighborhood.hlsl"
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
    Function/Renderer/Vulkan/descriptors/vulkan_descriptor_allocator.h
    Function/Renderer/Vulkan/descriptors/vulkan_descriptor_allocator.cpp
    Function/Renderer/Vulkan/descriptors/vulkan_descriptor_layout_cache.h
    Function/Renderer/Vulkan/descriptors/vulkan_descriptor_layout_cache.cpp
    Function/Renderer/Vulkan/descriptors/vulkan_descriptor_types.h
    Function/Renderer/Vulkan/descriptors/vulkan_descriptor_writer.h
    Function/Renderer/Vulkan/descriptors/vulkan_descriptor_writer.cpp
    Function/Renderer/Vulkan/frontend/vulkan_draw_list.h
    Function/Renderer/Vulkan/frontend/vulkan_draw_list_builder.h
    Function/Renderer/Vulkan/frontend/vulkan_draw_list_builder.cpp
    Function/Renderer/Vulkan/frontend/vulkan_debug_draw_types.h
    Function/Renderer/Vulkan/frontend/vulkan_render_data_translator.h
    Function/Renderer/Vulkan/frontend/vulkan_render_data_translator.cpp
    Function/Renderer/Vulkan/graph/vulkan_graph_executor.h
    Function/Renderer/Vulkan/graph/vulkan_graph_executor.cpp
    Function/Renderer/Vulkan/graph/vulkan_graph_resource.h
    Function/Renderer/Vulkan/graph/vulkan_pass_graph.h
    Function/Renderer/Vulkan/graph/vulkan_pass_graph.cpp
    Function/Renderer/Vulkan/passes/vulkan_debug_draw_pass.h
    Function/Renderer/Vulkan/passes/vulkan_debug_draw_pass.cpp
    Function/Renderer/Vulkan/passes/vulkan_ao_pass.h
    Function/Renderer/Vulkan/passes/vulkan_ao_pass.cpp
    Function/Renderer/Vulkan/passes/vulkan_bloom_pass.h
    Function/Renderer/Vulkan/passes/vulkan_bloom_pass.cpp
    Function/Renderer/Vulkan/passes/vulkan_ssr_pass.h
    Function/Renderer/Vulkan/passes/vulkan_ssr_pass.cpp
    Function/Renderer/Vulkan/passes/vulkan_forward_pass.h
    Function/Renderer/Vulkan/passes/vulkan_forward_pass.cpp
    Function/Renderer/Vulkan/passes/vulkan_object_id_pass.h
    Function/Renderer/Vulkan/passes/vulkan_object_id_pass.cpp
    Function/Renderer/Vulkan/passes/vulkan_post_process_pass.h
    Function/Renderer/Vulkan/passes/vulkan_post_process_pass.cpp
    Function/Renderer/Vulkan/passes/vulkan_smaa_pass.h
    Function/Renderer/Vulkan/passes/vulkan_smaa_pass.cpp
    Function/Renderer/Vulkan/passes/vulkan_shadow_pass.h
    Function/Renderer/Vulkan/passes/vulkan_shadow_pass.cpp
    Function/Renderer/Vulkan/passes/vulkan_skybox_pass.h
    Function/Renderer/Vulkan/passes/vulkan_skybox_pass.cpp
    Function/Renderer/Vulkan/pipeline/vulkan_pipeline_types.h
    Function/Renderer/Vulkan/pipeline/vulkan_pipeline_cache.h
    Function/Renderer/Vulkan/pipeline/vulkan_pipeline_cache.cpp
    Function/Renderer/Vulkan/shaders/vulkan_shader_library.h
    Function/Renderer/Vulkan/shaders/vulkan_shader_library.cpp
    Function/Renderer/Vulkan/vulkan_render_target.h
    Function/Renderer/Vulkan/vulkan_resource_context.h
    Function/Renderer/Vulkan/resources/vulkan_debug_draw_buffer.h
    Function/Renderer/Vulkan/resources/vulkan_debug_draw_buffer.cpp
    Function/Renderer/Vulkan/resources/vulkan_environment_resource.h
    Function/Renderer/Vulkan/resources/vulkan_environment_resource.cpp
    Function/Renderer/Vulkan/resources/vulkan_frame_lighting_resource.h
    Function/Renderer/Vulkan/resources/vulkan_frame_lighting_resource.cpp
    Function/Renderer/Vulkan/resources/vulkan_material_resource.h
    Function/Renderer/Vulkan/resources/vulkan_material_resource.cpp
    Function/Renderer/Vulkan/resources/vulkan_mesh_resource.h
    Function/Renderer/Vulkan/resources/vulkan_mesh_resource.cpp
    Function/Renderer/Vulkan/resources/vulkan_model_resource.h
    Function/Renderer/Vulkan/resources/vulkan_model_resource.cpp
    Function/Renderer/Vulkan/resources/vulkan_texture_resource.h
    Function/Renderer/Vulkan/resources/vulkan_texture_resource.cpp
    Function/Renderer/Vulkan/targets/vulkan_viewport_target.h
    Function/Renderer/Vulkan/targets/vulkan_viewport_target.cpp
    Function/Renderer/Vulkan/targets/vulkan_ao_target.h
    Function/Renderer/Vulkan/targets/vulkan_ao_target.cpp
    Function/Renderer/Vulkan/targets/vulkan_bloom_target.h
    Function/Renderer/Vulkan/targets/vulkan_bloom_target.cpp
    Function/Renderer/Vulkan/targets/vulkan_picking_target.h
    Function/Renderer/Vulkan/targets/vulkan_picking_target.cpp
    Function/Renderer/Vulkan/targets/vulkan_point_shadow_target.h
    Function/Renderer/Vulkan/targets/vulkan_point_shadow_target.cpp
    Function/Renderer/Vulkan/targets/vulkan_scene_color_target.h
    Function/Renderer/Vulkan/targets/vulkan_scene_color_target.cpp
    Function/Renderer/Vulkan/targets/vulkan_ssr_target.h
    Function/Renderer/Vulkan/targets/vulkan_ssr_target.cpp
    Function/Renderer/Vulkan/targets/vulkan_smaa_target.h
    Function/Renderer/Vulkan/targets/vulkan_smaa_target.cpp
    Function/Renderer/Vulkan/targets/vulkan_shadow_map_target.h
    Function/Renderer/Vulkan/targets/vulkan_shadow_map_target.cpp
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
find_path(NEXAUR_MINIAUDIO_INCLUDE_DIR miniaudio.h REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(Stb REQUIRED)
find_path(NEXAUR_TINYGLTF_INCLUDE_DIR tiny_gltf.h REQUIRED)
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
nexaur_select_dependency_target(NEXAUR_NLOHMANN_JSON_TARGET nlohmann_json::nlohmann_json)
nexaur_select_dependency_target(NEXAUR_SPDLOG_TARGET spdlog::spdlog spdlog)

target_include_directories(NexAurEngine PUBLIC
    "${Stb_INCLUDE_DIR}"
)

target_include_directories(NexAurEngine PRIVATE
    "${NEXAUR_MINIAUDIO_INCLUDE_DIR}"
    "${NEXAUR_TINYGLTF_INCLUDE_DIR}"
)

target_link_libraries(NexAurEngine PUBLIC
    ${NEXAUR_ENTT_TARGET}
    ${NEXAUR_GLFW_TARGET}
    ${NEXAUR_GLM_TARGET}
    ${NEXAUR_IMGUI_TARGET}
    ${NEXAUR_IMGUIZMO_TARGET}
    ${NEXAUR_NLOHMANN_JSON_TARGET}
    ${NEXAUR_SPDLOG_TARGET}
)

target_link_libraries(NexAurEngine PRIVATE
    ${NEXAUR_ASSIMP_TARGET}
    Vulkan::Vulkan
    vk-bootstrap::vk-bootstrap
    volk::volk
    GPUOpen::VulkanMemoryAllocator
)

set(NEXAUR_VULKAN_SHADER_SOURCE_DIR "${CMAKE_SOURCE_DIR}/assets/shaders/Renderer/Vulkan")
set(NEXAUR_VULKAN_SHADER_OUTPUT_DIR "$<TARGET_FILE_DIR:NexAurEngine>/shaders/Renderer/Vulkan")

function(nexaur_add_vulkan_hlsl_compile output_var source_file target_profile entry_point output_file)
    list(APPEND ${output_var}
        COMMAND "${DIRECTX_DXC_TOOL}" -spirv -fspv-target-env=vulkan1.3 -HV 2021
                -T ${target_profile}
                -E ${entry_point}
                -Fo "${NEXAUR_VULKAN_SHADER_OUTPUT_DIR}/${output_file}"
                "${NEXAUR_VULKAN_SHADER_SOURCE_DIR}/${source_file}"
    )
    set(${output_var} "${${output_var}}" PARENT_SCOPE)
endfunction()

set(NEXAUR_VULKAN_SHADER_COMPILE_COMMANDS)
nexaur_add_vulkan_hlsl_compile(NEXAUR_VULKAN_SHADER_COMPILE_COMMANDS vulkan_forward.hlsl vs_6_0 VSMain vulkan_forward.vert.spv)
nexaur_add_vulkan_hlsl_compile(NEXAUR_VULKAN_SHADER_COMPILE_COMMANDS vulkan_forward.hlsl ps_6_0 PSMain vulkan_forward.frag.spv)
nexaur_add_vulkan_hlsl_compile(NEXAUR_VULKAN_SHADER_COMPILE_COMMANDS vulkan_object_id.hlsl vs_6_0 VSMain vulkan_object_id.vert.spv)
nexaur_add_vulkan_hlsl_compile(NEXAUR_VULKAN_SHADER_COMPILE_COMMANDS vulkan_object_id.hlsl ps_6_0 PSMain vulkan_object_id.frag.spv)
nexaur_add_vulkan_hlsl_compile(NEXAUR_VULKAN_SHADER_COMPILE_COMMANDS vulkan_skybox.hlsl vs_6_0 VSMain vulkan_skybox.vert.spv)
nexaur_add_vulkan_hlsl_compile(NEXAUR_VULKAN_SHADER_COMPILE_COMMANDS vulkan_skybox.hlsl ps_6_0 PSMain vulkan_skybox.frag.spv)
nexaur_add_vulkan_hlsl_compile(NEXAUR_VULKAN_SHADER_COMPILE_COMMANDS vulkan_shadow_depth.hlsl vs_6_0 VSMain vulkan_shadow_depth.vert.spv)
nexaur_add_vulkan_hlsl_compile(NEXAUR_VULKAN_SHADER_COMPILE_COMMANDS vulkan_debug_draw.hlsl vs_6_0 VSMain vulkan_debug_draw.vert.spv)
nexaur_add_vulkan_hlsl_compile(NEXAUR_VULKAN_SHADER_COMPILE_COMMANDS vulkan_debug_draw.hlsl ps_6_0 PSMain vulkan_debug_draw.frag.spv)
nexaur_add_vulkan_hlsl_compile(NEXAUR_VULKAN_SHADER_COMPILE_COMMANDS vulkan_post_process.hlsl vs_6_0 VSMain vulkan_post_process.vert.spv)
nexaur_add_vulkan_hlsl_compile(NEXAUR_VULKAN_SHADER_COMPILE_COMMANDS vulkan_post_process.hlsl ps_6_0 PSMain vulkan_post_process.frag.spv)
nexaur_add_vulkan_hlsl_compile(NEXAUR_VULKAN_SHADER_COMPILE_COMMANDS ao/vulkan_ssao.hlsl vs_6_0 VSMain vulkan_ssao.vert.spv)
nexaur_add_vulkan_hlsl_compile(NEXAUR_VULKAN_SHADER_COMPILE_COMMANDS ao/vulkan_ssao.hlsl ps_6_0 PSMain vulkan_ssao.frag.spv)
nexaur_add_vulkan_hlsl_compile(NEXAUR_VULKAN_SHADER_COMPILE_COMMANDS ao/vulkan_ao_blur.hlsl vs_6_0 VSMain vulkan_ao_blur.vert.spv)
nexaur_add_vulkan_hlsl_compile(NEXAUR_VULKAN_SHADER_COMPILE_COMMANDS ao/vulkan_ao_blur.hlsl ps_6_0 PSMain vulkan_ao_blur.frag.spv)
nexaur_add_vulkan_hlsl_compile(NEXAUR_VULKAN_SHADER_COMPILE_COMMANDS bloom/vulkan_bloom_downsample.hlsl vs_6_0 VSMain vulkan_bloom_downsample.vert.spv)
nexaur_add_vulkan_hlsl_compile(NEXAUR_VULKAN_SHADER_COMPILE_COMMANDS bloom/vulkan_bloom_downsample.hlsl ps_6_0 PSMain vulkan_bloom_downsample.frag.spv)
nexaur_add_vulkan_hlsl_compile(NEXAUR_VULKAN_SHADER_COMPILE_COMMANDS bloom/vulkan_bloom_upsample.hlsl vs_6_0 VSMain vulkan_bloom_upsample.vert.spv)
nexaur_add_vulkan_hlsl_compile(NEXAUR_VULKAN_SHADER_COMPILE_COMMANDS bloom/vulkan_bloom_upsample.hlsl ps_6_0 PSMain vulkan_bloom_upsample.frag.spv)
nexaur_add_vulkan_hlsl_compile(NEXAUR_VULKAN_SHADER_COMPILE_COMMANDS bloom/vulkan_bloom_composite.hlsl vs_6_0 VSMain vulkan_bloom_composite.vert.spv)
nexaur_add_vulkan_hlsl_compile(NEXAUR_VULKAN_SHADER_COMPILE_COMMANDS bloom/vulkan_bloom_composite.hlsl ps_6_0 PSMain vulkan_bloom_composite.frag.spv)
nexaur_add_vulkan_hlsl_compile(NEXAUR_VULKAN_SHADER_COMPILE_COMMANDS reflection/vulkan_ssr_trace.hlsl vs_6_0 VSMain vulkan_ssr_trace.vert.spv)
nexaur_add_vulkan_hlsl_compile(NEXAUR_VULKAN_SHADER_COMPILE_COMMANDS reflection/vulkan_ssr_trace.hlsl ps_6_0 PSMain vulkan_ssr_trace.frag.spv)
nexaur_add_vulkan_hlsl_compile(NEXAUR_VULKAN_SHADER_COMPILE_COMMANDS post/vulkan_smaa_edge.hlsl vs_6_0 VSMain vulkan_smaa_edge.vert.spv)
nexaur_add_vulkan_hlsl_compile(NEXAUR_VULKAN_SHADER_COMPILE_COMMANDS post/vulkan_smaa_edge.hlsl ps_6_0 PSMain vulkan_smaa_edge.frag.spv)
nexaur_add_vulkan_hlsl_compile(NEXAUR_VULKAN_SHADER_COMPILE_COMMANDS post/vulkan_smaa_blend.hlsl vs_6_0 VSMain vulkan_smaa_blend.vert.spv)
nexaur_add_vulkan_hlsl_compile(NEXAUR_VULKAN_SHADER_COMPILE_COMMANDS post/vulkan_smaa_blend.hlsl ps_6_0 PSMain vulkan_smaa_blend.frag.spv)
nexaur_add_vulkan_hlsl_compile(NEXAUR_VULKAN_SHADER_COMPILE_COMMANDS post/vulkan_smaa_neighborhood.hlsl vs_6_0 VSMain vulkan_smaa_neighborhood.vert.spv)
nexaur_add_vulkan_hlsl_compile(NEXAUR_VULKAN_SHADER_COMPILE_COMMANDS post/vulkan_smaa_neighborhood.hlsl ps_6_0 PSMain vulkan_smaa_neighborhood.frag.spv)

add_custom_command(TARGET NexAurEngine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${NEXAUR_VULKAN_SHADER_OUTPUT_DIR}"
    ${NEXAUR_VULKAN_SHADER_COMPILE_COMMANDS}
    COMMENT "Compiling Vulkan renderer HLSL shaders"
    VERBATIM
)
