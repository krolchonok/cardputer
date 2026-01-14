/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <mooncake.h>
#include <cstdint>
#include <hal/hal.h>
#include <vector>
#include <string>

/**
 * @brief Voice recorder with SD card support
 */
class AppRecord : public mooncake::AppAbility {
public:
    AppRecord();
    ~AppRecord();

    void onOpen() override;
    void onRunning() override;
    void onClose() override;

private:
    enum State {
        STATE_IDLE,
        STATE_RECORDING,
        STATE_PLAYING,
        STATE_FILE_LIST
    };

    static constexpr size_t BUFFER_SIZE = 256;
    static constexpr size_t SAMPLE_RATE = 16000;
    static constexpr size_t MAX_RECORD_SIZE = SAMPLE_RATE * 60; // 60 seconds max

    State _state = STATE_IDLE;
    std::vector<int16_t> _audio_buffer;
    size_t _record_samples = 0;
    uint32_t _record_start_time = 0;
    uint32_t _last_update_time = 0;
    std::vector<std::string> _saved_files;
    int _selected_file_index = 0;
    bool _sd_available = false;

    void render_ui();
    void start_recording();
    void stop_recording();
    void start_playback();
    void stop_playback();
    void save_to_sd();
    void load_file_list();
    void play_file(const std::string& filename);
    void delete_file(const std::string& filename);
    std::string get_timestamp_filename();
};
