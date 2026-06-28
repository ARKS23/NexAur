project(Sandbox)

file(GLOB_RECURSE sandbox_sources 
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)

# 生成可执行文件
add_executable(Sandbox ${sandbox_sources})

# 链接引擎
target_link_libraries(Sandbox PRIVATE NexAurEngine)

# 设置工作目录
set_target_properties(Sandbox PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

if(BUILD_TESTING)
    function(nexaur_add_sandbox_smoke name argument)
        add_test(NAME "NexAur.${name}" COMMAND $<TARGET_FILE:Sandbox> "${argument}")
        set_tests_properties("NexAur.${name}" PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    endfunction()

    nexaur_add_sandbox_smoke(SceneSerializerSmoke "--scene-serializer-smoke")
    nexaur_add_sandbox_smoke(AudioSmoke "--audio-smoke")
    nexaur_add_sandbox_smoke(InputActionSmoke "--input-action-smoke")
    nexaur_add_sandbox_smoke(RenderSettingsSmoke "--render-settings-smoke")
    nexaur_add_sandbox_smoke(MaterialAssetSmoke "--material-asset-smoke")
    nexaur_add_sandbox_smoke(GameplaySystemsSmoke "--gameplay-systems-smoke")
    nexaur_add_sandbox_smoke(PhysicsTriggerSmoke "--physics-trigger-smoke")
    nexaur_add_sandbox_smoke(RuntimeCameraSmoke "--runtime-camera-smoke")
    nexaur_add_sandbox_smoke(RuntimeGameFlowSmoke "--runtime-game-flow-smoke")
    nexaur_add_sandbox_smoke(PrefabFactorySmoke "--prefab-factory-smoke")
    nexaur_add_sandbox_smoke(SmokeAll "--smoke-all")
endif()
