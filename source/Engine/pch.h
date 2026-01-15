#pragma once

#include "Core/Base.h"
#include "Core/Log/log_system.h"

#include <iostream>
#include <memory>
#include <utility>
#include <algorithm>
#include <functional>

#include <string>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <unordered_set>

// ECS
#include "entt/entt.hpp"

// 图形库
#include <glad/glad.h>
#include <GLFW/glfw3.h>

// 数学库
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"

// 输入
#include "Function/Input/KeyCode/key_codes.h"
#include "Function/Input/KeyCode/mouse_codes.h"

// Windows库处理
#ifdef NX_PLATFORM_WINDOWS
    // 必须定义 NOMINMAX，否则 Windows.h 里的 min/max 宏会和 std::min/max 冲突
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <Windows.h>
#endif