#pragma once

#include "Core/Base.h"
#include "spdlog/spdlog.h"
#include "spdlog/fmt/ostr.h"

namespace NexAur {
   class NEXAUR_API Log {
    public:
        static void Init();
    
        inline static std::shared_ptr<spdlog::logger>& GetCoreLogger() { return s_CoreLogger; }
        inline static std::shared_ptr<spdlog::logger>& GetClientLogger() { return s_ClientLogger; }

    private:
        static std::shared_ptr<spdlog::logger> s_CoreLogger;
        static std::shared_ptr<spdlog::logger> s_ClientLogger;
   };
}

/* ------------------------------- 定义一组宏，简化调用编码 -------------------------------*/
// Core log macros (引擎内部使用)
#define NA_CORE_ERROR(...)		::NexAur::Log::GetCoreLogger()->error(__VA_ARGS__)  // ...会把__VA_ARGS替换掉
#define NA_CORE_WARN(...)		::NexAur::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define NA_CORE_INFO(...)		::NexAur::Log::GetCoreLogger()->info(__VA_ARGS__)
#define NA_CORE_TRACE(...)		::NexAur::Log::GetCoreLogger()->trace(__VA_ARGS__)
#define NA_CORE_FATAL(...)		::NexAur::Log::GetCoreLogger()->critical(__VA_ARGS__)

// Client log macros (游戏逻辑使用)
#define NA_TRACE(...)         ::NexAur::Log::GetClientLogger()->trace(__VA_ARGS__)
#define NA_INFO(...)          ::NexAur::Log::GetClientLogger()->info(__VA_ARGS__)
#define NA_WARN(...)          ::NexAur::Log::GetClientLogger()->warn(__VA_ARGS__)
#define NA_ERROR(...)         ::NexAur::Log::GetClientLogger()->error(__VA_ARGS__)
#define NA_FATAL(...)         ::NexAur::Log::GetClientLogger()->critical(__VA_ARGS__)