/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "app_file_explorer.h"
#include "assets/folder_big.h"
#include "assets/folder_small.h"
#include <apps/utils/audio/audio.h>
#include <apps/utils/common.h>
#include <apps/utils/theme.h>
#include <mooncake_log.h>
#include <assets.h>
#include <hal.h>
#include <SD.h>
#include <FS.h>

using namespace mooncake;

AppFileExplorer::AppFileExplorer()
{
    setAppInfo().name     = "Explorer";
    setAppInfo().userData = new AppIcon_t(image_data_folder_big, image_data_folder_small);
}

AppFileExplorer::~AppFileExplorer()
{
    // delete static_cast<AppIcon_t*>(getAppInfo().userData);
}

void AppFileExplorer::onOpen()
{
    mclog::tagInfo(getAppInfo().name, "on open");

    // Initialize
    _mode = Mode::BROWSER;
    _current_path = "/";
    _selected_index = 0;
    _scroll_offset = 0;
    _viewer_scroll = 0;
    
    // Setup display
    GetHAL().canvas.setBaseColor(THEME_COLOR_BG);
    GetHAL().canvas.setFont(FONT_REPL);
    GetHAL().canvas.setTextScroll(false);
    GetHAL().canvas.setTextSize(1);
    
    // Check if SD card is mounted
    auto sd_info = GetHAL().sdCardProbe();
    if (!sd_info.is_mounted) {
        GetHAL().canvas.fillScreen(THEME_COLOR_BG);
        GetHAL().canvas.setCursor(10, 40);
        GetHAL().canvas.setTextColor(TFT_RED);
        GetHAL().canvas.println("SD Card Not Found!");
        GetHAL().pushCanvas();
        return;
    }
    
    // List root directory
    list_directory(_current_path);
    render_browser();
}

void AppFileExplorer::onRunning()
{
    bool need_redraw = false;
    bool key_handled = false;
    
    // Handle raw key events for arrow keys (without Fn)
    auto raw = GetHAL().keyboard.getLatestKeyEventRaw();
    if (raw.state) {
        if (_mode == Mode::BROWSER) {
            // Up arrow: row=2, col=11
            if (raw.row == 2 && raw.col == 11) {
                audio::play_random_tone();
                if (_selected_index > 0) {
                    _selected_index--;
                    if (_selected_index < _scroll_offset) {
                        _scroll_offset = _selected_index;
                    }
                    need_redraw = true;
                }
                key_handled = true;
                GetHAL().delay(150);
            }
            // Down arrow: row=3, col=11
            else if (raw.row == 3 && raw.col == 11) {
                audio::play_random_tone();
                if (_selected_index < (int)_file_list.size() - 1) {
                    _selected_index++;
                    if (_selected_index >= _scroll_offset + k_visible_items) {
                        _scroll_offset = _selected_index - k_visible_items + 1;
                    }
                    need_redraw = true;
                }
                key_handled = true;
                GetHAL().delay(150);
            }
            // Left arrow: row=3, col=10 - go up directory
            else if (raw.row == 3 && raw.col == 10) {
                audio::play_random_tone();
                go_back();
                need_redraw = true;
                key_handled = true;
                GetHAL().delay(200);
            }
            // Enter: row=2, col=13
            else if (raw.row == 2 && raw.col == 13) {
                audio::play_random_tone();
                open_selected_item();
                need_redraw = true;
                key_handled = true;
                GetHAL().delay(200);
            }
        } else if (_mode == Mode::VIEWER) {
            // Up arrow: row=2, col=11
            if (raw.row == 2 && raw.col == 11) {
                audio::play_random_tone();
                if (_viewer_scroll > 0) {
                    _viewer_scroll--;
                    need_redraw = true;
                }
                key_handled = true;
                GetHAL().delay(150);
            }
            // Down arrow: row=3, col=11
            else if (raw.row == 3 && raw.col == 11) {
                audio::play_random_tone();
                int max_scroll = (int)_file_content.size() - k_visible_items;
                if (_viewer_scroll < max_scroll && max_scroll > 0) {
                    _viewer_scroll++;
                    need_redraw = true;
                }
                key_handled = true;
                GetHAL().delay(150);
            }
            // Left arrow: row=3, col=10 - exit viewer
            else if (raw.row == 3 && raw.col == 10) {
                audio::play_random_tone();
                _mode = Mode::BROWSER;
                need_redraw = true;
                key_handled = true;
                GetHAL().delay(200);
            }
        }
    }
    
    // Handle regular key events (only if not already handled by raw)
    auto key_event = GetHAL().keyboard.getLatestKeyEvent();
    if (key_event.state && !key_handled) {
        if (_mode == Mode::BROWSER) {
            switch (key_event.keyCode) {
                case KEY_UP:
                    audio::play_random_tone();
                    if (_selected_index > 0) {
                        _selected_index--;
                        if (_selected_index < _scroll_offset) {
                            _scroll_offset = _selected_index;
                        }
                        need_redraw = true;
                    }
                    GetHAL().delay(150);
                    break;
                case KEY_DOWN:
                    audio::play_random_tone();
                    if (_selected_index < (int)_file_list.size() - 1) {
                        _selected_index++;
                        if (_selected_index >= _scroll_offset + k_visible_items) {
                            _scroll_offset = _selected_index - k_visible_items + 1;
                        }
                        need_redraw = true;
                    }
                    GetHAL().delay(150);
                    break;
                case KEY_LEFT:
                case KEY_ESC:
                case KEY_BACKSPACE:
                    audio::play_random_tone();
                    go_back();
                    need_redraw = true;
                    GetHAL().delay(200);
                    break;
                case KEY_ENTER:
                    audio::play_random_tone();
                    open_selected_item();
                    need_redraw = true;
                    GetHAL().delay(200);
                    break;
                default:
                    break;
            }
        } else if (_mode == Mode::VIEWER) {
            switch (key_event.keyCode) {
                case KEY_UP:
                    audio::play_random_tone();
                    if (_viewer_scroll > 0) {
                        _viewer_scroll--;
                        need_redraw = true;
                    }
                    GetHAL().delay(150);
                    break;
                case KEY_DOWN: {
                    audio::play_random_tone();
                    int max_scroll = (int)_file_content.size() - k_visible_items;
                    if (_viewer_scroll < max_scroll && max_scroll > 0) {
                        _viewer_scroll++;
                        need_redraw = true;
                    }
                    GetHAL().delay(150);
                    break;
                }
                case KEY_LEFT:
                case KEY_ESC:
                case KEY_BACKSPACE:
                    audio::play_random_tone();
                    _mode = Mode::BROWSER;
                    need_redraw = true;
                    GetHAL().delay(200);
                    break;
                default:
                    break;
            }
        }
    }
    
    // Close app when home button clicked
    if (GetHAL().homeButton.wasClicked()) {
        audio::play_random_tone();
        close();
    }
    
    if (need_redraw) {
        if (_mode == Mode::BROWSER) {
            render_browser();
        } else {
            render_viewer();
        }
    }
}

void AppFileExplorer::onClose()
{
    mclog::tagInfo(getAppInfo().name, "on close");
}

void AppFileExplorer::list_directory(const std::string& path)
{
    mclog::tagInfo(getAppInfo().name, "listing directory: {}", path.c_str());
    
    _file_list.clear();
    
    // Add parent directory entry if not in root
    if (path != "/") {
        FileItem parent;
        parent.name = "..";
        parent.is_directory = true;
        parent.size = 0;
        _file_list.push_back(parent);
    }
    
    File root = SD.open(path.c_str());
    if (!root) {
        mclog::tagError(getAppInfo().name, "failed to open directory");
        return;
    }
    
    if (!root.isDirectory()) {
        mclog::tagError(getAppInfo().name, "not a directory");
        root.close();
        return;
    }
    
    File file = root.openNextFile();
    while (file) {
        FileItem item;
        item.name = file.name();
        
        // Extract just the filename from full path
        size_t last_slash = item.name.find_last_of('/');
        if (last_slash != std::string::npos) {
            item.name = item.name.substr(last_slash + 1);
        }
        
        item.is_directory = file.isDirectory();
        item.size = file.size();
        
        _file_list.push_back(item);
        file.close();
        file = root.openNextFile();
    }
    root.close();
    
    mclog::tagInfo(getAppInfo().name, "found {} items", _file_list.size());
}

void AppFileExplorer::render_browser()
{
    GetHAL().canvas.fillScreen(THEME_COLOR_BG);
    GetHAL().canvas.setFont(FONT_REPL);
    GetHAL().canvas.setTextSize(1);
    
    // Header: show path
    GetHAL().canvas.setTextColor(TFT_ORANGE, THEME_COLOR_BG);
    GetHAL().canvas.setCursor(2, 0);
    std::string display_path = _current_path;
    if (display_path.length() > 30) {
        display_path = "..." + display_path.substr(display_path.length() - 27);
    }
    GetHAL().canvas.print(display_path.c_str());
    
    // List items
    const int start_y = 12;
    const int line_h = 12;
    
    for (int i = 0; i < k_visible_items && (i + _scroll_offset) < (int)_file_list.size(); i++) {
        int idx = i + _scroll_offset;
        const FileItem& item = _file_list[idx];
        int y = start_y + i * line_h;
        
        // Selected item highlight
        if (idx == _selected_index) {
            GetHAL().canvas.fillRect(0, y, GetHAL().canvas.width() - 4, line_h, TFT_CYAN);
            GetHAL().canvas.setTextColor(TFT_BLACK, TFT_CYAN);
        } else {
            GetHAL().canvas.setTextColor(item.is_directory ? TFT_CYAN : TFT_WHITE, THEME_COLOR_BG);
        }
        
        GetHAL().canvas.setCursor(2, y + 2);
        
        if (item.is_directory) {
            std::string name = "[" + item.name + "]";
            if (name.length() > 28) name = name.substr(0, 25) + "...]";
            GetHAL().canvas.print(name.c_str());
        } else {
            std::string name = item.name;
            if (name.length() > 22) name = name.substr(0, 19) + "...";
            GetHAL().canvas.print(name.c_str());
            
            // File size on right
            char size_buf[16];
            if (item.size < 1024) {
                snprintf(size_buf, sizeof(size_buf), " %dB", (int)item.size);
            } else if (item.size < 1024 * 1024) {
                snprintf(size_buf, sizeof(size_buf), " %.1fK", item.size / 1024.0f);
            } else {
                snprintf(size_buf, sizeof(size_buf), " %.1fM", item.size / (1024.0f * 1024.0f));
            }
            if (idx == _selected_index) {
                GetHAL().canvas.setTextColor(TFT_DARKGREY, TFT_CYAN);
            } else {
                GetHAL().canvas.setTextColor(TFT_DARKGREY, THEME_COLOR_BG);
            }
            GetHAL().canvas.print(size_buf);
        }
    }
    
    // Scrollbar
    if ((int)_file_list.size() > k_visible_items) {
        int bar_x = GetHAL().canvas.width() - 2;
        int bar_h = k_visible_items * line_h;
        int thumb_h = std::max(4, (k_visible_items * bar_h) / (int)_file_list.size());
        int thumb_y = start_y + (_scroll_offset * bar_h) / (int)_file_list.size();
        
        GetHAL().canvas.drawFastVLine(bar_x, start_y, bar_h, TFT_DARKGREY);
        GetHAL().canvas.fillRect(bar_x - 1, thumb_y, 3, thumb_h, TFT_WHITE);
    }
    
    GetHAL().pushCanvas();
}

void AppFileExplorer::render_viewer()
{
    GetHAL().canvas.fillScreen(THEME_COLOR_BG);
    
    // Draw title
    GetHAL().canvas.setTextColor(TFT_ORANGE);
    GetHAL().canvas.setCursor(2, 2);
    GetHAL().canvas.print("Viewing File");
    
    // Draw separator
    GetHAL().canvas.drawLine(0, 11, GetHAL().canvas.width(), 11, TFT_DARKGREY);
    
    // Draw file content
    int y = 14;
    GetHAL().canvas.setTextColor(TFT_WHITE);
    
    for (int i = 0; i < k_visible_items && (i + _viewer_scroll) < (int)_file_content.size(); i++) {
        int line_index = i + _viewer_scroll;
        GetHAL().canvas.setCursor(2, y + 2);
        
        // Truncate long lines
        std::string line = _file_content[line_index];
        if (line.length() > 32) {
            line = line.substr(0, 29) + "...";
        }
        GetHAL().canvas.print(line.c_str());
        
        y += k_item_height;
    }
    
    // Draw scrollbar if needed
    if ((int)_file_content.size() > k_visible_items) {
        int scrollbar_height = k_visible_items * k_item_height;
        int thumb_height = (k_visible_items * scrollbar_height) / _file_content.size();
        int thumb_pos = (_viewer_scroll * scrollbar_height) / _file_content.size();
        
        GetHAL().canvas.drawLine(GetHAL().canvas.width() - 2, 14, 
                                 GetHAL().canvas.width() - 2, 14 + scrollbar_height, 
                                 TFT_DARKGREY);
        GetHAL().canvas.fillRect(GetHAL().canvas.width() - 3, 14 + thumb_pos, 
                                3, thumb_height, TFT_CYAN);
    }
    
    GetHAL().pushCanvas();
}

void AppFileExplorer::open_selected_item()
{
    if (_selected_index < 0 || _selected_index >= (int)_file_list.size()) {
        return;
    }
    
    const FileItem& item = _file_list[_selected_index];
    
    if (item.is_directory) {
        // Navigate to directory
        if (item.name == "..") {
            go_back();
        } else {
            // Build new path
            if (_current_path == "/") {
                _current_path = "/" + item.name;
            } else {
                _current_path = _current_path + "/" + item.name;
            }
            
            _selected_index = 0;
            _scroll_offset = 0;
            list_directory(_current_path);
        }
    } else {
        // Open file for viewing
        std::string filepath;
        if (_current_path == "/") {
            filepath = "/" + item.name;
        } else {
            filepath = _current_path + "/" + item.name;
        }
        
        read_file_content(filepath);
        _mode = Mode::VIEWER;
        _viewer_scroll = 0;
    }
}

void AppFileExplorer::read_file_content(const std::string& filepath)
{
    mclog::tagInfo(getAppInfo().name, "reading file: {}", filepath.c_str());
    
    _file_content.clear();
    
    File file = SD.open(filepath.c_str());
    if (!file) {
        _file_content.push_back("Error: Cannot open file");
        return;
    }
    
    // Check file size - don't load huge files
    if (file.size() > 10240) {  // 10KB limit
        _file_content.push_back("Error: File too large");
        _file_content.push_back("(max 10KB)");
        file.close();
        return;
    }
    
    // Read file line by line
    std::string line;
    while (file.available()) {
        char c = file.read();
        if (c == '\n') {
            _file_content.push_back(line);
            line.clear();
            
            // Limit number of lines
            if (_file_content.size() > 500) {
                _file_content.push_back("...[truncated]");
                break;
            }
        } else if (c != '\r') {
            line += c;
        }
    }
    
    // Add last line if not empty
    if (!line.empty()) {
        _file_content.push_back(line);
    }
    
    file.close();
    
    if (_file_content.empty()) {
        _file_content.push_back("(empty file)");
    }
    
    mclog::tagInfo(getAppInfo().name, "loaded {} lines", _file_content.size());
}

void AppFileExplorer::go_back()
{
    if (_current_path == "/") {
        return;  // Already at root
    }
    
    // Go up one directory level
    size_t last_slash = _current_path.find_last_of('/');
    if (last_slash == 0) {
        _current_path = "/";
    } else if (last_slash != std::string::npos) {
        _current_path = _current_path.substr(0, last_slash);
    }
    
    _selected_index = 0;
    _scroll_offset = 0;
    list_directory(_current_path);
}
