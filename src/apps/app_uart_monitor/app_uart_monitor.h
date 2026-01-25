/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <apps/utils/repl_view.h>
#include <mooncake.h>
#include <cstdint>
#include <memory>
#include <string>

class AppUartMonitor : public mooncake::AppAbility {
public:
    AppUartMonitor();
    ~AppUartMonitor();

    void onOpen() override;
    void onRunning() override;
    void onClose() override;

private:
    enum class Parity {
        None,
        Even,
        Odd,
    };

    std::unique_ptr<ReplView> _repl_view;
    uint32_t _baud     = 115200;
    uint8_t _data_bits = 8;
    uint8_t _stop_bits = 1;
    Parity _parity     = Parity::None;
    bool _uart_started = false;
    bool _saw_cr       = false;
    std::string _rx_buffer;

    void handle_command(const std::string& command);
    void handle_local_command(const std::string& command);
    void send_line(const std::string& line);
    void poll_uart();
    void apply_uart_settings();
    void show_status();
    void show_help();
    std::string escape_text(const std::string& text) const;
    uint32_t make_uart_config() const;
};
