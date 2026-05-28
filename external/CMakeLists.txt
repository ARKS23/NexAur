# 第三方库总控CMakeLists.txt

# 第三方库只作为 NexAur 的依赖使用，默认关闭测试、样例和安装目标，避免污染 IDE 解决方案。
function(nexaur_set_external_option option_name option_value)
    set(${option_name} ${option_value} CACHE BOOL "" FORCE)
endfunction()

nexaur_set_external_option(BUILD_TESTING OFF)

# GLAD
add_subdirectory(glad)

# GLFW
nexaur_set_external_option(GLFW_BUILD_DOCS OFF)
nexaur_set_external_option(GLFW_BUILD_EXAMPLES OFF)
nexaur_set_external_option(GLFW_BUILD_TESTS OFF)
nexaur_set_external_option(GLFW_INSTALL OFF)
add_subdirectory(glfw)

# GLM
nexaur_set_external_option(GLM_BUILD_LIBRARY OFF)
nexaur_set_external_option(GLM_BUILD_TESTS OFF)
nexaur_set_external_option(GLM_BUILD_INSTALL OFF)
nexaur_set_external_option(GLM_TEST_ENABLE OFF)
nexaur_set_external_option(GLM_PERF_TEST_ENABLE OFF)
add_subdirectory(glm)

# SPDLOG
nexaur_set_external_option(SPDLOG_BUILD_ALL OFF)
nexaur_set_external_option(SPDLOG_BUILD_EXAMPLE OFF)
nexaur_set_external_option(SPDLOG_BUILD_EXAMPLE_HO OFF)
nexaur_set_external_option(SPDLOG_BUILD_TESTS OFF)
nexaur_set_external_option(SPDLOG_BUILD_TESTS_HO OFF)
nexaur_set_external_option(SPDLOG_BUILD_BENCH OFF)
nexaur_set_external_option(SPDLOG_INSTALL OFF)
add_subdirectory(spdlog)

# EnTT
nexaur_set_external_option(ENTT_INSTALL OFF)
nexaur_set_external_option(ENTT_BUILD_TESTING OFF)
nexaur_set_external_option(ENTT_BUILD_TESTBED OFF)
nexaur_set_external_option(ENTT_BUILD_DOCS OFF)
add_subdirectory(entt)

# stb
add_subdirectory(stb)

# assimp
nexaur_set_external_option(ASSIMP_BUILD_ASSIMP_TOOLS OFF)
nexaur_set_external_option(ASSIMP_BUILD_SAMPLES OFF)
nexaur_set_external_option(ASSIMP_BUILD_TESTS OFF)
nexaur_set_external_option(ASSIMP_BUILD_DOCS OFF)
nexaur_set_external_option(ASSIMP_INSTALL OFF)
nexaur_set_external_option(ASSIMP_INSTALL_PDB OFF)
nexaur_set_external_option(ASSIMP_WARNINGS_AS_ERRORS OFF)
nexaur_set_external_option(ASSIMP_NO_EXPORT ON)
nexaur_set_external_option(ASSIMP_IGNORE_GIT_HASH ON)
add_subdirectory(assimp)
if(TARGET UpdateAssimpLibsDebugSymbolsAndDLLs)
    # Assimp 的 MSVC 拷贝辅助目标由 vendored CMake 硬编码生成，保留但排除出默认构建。
    set_target_properties(UpdateAssimpLibsDebugSymbolsAndDLLs PROPERTIES
        EXCLUDE_FROM_ALL TRUE
        EXCLUDE_FROM_DEFAULT_BUILD TRUE
        FOLDER "ThirdParty/assimp")
endif()

# imgui
add_subdirectory(imgui)
