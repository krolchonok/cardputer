/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <mooncake.h>
#include <hal/hal.h>
#include <FS.h>
#include <cstdint>
#include <string>
#include <vector>

struct AppIcon_t;

class AppIrFiles : public mooncake::AppAbility {
public:
    AppIrFiles();
    ~AppIrFiles();

    void onOpen() override;
    void onRunning() override;
    void onClose() override;

private:
    struct IrCommand {
        std::string name;
        uint8_t address = 0;
        uint8_t command = 0;
    };

    std::vector<std::string> _files;
    std::vector<IrCommand> _commands;
    int _selected_file    = 0;
    int _selected_command = 0;
    bool _sd_ready        = false;
    std::string _status;

    void render();
    bool refresh_file_list();
    bool load_current_file();
    bool parse_ir_file(fs::File& file, std::vector<IrCommand>& out);

    static std::string trim(const std::string& text);
    static std::string to_lower(std::string text);
    static std::string first_token(const std::string& text);
    static bool parse_hex_byte(const std::string& token, uint8_t& out);
};
