find_package(EnTT CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

add_executable(NexAurRendererTests
    renderer_tests.cpp
    ../../Engine/Core/UUID.cpp
    ../../Engine/Core/Log/log_system.cpp
)

target_compile_features(NexAurRendererTests PRIVATE cxx_std_20)
target_link_libraries(NexAurRendererTests PRIVATE
    NexAurRendererFrontend
    NexAurRendererVulkan
    EnTT::EnTT
    glfw
    spdlog::spdlog
)

set_target_properties(NexAurRendererTests PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

function(nexaur_add_renderer_test name argument)
    add_test(NAME "NexAur.Renderer.${name}"
        COMMAND $<TARGET_FILE:NexAurRendererTests> "${argument}")
    set_tests_properties("NexAur.Renderer.${name}" PROPERTIES
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
endfunction()

nexaur_add_renderer_test(FrameContract "--render-frame-contract")
nexaur_add_renderer_test(RenderSettings "--render-settings")
nexaur_add_renderer_test(ShadowFrameBuilder "--shadow-frame-builder")
nexaur_add_renderer_test(RenderGraphStatePlanner "--render-graph-state-planner")
nexaur_add_renderer_test(FrameFeaturePlan "--frame-feature-plan")
nexaur_add_renderer_test(RetirementQueue "--retirement-queue")
nexaur_add_renderer_test(FrameContext "--frame-context")
nexaur_add_renderer_test(AsyncTransferState "--async-transfer-state")
