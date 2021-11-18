// https://www.dropbox.com/developers/documentation/http/documentation#files-delete
// https://blogs.dropbox.com/developers/2015/04/a-preview-of-the-new-dropbox-api-v2/
// https://github.com/open-source-parsers/jsoncpp
// https://github.com/gabime/spdlog

#include <csignal>
#include <fstream>
#include <thread>

#include "dbx_service.hpp"
#include "globals.hpp"
#include "json/json.h"
#include "spdlog/async.h"
#include "spdlog/spdlog.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"

using namespace std;

void init_logging();
void init_properties();
void init_cleanup();

int main() {
    init_logging();
    init_properties();
    init_cleanup();

    globals::sys_logger()->info("Starting Dropbox event loop.");
    thread t(dbx_service::exec_dbx_sync);
    t.join();

    return 0;
}

void init_logging() {
    // shared logging settings
    spdlog::init_thread_pool(8192, 1);
    spdlog::set_pattern("[%Y-%m-%d %T.%e] [%n] [tid %t] [%^%l%$] %v");
    auto stdout_sink = make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto gui_rotating_sink = make_shared<spdlog::sinks::rotating_file_sink_mt>(string(globals::LOGS_DIR) + "gui_proc.out", 1024*1024*10, 3);
    auto dbx_rotating_sink = make_shared<spdlog::sinks::rotating_file_sink_mt>(string(globals::LOGS_DIR) + "dbx_proc.out", 1024*1024*10, 3);

    // system
    vector<spdlog::sink_ptr> sys_sinks { stdout_sink, gui_rotating_sink, dbx_rotating_sink };
    auto sys_logger = make_shared<spdlog::async_logger>(
        globals::SYS_LOGGER,
        sys_sinks.begin(),
        sys_sinks.end(),
        spdlog::thread_pool(),
        spdlog::async_overflow_policy::block);
    spdlog::register_logger(sys_logger);

    // gui
    vector<spdlog::sink_ptr> gui_sinks { stdout_sink, gui_rotating_sink };
    auto gui_logger = make_shared<spdlog::async_logger>(
        globals::GUI_LOGGER,
        gui_sinks.begin(),
        gui_sinks.end(),
        spdlog::thread_pool(),
        spdlog::async_overflow_policy::block);
    spdlog::register_logger(gui_logger);

    // dropbox
    vector<spdlog::sink_ptr> dbx_sinks { stdout_sink, dbx_rotating_sink };
    auto dbx_logger = make_shared<spdlog::async_logger>(
        globals::DBX_LOGGER,
        dbx_sinks.begin(),
        dbx_sinks.end(),
        spdlog::thread_pool(),
        spdlog::async_overflow_policy::block);
    spdlog::register_logger(dbx_logger);

    globals::sys_logger()->info("Logging initialized.");
}

void init_properties() {
    globals::sys_logger()->info("Loading properties.");

    ifstream file(string(globals::CONF_DIR) + "properties.json");
    if (file.is_open()) {
        Json::Value jsonData;
        file >> jsonData;
        file.close();

        globals::configuration.dbx_synch_period = jsonData["dbx_synch_period"].asInt();
        globals::sys_logger()->info("Property set: dbx_synch_period={0}", globals::configuration.dbx_synch_period);
        globals::configuration.dbx_request_timeout = jsonData["dbx_request_timeout"].asInt();
        globals::sys_logger()->info("Property set: dbx_request_timeout={0}", globals::configuration.dbx_request_timeout);
        globals::configuration.dbx_access_token = jsonData["dbx_access_token"].asString();
        globals::sys_logger()->info("Property set: dbx_access_token={0}", globals::configuration.dbx_access_token);
    } else {
        globals::sys_logger()->critical("Could not open properties file.");
        exit(1);
    }
}

void init_cleanup() {
    auto on_shutdown = [] (int i) { 
        globals::sys_logger()->critical("Received shutdown signal ({0}).", i);
        globals::sys_logger()->critical("Exiting main process.");
        exit(1); 
    };

    signal(SIGINT, on_shutdown);
    signal(SIGABRT, on_shutdown);
    signal(SIGTERM, on_shutdown);
    signal(SIGTSTP, on_shutdown);
}