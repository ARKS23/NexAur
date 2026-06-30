#pragma once

#include "spdlog/spdlog.h"
#include "spdlog/fmt/ostr.h"

#include "Core/Base.h"

#include <mutex>
#include <string>
#include <vector>

namespace NexAur {
    class RecentLogSink;

    struct NEXAUR_API LogMessage {
        spdlog::level::level_enum level = spdlog::level::info;
        std::string logger_name;
        std::string message;
    };

    class NEXAUR_API LogSystem {
        public:
            static void init();
            static std::vector<LogMessage> getRecentMessages();
            static void clearRecentMessages();
            static const char* levelToString(spdlog::level::level_enum level);
        
        public:
            inline static std::shared_ptr<spdlog::logger>& getCoreLogger() { return s_core_logger; }

        private:
            friend class RecentLogSink;

            static void appendRecentMessage(LogMessage message);

        private:
            static std::shared_ptr<spdlog::logger> s_core_logger;
            static std::vector<LogMessage> s_recent_messages;
            static std::mutex s_recent_messages_mutex;
    };
} // namespace NexAur

/* ------------------------------- 定义一组宏，简化调用编码 -------------------------------*/
#define NX_CORE_ERROR(...)		::NexAur::LogSystem::getCoreLogger()->error(__VA_ARGS__)
#define NX_CORE_WARN(...)		::NexAur::LogSystem::getCoreLogger()->warn(__VA_ARGS__)
#define NX_CORE_INFO(...)		::NexAur::LogSystem::getCoreLogger()->info(__VA_ARGS__)
#define NX_CORE_TRACE(...)		::NexAur::LogSystem::getCoreLogger()->trace(__VA_ARGS__)
#define NX_CORE_FATAL(...)		::NexAur::LogSystem::getCoreLogger()->critical(__VA_ARGS__)
