#pragma once
#include <memory>

#ifdef _WIN32
    // Windows 平台需要处理 dllexport/dllimport
    #ifdef _WIN64
        #define NX_PLATFORM_WINDOWS
    #else
        #error "x86 Builds are not supported!"
    #endif
#elif defined(__linux__)
    #define NX_PLATFORM_LINUX
    // Linux 默认导出符号，通常不需要特殊处理，但保留宏定义是个好习惯
#else
    #error "Unknown platform!"
#endif


// 处理 DLL 导出/导入逻辑
#ifdef NX_PLATFORM_WINDOWS
    #if defined(NEXAUR_DYNAMIC_LINK)
        #ifdef NX_BUILD_DLL
            #define NEXAUR_API __declspec(dllexport)
        #else
            #define NEXAUR_API __declspec(dllimport)
        #endif
    #else // 静态链接模式，不需要任何修饰符
        #define NEXAUR_API
    #endif
#else
    #define NEXAUR_API
#endif

// 位操作宏定义，事件分类中需要用到
#define BIT(x) (1 << x)
// 绑定事件回调宏
#define NX_BIND_EVENT_FN(fn) std::bind(&fn, this, std::placeholders::_1)
