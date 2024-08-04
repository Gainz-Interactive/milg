#include "logging.hpp"

namespace milg {
    std::shared_ptr<spdlog::logger> Logging::s_logger = spdlog::stdout_color_mt("MILG");

    void Logging::init() {
        s_logger->set_level(spdlog::level::trace);
        s_logger->set_pattern("%^[%T] [%l] %n%$: %v");
    }

    std::shared_ptr<spdlog::logger> &Logging::logger() {
        return s_logger;
    }
} // namespace milg
