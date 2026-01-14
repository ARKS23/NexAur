#pragma once

#include "spdlog/spdlog.h"
#include "spdlog/fmt/ostr.h"

#include "Core/Base.h"

namespace NexAur {
    class NEXAUR_API LogSystem {
        public:
            static void init();
        
        public:
            inline static std::shared_ptr<spdlog::logger>& getCoreLogger() { return s_core_logger; }

        private:
            static std::shared_ptr<spdlog::logger> s_core_logger;
    };
} // namespace NexAur

/* ------------------------------- 定义一组宏，简化调用编码 -------------------------------*/
#define NX_CORE_ERROR(...)		::NexAur::LogSystem::getCoreLogger()->error(__VA_ARGS__)
#define NX_CORE_WARN(...)		::NexAur::LogSystem::getCoreLogger()->warn(__VA_ARGS__)
#define NX_CORE_INFO(...)		::NexAur::LogSystem::getCoreLogger()->info(__VA_ARGS__)
#define NX_CORE_TRACE(...)		::NexAur::LogSystem::getCoreLogger()->trace(__VA_ARGS__)
#define NX_CORE_FATAL(...)		::NexAur::LogSystem::getCoreLogger()->critical(__VA_ARGS__)