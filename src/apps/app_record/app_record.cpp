/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "app_record.h"
#include "assets/record_big.h"
#include "assets/record_small.h"
#include <apps/utils/audio/audio.h>
#include <apps/utils/common.h>
#include <apps/utils/theme.h>
#include <mooncake_log.h>
#include <assets.h>
#include <SD.h>
#include <time.h>

using namespace mooncake;

AppRecord::AppRecord()
{
    setAppInfo().name     = "Record";
    setAppInfo().userData = new AppIcon_t(image_data_record_big, image_data_record_small);
}

AppRecord::~AppRecord()
{
    delete static_cast<AppIcon_t*>(getAppInfo().userData);
}

void AppRecord::onOpen()
{
    mclog::tagInfo(getAppInfo().name, "on open");

    audio::set_keyboard_sfx_enable(false);
    
    _state = STATE_IDLE;
    _audio_buffer.clear();
    _record_samples = 0;
    _record_start_time = 0;
    _last_update_time = 0;
    
    // Disable SD for now to avoid crashes
    _sd_available = false;
    
    render_ui();
}

void AppRecord::onRunning()
{
    auto key_event = GetHAL().keyboard.getLatestKeyEvent();
    
    if (key_event.state) {
        if (_state == STATE_IDLE) {
            if (key_event.keyCode == KEY_ENTER) {
                start_recording();
            }
        } else if (_state == STATE_RECORDING) {
            if (key_event.keyCode == KEY_ENTER) {
                stop_recording();
            }
        } else if (_state == STATE_PLAYING) {
            if (key_event.keyCode == KEY_ENTER) {
                stop_playback();
            }
        }
    }
    
    // Update recording
    if (_state == STATE_RECORDING) {
        if (!GetHAL().mic.isRunning()) {
            _state = STATE_IDLE;
            render_ui();
            return;
        }
        int16_t buffer[BUFFER_SIZE];
        if (GetHAL().mic.record(buffer, BUFFER_SIZE, SAMPLE_RATE)) {
            for (size_t i = 0; i < BUFFER_SIZE && _audio_buffer.size() < MAX_RECORD_SIZE; i++) {
                _audio_buffer.push_back(buffer[i]);
            }
            _record_samples = _audio_buffer.size();
            
            // Update UI every 500ms
            if (GetHAL().millis() - _last_update_time > 500) {
                _last_update_time = GetHAL().millis();
                render_ui();
            }
            
            // Auto-stop at max
            if (_record_samples >= MAX_RECORD_SIZE) {
                stop_recording();
            }
        }
    }
    
    // Close app when home button clicked
    if (GetHAL().homeButton.wasClicked()) {
        close();
    }
}

void AppRecord::onClose()
{
    mclog::tagInfo(getAppInfo().name, "on close");
    
    if (_state == STATE_RECORDING) {
        stop_recording();
    }
    if (_state == STATE_PLAYING) {
        stop_playback();
    }
    
    GetHAL().mic.end();
    GetHAL().speaker.begin();
    GetHAL().speaker.setVolume(255);
    
    _audio_buffer.clear();
    audio::set_keyboard_sfx_enable(true);
}

void AppRecord::start_recording()
{
    mclog::tagInfo(getAppInfo().name, "start recording");
    
    GetHAL().speaker.end();
    
    // Ensure mic is available and configure conservatively
    if (!GetHAL().mic.isEnabled()) {
        mclog::tagError(getAppInfo().name, "mic not enabled; abort recording");
        _state = STATE_IDLE;
        render_ui();
        return;
    }

    auto cfg = GetHAL().mic.config();
    // Conservative mic configuration to improve stability
    cfg.sample_rate = SAMPLE_RATE;
    cfg.over_sampling = 1;
    cfg.magnification = 16;
    cfg.noise_filter_level = 0;
    cfg.dma_buf_len = 64;
    cfg.dma_buf_count = 4;
    cfg.task_priority = 1;
    cfg.task_pinned_core = 0; // pin to core 0
    GetHAL().mic.config(cfg);
    
    if (!GetHAL().mic.begin()) {
        mclog::tagError(getAppInfo().name, "mic begin failed");
        _state = STATE_IDLE;
        render_ui();
        return;
    }
    // small delay to let mic task start cleanly
    GetHAL().delay(10);
    
    _audio_buffer.clear();
    _record_samples = 0;
    _record_start_time = GetHAL().millis();
    _last_update_time = _record_start_time;
    _state = STATE_RECORDING;
    
    render_ui();
}

void AppRecord::stop_recording()
{
    mclog::tagInfo(getAppInfo().name, "stop recording, samples: {}", _record_samples);
    
    if (GetHAL().mic.isRunning()) {
        while (GetHAL().mic.isRecording()) {
            GetHAL().delay(1);
        }
        GetHAL().mic.end();
        GetHAL().delay(5);
    }
    
    _state = STATE_IDLE;
    
    // Prepare for playback
    if (_record_samples > 0) {
        GetHAL().speaker.begin();
        GetHAL().speaker.setVolume(255);
        start_playback();
    } else {
        render_ui();
    }
}

void AppRecord::start_playback()
{
    if (_audio_buffer.empty()) {
        return;
    }
    
    mclog::tagInfo(getAppInfo().name, "start playback");
    
    _state = STATE_PLAYING;
    render_ui();
    
    GetHAL().speaker.playRaw(_audio_buffer.data(), _audio_buffer.size(), SAMPLE_RATE, false);
    
    // Wait for playback to finish
    while (GetHAL().speaker.isPlaying()) {
        GetHAL().delay(10);
        
        // Allow interruption
        auto key = GetHAL().keyboard.getLatestKeyEvent();
        if (key.state && key.keyCode == KEY_ENTER) {
            stop_playback();
            return;
        }
    }
    
    _state = STATE_IDLE;
    render_ui();
}

void AppRecord::stop_playback()
{
    mclog::tagInfo(getAppInfo().name, "stop playback");
    GetHAL().speaker.stop();
    _state = STATE_IDLE;
    render_ui();
}

void AppRecord::save_to_sd()
{
    if (!_sd_available || _audio_buffer.empty()) {
        return;
    }
    
    std::string filename = get_timestamp_filename();
    std::string filepath = "/voice/" + filename;
    
    // Create directory if not exists
    if (!SD.exists("/voice")) {
        SD.mkdir("/voice");
    }
    
    File file = SD.open(filepath.c_str(), FILE_WRITE);
    if (!file) {
        mclog::tagError(getAppInfo().name, "failed to open file: {}", filepath);
        return;
    }
    
    // Write simple header (sample rate + sample count)
    file.write((uint8_t*)&SAMPLE_RATE, sizeof(SAMPLE_RATE));
    size_t count = _audio_buffer.size();
    file.write((uint8_t*)&count, sizeof(count));
    
    // Write audio data
    file.write((uint8_t*)_audio_buffer.data(), _audio_buffer.size() * sizeof(int16_t));
    file.close();
    
    mclog::tagInfo(getAppInfo().name, "saved to: {}", filepath);
}

void AppRecord::load_file_list()
{
    _saved_files.clear();
    
    if (!_sd_available) {
        return;
    }
    
    File dir = SD.open("/voice");
    if (!dir || !dir.isDirectory()) {
        return;
    }
    
    File file = dir.openNextFile();
    while (file) {
        if (!file.isDirectory()) {
            _saved_files.push_back(file.name());
        }
        file = dir.openNextFile();
    }
    
    // Sort by name (timestamp)
    std::sort(_saved_files.begin(), _saved_files.end(), std::greater<std::string>());
    
    mclog::tagInfo(getAppInfo().name, "found {} recordings", _saved_files.size());
}

void AppRecord::play_file(const std::string& filename)
{
    std::string filepath = "/voice/" + filename;
    
    File file = SD.open(filepath.c_str(), FILE_READ);
    if (!file) {
        mclog::tagError(getAppInfo().name, "failed to open file: {}", filepath);
        return;
    }
    
    // Read header
    size_t sample_rate;
    size_t sample_count;
    file.read((uint8_t*)&sample_rate, sizeof(sample_rate));
    file.read((uint8_t*)&sample_count, sizeof(sample_count));
    
    // Read audio data
    _audio_buffer.clear();
    _audio_buffer.resize(sample_count);
    file.read((uint8_t*)_audio_buffer.data(), sample_count * sizeof(int16_t));
    file.close();
    
    mclog::tagInfo(getAppInfo().name, "loaded file: {}, samples: {}", filename, sample_count);
    
    GetHAL().speaker.begin();
    GetHAL().speaker.setVolume(255);
    start_playback();
}

void AppRecord::delete_file(const std::string& filename)
{
    std::string filepath = "/voice/" + filename;
    if (SD.remove(filepath.c_str())) {
        mclog::tagInfo(getAppInfo().name, "deleted: {}", filepath);
    } else {
        mclog::tagError(getAppInfo().name, "failed to delete: {}", filepath);
    }
}

std::string AppRecord::get_timestamp_filename()
{
    time_t now = time(nullptr);
    struct tm* timeinfo = localtime(&now);
    char buffer[32];
    strftime(buffer, sizeof(buffer), "%Y%m%d_%H%M%S.raw", timeinfo);
    return std::string(buffer);
}

void AppRecord::render_ui()
{
    GetHAL().canvas.fillScreen(THEME_COLOR_BG);
    GetHAL().canvas.setFont(FONT_BASIC);
    GetHAL().canvas.setTextSize(1);
    GetHAL().canvas.setCursor(0, 0);
    
    if (_state == STATE_IDLE) {
        GetHAL().canvas.setTextColor(TFT_ORANGE, THEME_COLOR_BG);
        GetHAL().canvas.println("[Voice Recorder]");
        GetHAL().canvas.println();
        
        if (_record_samples > 0) {
            GetHAL().canvas.setTextColor(TFT_WHITE, THEME_COLOR_BG);
            float duration = (float)_record_samples / SAMPLE_RATE;
            GetHAL().canvas.printf("Last: %.1fs\n", duration);
            GetHAL().canvas.println();
        }
        
        GetHAL().canvas.setTextColor(TFT_GREEN, THEME_COLOR_BG);
        GetHAL().canvas.println("ENTER: Record");
        
        if (_sd_available && !_saved_files.empty()) {
            GetHAL().canvas.setTextColor(TFT_CYAN, THEME_COLOR_BG);
            GetHAL().canvas.printf("L: List (%d)\n", _saved_files.size());
        }
        
    } else if (_state == STATE_RECORDING) {
        GetHAL().canvas.setTextColor(TFT_RED, THEME_COLOR_BG);
        GetHAL().canvas.println("[RECORDING]");
        GetHAL().canvas.println();
        
        float duration = (float)_record_samples / SAMPLE_RATE;
        
        GetHAL().canvas.setTextColor(TFT_WHITE, THEME_COLOR_BG);
        GetHAL().canvas.printf("Time: %.1fs\n", duration);
        GetHAL().canvas.printf("Size: %d\n", _record_samples);
        GetHAL().canvas.println();
        
        GetHAL().canvas.setTextColor(TFT_YELLOW, THEME_COLOR_BG);
        GetHAL().canvas.println("ENTER: Stop");
        
    } else if (_state == STATE_PLAYING) {
        GetHAL().canvas.setTextColor(TFT_GREEN, THEME_COLOR_BG);
        GetHAL().canvas.println("[PLAYING]");
        GetHAL().canvas.println();
        
        float duration = (float)_record_samples / SAMPLE_RATE;
        GetHAL().canvas.setTextColor(TFT_WHITE, THEME_COLOR_BG);
        GetHAL().canvas.printf("Duration: %.1fs\n", duration);
        GetHAL().canvas.println();
        
        GetHAL().canvas.setTextColor(TFT_YELLOW, THEME_COLOR_BG);
        GetHAL().canvas.println("ENTER: Stop");
    }
    
    GetHAL().pushCanvas();
}
