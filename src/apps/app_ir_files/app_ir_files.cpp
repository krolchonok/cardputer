/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "app_ir_files.h"
#include "app_remote/assets/ir_big.h"
#include "app_remote/assets/ir_small.h"
#include <apps/utils/audio/audio.h>
#include <apps/utils/common.h>
#include <apps/utils/theme.h>
#include <assets.h>
#include <mooncake_log.h>
#include <SD.h>
#include <fmt/format.h>
#include <algorithm>
#include <cctype>
#include <cstdlib>

using namespace mooncake;

AppIrFiles::AppIrFiles()
{
    setAppInfo().name     = "IR Files";
    setAppInfo().userData = new AppIcon_t(image_data_ir_big, image_data_ir_small);
}

AppIrFiles::~AppIrFiles()
{
    delete static_cast<AppIcon_t*>(getAppInfo().userData);
}

void AppIrFiles::onOpen()
{
    mclog::tagInfo(getAppInfo().name, "on open");

    GetHAL().irInit();

    GetHAL().canvas.setBaseColor(THEME_COLOR_BG);
    GetHAL().canvas.setFont(FONT_REPL);
    GetHAL().canvas.setTextScroll(false);
    GetHAL().canvas.setTextSize(1);

    _sd_ready = GetHAL().sdCardProbe().is_mounted;
    if (!_sd_ready) {
        _status = "SD card not found";
        render();
        return;
    }

    if (!refresh_file_list()) {
        _status = "Put .ir files in /ir";
        render();
        return;
    }

    load_current_file();
    render();
}

void AppIrFiles::onRunning()
{
    // Handle raw key events for arrow keys (without Fn)
    auto raw = GetHAL().keyboard.getLatestKeyEventRaw();
    if (raw.state) {
        // Left arrow: row=3, col=10
        if (raw.row == 3 && raw.col == 10) {
            if (!_files.empty()) {
                _selected_file = (_selected_file - 1 + static_cast<int>(_files.size())) % static_cast<int>(_files.size());
                load_current_file();
                render();
            }
        }
        // Right arrow: row=3, col=12
        else if (raw.row == 3 && raw.col == 12) {
            if (!_files.empty()) {
                _selected_file = (_selected_file + 1) % static_cast<int>(_files.size());
                load_current_file();
                render();
            }
        }
        // Up arrow: row=2, col=11
        else if (raw.row == 2 && raw.col == 11) {
            if (!_commands.empty() && _selected_command > 0) {
                _selected_command--;
                render();
            }
        }
        // Down arrow: row=3, col=11
        else if (raw.row == 3 && raw.col == 11) {
            if (!_commands.empty() && _selected_command < static_cast<int>(_commands.size()) - 1) {
                _selected_command++;
                render();
            }
        }
        // Enter: row=2, col=13
        else if (raw.row == 2 && raw.col == 13) {
            if (!_commands.empty()) {
                const auto& cmd = _commands[_selected_command];
                GetHAL().irSend(cmd.address, cmd.command);
                audio::play_random_tone();
                _status = fmt::format("Sent {} (0x{:02X},0x{:02X})", cmd.name, cmd.address, cmd.command);
                render();
            }
        }
    }
    
    // Handle regular key events (with Fn+arrows)
    auto key_event = GetHAL().keyboard.getLatestKeyEvent();
    if (key_event.state) {
        switch (key_event.keyCode) {
            case KEY_LEFT:
                if (!_files.empty()) {
                    _selected_file = (_selected_file - 1 + static_cast<int>(_files.size())) % static_cast<int>(_files.size());
                    load_current_file();
                    render();
                }
                break;
            case KEY_RIGHT:
                if (!_files.empty()) {
                    _selected_file = (_selected_file + 1) % static_cast<int>(_files.size());
                    load_current_file();
                    render();
                }
                break;
            case KEY_UP:
                if (!_commands.empty() && _selected_command > 0) {
                    _selected_command--;
                    render();
                }
                break;
            case KEY_DOWN:
                if (!_commands.empty() && _selected_command < static_cast<int>(_commands.size()) - 1) {
                    _selected_command++;
                    render();
                }
                break;
            case KEY_ENTER:
                if (!_commands.empty()) {
                    const auto& cmd = _commands[_selected_command];
                    GetHAL().irSend(cmd.address, cmd.command);
                    audio::play_random_tone();
                    _status = fmt::format("Sent {} (0x{:02X},0x{:02X})", cmd.name, cmd.address, cmd.command);
                    render();
                }
                break;
            default:
                break;
        }
    }

    if (GetHAL().homeButton.wasClicked()) {
        audio::play_random_tone();
        close();
    }
}

void AppIrFiles::onClose()
{
    mclog::tagInfo(getAppInfo().name, "on close");
}

void AppIrFiles::render()
{
    GetHAL().canvas.fillScreen(THEME_COLOR_BG);
    GetHAL().canvas.setFont(FONT_REPL);
    GetHAL().canvas.setTextSize(1);
    GetHAL().canvas.setTextColor(TFT_CYAN, THEME_COLOR_BG);
    GetHAL().canvas.setCursor(4, 4);
    GetHAL().canvas.println("IR Files");

    GetHAL().canvas.setTextColor(TFT_WHITE, THEME_COLOR_BG);
    int y = 20;

    if (!_sd_ready) {
        GetHAL().canvas.setCursor(4, y);
        GetHAL().canvas.setTextColor(TFT_RED, THEME_COLOR_BG);
        GetHAL().canvas.println("SD card not found");
    } else if (_files.empty()) {
        GetHAL().canvas.setCursor(4, y);
        GetHAL().canvas.setTextColor(TFT_YELLOW, THEME_COLOR_BG);
        GetHAL().canvas.println("No .ir files in /ir");
    } else {
        GetHAL().canvas.setCursor(4, y);
        GetHAL().canvas.printf("File %d/%d: %s", _selected_file + 1, static_cast<int>(_files.size()),
                               _files[_selected_file].c_str());
        y += 14;

        if (_commands.empty()) {
            GetHAL().canvas.setCursor(4, y);
            GetHAL().canvas.setTextColor(TFT_YELLOW, THEME_COLOR_BG);
            GetHAL().canvas.println("No commands found");
        } else {
            int start = std::max(0, _selected_command - 2);
            int lines = 6;
            for (int i = 0; i < lines && start + i < static_cast<int>(_commands.size()); ++i) {
                int idx          = start + i;
                const auto& item = _commands[idx];
                bool selected    = idx == _selected_command;

                GetHAL().canvas.setCursor(4, y + i * 14);
                if (selected) {
                    GetHAL().canvas.setTextColor(TFT_BLACK, TFT_CYAN);
                } else {
                    GetHAL().canvas.setTextColor(TFT_WHITE, THEME_COLOR_BG);
                }

                GetHAL().canvas.printf("%s (0x%02X,0x%02X)", item.name.c_str(), item.address, item.command);
            }
        }
    }

    GetHAL().canvas.setTextColor(TFT_DARKGREY, THEME_COLOR_BG);
    GetHAL().canvas.setCursor(4, 114);
    GetHAL().canvas.println("Left/Right: file  Up/Down: code  Enter: send");

    if (!_status.empty()) {
        GetHAL().canvas.setTextColor(TFT_GREEN, THEME_COLOR_BG);
        GetHAL().canvas.setCursor(4, 128);
        GetHAL().canvas.println(_status.c_str());
    }

    GetHAL().pushCanvas();
}

bool AppIrFiles::refresh_file_list()
{
    _files.clear();

    fs::File dir = SD.open("/ir");
    if (!dir || !dir.isDirectory()) {
        mclog::tagWarn(getAppInfo().name, "/ir not found or not a directory");
        return false;
    }

    fs::File file = dir.openNextFile();
    while (file) {
        if (!file.isDirectory()) {
            std::string name = file.name();
            auto slash       = name.find_last_of('/');
            if (slash != std::string::npos) {
                name = name.substr(slash + 1);
            }
            auto lower = to_lower(name);
            if (lower.size() >= 3 && lower.rfind(".ir") == lower.size() - 3) {
                _files.push_back(name);
            }
        }
        file.close();
        file = dir.openNextFile();
    }
    dir.close();

    if (_selected_file >= static_cast<int>(_files.size())) {
        _selected_file = 0;
    }

    return !_files.empty();
}

bool AppIrFiles::load_current_file()
{
    _commands.clear();

    if (_files.empty()) {
        return false;
    }

    std::string path = std::string("/ir/") + _files[_selected_file];
    fs::File file    = SD.open(path.c_str());
    if (!file) {
        mclog::tagError(getAppInfo().name, "failed to open {}", path);
        _status = "Open file failed";
        return false;
    }

    bool parse_ok = parse_ir_file(file, _commands);
    file.close();

    _selected_command = 0;

    if (!parse_ok) {
        _commands.clear();
        _status = "Parse error";
        return false;
    }

    if (_commands.empty()) {
        _status = "No commands";
        return false;
    }

    _status = fmt::format("Loaded {} commands", _commands.size());
    return true;
}

bool AppIrFiles::parse_ir_file(fs::File& file, std::vector<IrCommand>& out)
{
    std::string current_name;
    uint8_t address  = 0;
    uint8_t command  = 0;
    bool has_address = false;
    bool has_command = false;
    bool parse_ok    = true;

    while (file.available()) {
        std::string line = file.readStringUntil('\n').c_str();
        line             = trim(line);

        if (line.empty() || line[0] == '#') {
            continue;
        }

        auto lower = to_lower(line);

        if (lower.rfind("name:", 0) == 0) {
            current_name = trim(line.substr(line.find(':') + 1));
            continue;
        }

        if (lower.rfind("address:", 0) == 0) {
            std::string token = first_token(line.substr(line.find(':') + 1));
            if (!token.empty()) {
                uint8_t value = 0;
                if (parse_hex_byte(token, value)) {
                    address     = value;
                    has_address = true;
                } else {
                    parse_ok = false;
                }
            }
            continue;
        }

        if (lower.rfind("command:", 0) == 0) {
            std::string token = first_token(line.substr(line.find(':') + 1));
            if (!token.empty()) {
                uint8_t value = 0;
                if (parse_hex_byte(token, value)) {
                    command     = value;
                    has_command = true;
                } else {
                    parse_ok = false;
                }
            }
        }

        if (has_address && has_command && !current_name.empty()) {
            out.push_back({current_name, address, command});
            has_address  = false;
            has_command  = false;
            current_name = "";
        }
    }

    return parse_ok;
}

std::string AppIrFiles::trim(const std::string& text)
{
    const char* whitespace = " \t\r\n";
    auto start             = text.find_first_not_of(whitespace);
    if (start == std::string::npos) {
        return "";
    }
    auto end = text.find_last_not_of(whitespace);
    return text.substr(start, end - start + 1);
}

std::string AppIrFiles::to_lower(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

std::string AppIrFiles::first_token(const std::string& text)
{
    auto start = text.find_first_not_of(" \t");
    if (start == std::string::npos) {
        return "";
    }
    auto end = text.find_first_of(" \t", start);
    if (end == std::string::npos) {
        end = text.length();
    }
    return text.substr(start, end - start);
}

bool AppIrFiles::parse_hex_byte(const std::string& token, uint8_t& out)
{
    char* end      = nullptr;
    unsigned long v = std::strtoul(token.c_str(), &end, 16);
    if (end == token.c_str() || *end != '\0' || v > 0xFF) {
        return false;
    }
    out = static_cast<uint8_t>(v);
    return true;
}
