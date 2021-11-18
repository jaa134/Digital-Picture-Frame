#ifndef GLOBALS_HPP
#define GLOBALS_HPP

#include "spdlog/spdlog.h"

namespace globals {

    const char* const CONF_DIR = "conf/";
    const char* const LOGS_DIR = "logs/";
    const char* const STORE_DIR = "store/";
    const char* const STORE_BUFFER_DIR = "store/buffer/";

    const char* const SYS_LOGGER = "sys_logger";
    const char* const GUI_LOGGER = "gui_logger";
    const char* const DBX_LOGGER = "dbx_logger";

    std::shared_ptr<spdlog::logger> sys_logger() { return spdlog::get(SYS_LOGGER); }
    std::shared_ptr<spdlog::logger> gui_logger() { return spdlog::get(GUI_LOGGER); }
    std::shared_ptr<spdlog::logger> dbx_logger() { return spdlog::get(DBX_LOGGER); }

    struct properties {
        int dbx_synch_period;
        int dbx_request_timeout;
        std::string dbx_access_token;
    } configuration;

}

#endif