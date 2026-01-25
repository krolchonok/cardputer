/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "app_uart_monitor.h"
#include <apps/utils/audio/audio.h>
#include <apps/utils/common.h>
#include <apps/utils/theme.h>
#include <hal/hal_config.h>
#include <mooncake_log.h>
#include <Arduino.h>
#include <cstdio>
#include <cctype>
#include <cstdlib>
#include <vector>

using namespace mooncake;

static constexpr size_t RX_LINE_MAX = 200;

AppUartMonitor::AppUartMonitor()
{
    setAppInfo().name = "UART";
}

AppUartMonitor::~AppUartMonitor() = default;

void AppUartMonitor::onOpen()
{
    mclog::tagInfo(getAppInfo().name, "on open");

    apply_uart_settings();

    _repl_view               = std::make_unique<ReplView>();
    _repl_view->onRenderTips = [this]() {
        GetHAL().canvas.setTextColor(TFT_ORANGE);
        GetHAL().canvas.println("UART monitor (Cardputer)");
        GetHAL().canvas.printf("Pins: TX=GPIO%d RX=GPIO%d\n", static_cast<int>(HAL_PIN_GPS_TX),
                               static_cast<int>(HAL_PIN_GPS_RX));
        GetHAL().canvas.println("Type :help for commands");
        GetHAL().canvas.setTextColor(TFT_WHITE);
    };
    _repl_view->onCommand = [this](const std::string& command) { handle_command(command); };
    _repl_view->setPromptText("UART> ");
    _repl_view->init();

    show_status();
}

void AppUartMonitor::onRunning()
{
    if (_repl_view) {
        _repl_view->update();
    }

    poll_uart();

    if (GetHAL().homeButton.wasClicked()) {
        audio::play_random_tone();
        close();
    }
}

void AppUartMonitor::onClose()
{
    mclog::tagInfo(getAppInfo().name, "on close");

    if (_uart_started) {
        Serial2.end();
        _uart_started = false;
    }

    if (_repl_view) {
        _repl_view.reset();
    }
}

void AppUartMonitor::handle_command(const std::string& command)
{
    if (command.empty()) {
        return;
    }

    if (command[0] == ':') {
        handle_local_command(command.substr(1));
        return;
    }

    send_line(command);
}

void AppUartMonitor::handle_local_command(const std::string& command)
{
    std::vector<std::string> tokens;
    size_t start = 0;
    while (start < command.size()) {
        while (start < command.size() && std::isspace(static_cast<unsigned char>(command[start]))) {
            start++;
        }
        if (start >= command.size()) {
            break;
        }
        size_t end = start;
        while (end < command.size() && !std::isspace(static_cast<unsigned char>(command[end]))) {
            end++;
        }
        tokens.push_back(command.substr(start, end - start));
        start = end;
    }

    if (tokens.empty()) {
        return;
    }

    const std::string& cmd = tokens[0];
    if (cmd == "help") {
        show_help();
        return;
    }
    if (cmd == "show") {
        show_status();
        return;
    }
    if (cmd == "baud" && tokens.size() >= 2) {
        char* end = nullptr;
        unsigned long baud = std::strtoul(tokens[1].c_str(), &end, 10);
        if (end == tokens[1].c_str() || *end != '\0' || baud < 300 || baud > 2000000) {
            _repl_view->showMessage("Invalid baud rate", TFT_RED);
            _repl_view->refreshPrompt();
            return;
        }
        _baud = static_cast<uint32_t>(baud);
        apply_uart_settings();
        show_status();
        return;
    }
    if (cmd == "parity" && tokens.size() >= 2) {
        if (tokens[1] == "none") {
            _parity = Parity::None;
        } else if (tokens[1] == "even") {
            _parity = Parity::Even;
        } else if (tokens[1] == "odd") {
            _parity = Parity::Odd;
        } else {
            _repl_view->showMessage("Parity: none|even|odd", TFT_RED);
            _repl_view->refreshPrompt();
            return;
        }
        apply_uart_settings();
        show_status();
        return;
    }
    if (cmd == "stop" && tokens.size() >= 2) {
        if (tokens[1] == "1") {
            _stop_bits = 1;
        } else if (tokens[1] == "2") {
            _stop_bits = 2;
        } else {
            _repl_view->showMessage("Stop bits: 1|2", TFT_RED);
            _repl_view->refreshPrompt();
            return;
        }
        apply_uart_settings();
        show_status();
        return;
    }
    if (cmd == "data" && tokens.size() >= 2) {
        int bits = std::atoi(tokens[1].c_str());
        if (bits < 5 || bits > 8) {
            _repl_view->showMessage("Data bits: 5-8", TFT_RED);
            _repl_view->refreshPrompt();
            return;
        }
        _data_bits = static_cast<uint8_t>(bits);
        apply_uart_settings();
        show_status();
        return;
    }
    if (cmd == "clear") {
        _repl_view->clearScreen();
        _repl_view->refreshPrompt();
        return;
    }

    _repl_view->showMessage("Unknown command. Try :help", TFT_RED);
    _repl_view->refreshPrompt();
}

void AppUartMonitor::send_line(const std::string& line)
{
    if (!_uart_started) {
        _repl_view->showMessage("UART not started", TFT_RED);
        _repl_view->refreshPrompt();
        return;
    }

    Serial2.write(reinterpret_cast<const uint8_t*>(line.data()), line.size());
    Serial2.write("\r\n", 2);

    _repl_view->showMessage("TX: " + line, TFT_GREEN);
    _repl_view->refreshPrompt();
}

void AppUartMonitor::poll_uart()
{
    if (!_uart_started) {
        return;
    }

    while (Serial2.available() > 0) {
        char c = static_cast<char>(Serial2.read());

        if (c == '\r') {
            if (!_rx_buffer.empty()) {
                _repl_view->showMessage("RX: " + escape_text(_rx_buffer));
                _repl_view->refreshPrompt();
                _rx_buffer.clear();
            }
            _saw_cr = true;
            continue;
        }

        if (c == '\n') {
            if (_saw_cr) {
                _saw_cr = false;
                continue;
            }
            if (!_rx_buffer.empty()) {
                _repl_view->showMessage("RX: " + escape_text(_rx_buffer));
                _repl_view->refreshPrompt();
                _rx_buffer.clear();
            }
            continue;
        }

        _saw_cr = false;
        _rx_buffer.push_back(c);
        if (_rx_buffer.size() >= RX_LINE_MAX) {
            _repl_view->showMessage("RX: " + escape_text(_rx_buffer));
            _repl_view->refreshPrompt();
            _rx_buffer.clear();
        }
    }
}

void AppUartMonitor::apply_uart_settings()
{
    uint32_t config = make_uart_config();
    if (_uart_started) {
        Serial2.end();
    }
    Serial2.begin(_baud, config, HAL_PIN_GPS_RX, HAL_PIN_GPS_TX);
    _uart_started = true;
}

void AppUartMonitor::show_status()
{
    char parity_char = 'N';
    if (_parity == Parity::Even) {
        parity_char = 'E';
    } else if (_parity == Parity::Odd) {
        parity_char = 'O';
    }
    std::string status = "UART ";
    status += std::to_string(_baud);
    status += " ";
    status += std::to_string(_data_bits);
    status += parity_char;
    status += std::to_string(_stop_bits);
    _repl_view->showMessage(status, TFT_CYAN);
    _repl_view->refreshPrompt();
}

void AppUartMonitor::show_help()
{
    _repl_view->showMessage("Local commands:");
    _repl_view->showMessage(":show");
    _repl_view->showMessage(":baud <rate>");
    _repl_view->showMessage(":parity none|even|odd");
    _repl_view->showMessage(":stop 1|2");
    _repl_view->showMessage(":data 5-8");
    _repl_view->showMessage(":clear");
    _repl_view->showMessage("Other input is sent over UART.");
    _repl_view->refreshPrompt();
}

std::string AppUartMonitor::escape_text(const std::string& text) const
{
    std::string out;
    out.reserve(text.size());
    for (unsigned char c : text) {
        if (c >= 32 && c <= 126) {
            out.push_back(static_cast<char>(c));
        } else if (c == '\t') {
            out += "\\t";
        } else if (c == '\n') {
            out += "\\n";
        } else if (c == '\r') {
            out += "\\r";
        } else {
            char buf[5];
            std::snprintf(buf, sizeof(buf), "\\x%02X", c);
            out += buf;
        }
    }
    return out;
}

uint32_t AppUartMonitor::make_uart_config() const
{
    const bool stop2 = (_stop_bits == 2);
    switch (_parity) {
        case Parity::Even:
            switch (_data_bits) {
                case 5: return stop2 ? SERIAL_5E2 : SERIAL_5E1;
                case 6: return stop2 ? SERIAL_6E2 : SERIAL_6E1;
                case 7: return stop2 ? SERIAL_7E2 : SERIAL_7E1;
                default: return stop2 ? SERIAL_8E2 : SERIAL_8E1;
            }
        case Parity::Odd:
            switch (_data_bits) {
                case 5: return stop2 ? SERIAL_5O2 : SERIAL_5O1;
                case 6: return stop2 ? SERIAL_6O2 : SERIAL_6O1;
                case 7: return stop2 ? SERIAL_7O2 : SERIAL_7O1;
                default: return stop2 ? SERIAL_8O2 : SERIAL_8O1;
            }
        case Parity::None:
        default:
            switch (_data_bits) {
                case 5: return stop2 ? SERIAL_5N2 : SERIAL_5N1;
                case 6: return stop2 ? SERIAL_6N2 : SERIAL_6N1;
                case 7: return stop2 ? SERIAL_7N2 : SERIAL_7N1;
                default: return stop2 ? SERIAL_8N2 : SERIAL_8N1;
            }
    }
}
