/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <mooncake.h>
#include <cstdint>
#include <memory>
#include <WebServer.h>
#include <SD.h>

/**
 * @brief Web Server App - Serve files and manage SD card via HTTP
 *
 */
class AppWebServer : public mooncake::AppAbility {
public:
    AppWebServer();
    ~AppWebServer();

    void onOpen() override;
    void onRunning() override;
    void onClose() override;

private:
    enum State_t {
        STATE_INIT = 0,
        STATE_WAITING_WIFI,
        STATE_RUNNING,
        STATE_ERROR
    };

    State_t _current_state = STATE_INIT;
    std::unique_ptr<WebServer> _server;
    uint32_t _time_count = 0;
    int _key_event_slot_id = -1;
    bool _server_running = false;
    std::string _current_path = "/";  // Current directory path
    File _upload_file;
    bool _upload_in_progress = false;
    bool _upload_failed = false;
    std::string _upload_error;
    std::string _upload_target_path;

    void render_interface();
    void start_server();
    void stop_server();
    void setup_routes();
    void render_main_page();
    void render_file_list();
    void handle_file_upload();
    void handle_file_upload_stream();
    void handle_file_delete();
    void handle_file_download();
    void handle_create_directory();
    void handle_not_found();
};
