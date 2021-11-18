#ifndef DBX_SERVICE_HPP
#define DBX_SERVICE_HPP

#include <curl/curl.h>
#include <fstream>
#include <sys/stat.h>

#include "globals.hpp"
#include "json/json.h"
#include "spdlog/spdlog.h"

namespace dbx_service {

    Json::Value dbx_list_files() {
        globals::dbx_logger()->info("Sending request to list files.");

        CURL* curl;
        CURLcode res;
        curl = curl_easy_init();
        if (curl) {
            std::stringstream httpData;
            auto write_response_to_stream = [] (const char* in, size_t size, size_t num, char* out) -> std::size_t {
                std::string data(in, (std::size_t) size * num);
                *((std::stringstream*) out) << data;
                return size * num; 
            };

            struct curl_slist* headers = NULL;
            headers = curl_slist_append(headers, ("Authorization: Bearer " + globals::configuration.dbx_access_token).c_str());
            headers = curl_slist_append(headers, "Content-Type: application/json");

            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            curl_easy_setopt(curl, CURLOPT_URL, "https://api.dropboxapi.com/2/files/list_folder");
            curl_easy_setopt(curl, CURLOPT_POST, 1);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "{\"path\":\"\", \"include_non_downloadable_files\": false}");
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, globals::configuration.dbx_request_timeout);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, *write_response_to_stream);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &httpData);

            res = curl_easy_perform(curl);
            long http_code = 0;
            curl_easy_getinfo (curl, CURLINFO_RESPONSE_CODE, &http_code);
            if (http_code != 200 || res != CURLE_OK) {
                globals::dbx_logger()->error("Request to list files failed: status=\"{0}\" res=\"{1}\"", http_code, curl_easy_strerror(res));
            }
            else {
                Json::Value jsonData;
                Json::CharReaderBuilder jsonReader;
                std::string errs;

                if (Json::parseFromStream(jsonReader, httpData, &jsonData, &errs)) {
                    return jsonData["entries"];
                }
                else {
                    globals::dbx_logger()->error("Json read error:\n{0}", errs);
                    globals::dbx_logger()->error("Could not parse HTTP data as JSON:\n{0}", httpData.str());
                }
            }

            curl_easy_cleanup(curl);
        }
        else {
            globals::dbx_logger()->error("Could not form curl handle.");
        }
        curl_global_cleanup();

        return Json::arrayValue;
    };

    void dbx_delete_file(std::string filename) {
        globals::dbx_logger()->info("Sending request to delete file: {0}", filename);

        CURL* curl;
        CURLcode res;
        curl = curl_easy_init();
        if (curl) {
            struct curl_slist* headers = NULL;
            headers = curl_slist_append(headers, ("Authorization: Bearer " + globals::configuration.dbx_access_token).c_str());
            headers = curl_slist_append(headers, "Content-Type: application/json");

            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            curl_easy_setopt(curl, CURLOPT_URL, "https://api.dropboxapi.com/2/files/delete_v2");
            curl_easy_setopt(curl, CURLOPT_POST, 1);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, ("{\"path\":\"/" + filename + "\"}").c_str());
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, globals::configuration.dbx_request_timeout);

            res = curl_easy_perform(curl);
            long http_code = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
            if (http_code != 200 || res != CURLE_OK) {
                globals::dbx_logger()->error("Request to delete file failed: status=\"{0}\" res=\"{1}\"", http_code, curl_easy_strerror(res));
            }

            curl_easy_cleanup(curl);
        }
        else {
            globals::dbx_logger()->error("Could not form curl handle.");
        }
        curl_global_cleanup();
    };

    void dbx_revert_download(std::string filename) {
        if (remove((globals::STORE_BUFFER_DIR + filename).c_str())) {
            globals::dbx_logger()->error("Could not remove buffer file: {0}", strerror(errno));
        }
        else {
            globals::dbx_logger()->info("Buffer file has been removed.");
        }
    }

    void dbx_download_file(std::string filename) {
        globals::dbx_logger()->info("Sending request to download file: {0}", filename);

        CURL* curl;
        CURLcode res;

        curl = curl_easy_init();
        if (curl) {
            FILE* file;
            file = fopen((globals::STORE_BUFFER_DIR + filename).c_str(), "wb");

            struct curl_slist *headers = NULL;
            headers = curl_slist_append(headers, ("Authorization: Bearer " + globals::configuration.dbx_access_token).c_str());
            headers = curl_slist_append(headers, "Content-Type:");
            headers = curl_slist_append(headers, ("Dropbox-API-Arg: {\"path\":\"/" + filename + "\"}").c_str());

            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            curl_easy_setopt(curl, CURLOPT_URL, "https://content.dropboxapi.com/2/files/download");
            curl_easy_setopt(curl, CURLOPT_POST, 1);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "");
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, globals::configuration.dbx_request_timeout);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, NULL);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);

            res = curl_easy_perform(curl);
            long http_code = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
            if (http_code != 200 || res != CURLE_OK) {
                globals::dbx_logger()->error("Request to download file failed: status=\"{0}\" res=\"{1}\"", http_code, curl_easy_strerror(res));
                dbx_revert_download(filename);
            }
            else if (rename((globals::STORE_BUFFER_DIR + filename).c_str(), (globals::STORE_DIR + filename).c_str())) {
                globals::dbx_logger()->error("Could not move file from buffer: {0}", strerror(errno));
                dbx_revert_download(filename);
            }

            curl_easy_cleanup(curl);
            fclose(file);
        }
        else {
            globals::dbx_logger()->error("Could not form curl handle.");
        }
        curl_global_cleanup();
    };

    void exec_dbx_sync() {
        if (mkdir(globals::STORE_DIR, 0777) == -1 && errno != EEXIST) {
            globals::sys_logger()->critical("Cannot create store directory: {0}", strerror(errno));
            exit(1);
        }
        if (mkdir(globals::STORE_BUFFER_DIR, 0777) == -1 && errno != EEXIST) {
            globals::sys_logger()->critical("Cannot create store buffer directory: {0}", strerror(errno));
            exit(1);
        }

        while (true) {
            globals::dbx_logger()->info("Starting dropbox synchronization.");
            
            Json::Value dbx_entries = dbx_list_files();
            globals::dbx_logger()->info("Dropbox list-files request found {0} remote items.", dbx_entries.size());
            for (Json::Value::ArrayIndex i = 0; i != dbx_entries.size(); i++) {
                if (dbx_entries[i].isMember("name")) {
                    std::string filename = dbx_entries[i]["name"].asString();
                    std::string extension = filename.substr(filename.find_last_of(".") + 1);
                    if (extension == "png" || extension == "jpg" || extension == "jpeg") {
                        std::ifstream infile(globals::STORE_DIR + filename);
                        if (!infile.good()) {
                            dbx_download_file(filename);
                        }
                        infile.close();
                    }
                    else {
                        globals::dbx_logger()->warn("Unsupported file type: {0}", filename);
                        dbx_delete_file(filename);
                    }
                }
            }

            sleep(globals::configuration.dbx_synch_period);
        }
    }

}

#endif 