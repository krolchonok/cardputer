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
#include <algorithm>
#include <cstring>

using namespace mooncake;

namespace {
    // Layout constants
    constexpr int k_header_height = 18;
    constexpr int k_item_padding = 2;
    constexpr int k_scrollbar_width = 4;
    constexpr int k_content_margin = 2;
    
    // Colors - Modern dark theme
    constexpr uint32_t k_color_bg = 0x1a1a2e;           // Dark blue-black
    constexpr uint32_t k_color_header_bg = 0x16213e;    // Darker blue
    constexpr uint32_t k_color_header_text = 0x00d4ff;  // Cyan
    constexpr uint32_t k_color_selected_bg = 0x0f3460;  // Blue accent
    constexpr uint32_t k_color_selected_border = 0x00d4ff; // Cyan border
    constexpr uint32_t k_color_folder = 0xffd700;       // Gold for folders
    constexpr uint32_t k_color_file = 0xe0e0e0;         // Light gray for files
    constexpr uint32_t k_color_size = 0x808080;         // Gray for size
    constexpr uint32_t k_color_scrollbar_bg = 0x2a2a4a; // Dark scrollbar track
    constexpr uint32_t k_color_scrollbar_thumb = 0x00d4ff; // Cyan thumb
    constexpr uint32_t k_color_separator = 0x3a3a5a;    // Subtle separator
    constexpr uint32_t k_color_error = 0xff4444;        // Red for errors
}

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
    GetHAL().canvas.setBaseColor(k_color_bg);
    GetHAL().canvas.setFont(FONT_REPL);
    GetHAL().canvas.setTextScroll(false);
    GetHAL().canvas.setTextSize(1);
    
    // Check if SD card is mounted
    auto sd_info = GetHAL().sdCardProbe();
    if (!sd_info.is_mounted) {
        auto& canvas = GetHAL().canvas;
        const int w = canvas.width();
        const int h = canvas.height();
        
        canvas.fillScreen(k_color_bg);
        
        // Header
        canvas.fillRect(0, 0, w, k_header_height, k_color_header_bg);
        canvas.setTextColor(k_color_header_text, k_color_header_bg);
        canvas.setCursor(4, 1);
        canvas.print("Explorer");
        canvas.drawFastHLine(0, k_header_height, w, k_color_separator);
        
        // Error message centered
        canvas.setTextColor(k_color_error, k_color_bg);
        canvas.setCursor(w / 2 - 60, h / 2 - 8);
        canvas.print("SD Card Not Found!");
        
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
    const int browser_visible_items = calc_browser_visible_items();
    const int viewer_visible_items = calc_viewer_visible_items();
    
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
                    if (_selected_index >= _scroll_offset + browser_visible_items) {
                        _scroll_offset = _selected_index - browser_visible_items + 1;
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
                int max_scroll = (int)_file_content.size() - viewer_visible_items;
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
                        if (_selected_index >= _scroll_offset + browser_visible_items) {
                            _scroll_offset = _selected_index - browser_visible_items + 1;
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
                    int max_scroll = (int)_file_content.size() - viewer_visible_items;
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
    auto& canvas = GetHAL().canvas;
    const int w = canvas.width();
    const int h = canvas.height();
    
    // Clear with background
    canvas.fillScreen(k_color_bg);
    canvas.setFont(FONT_REPL);
    canvas.setTextSize(1);
    
    // === Header with path ===
    canvas.fillRect(0, 0, w, k_header_height, k_color_header_bg);
    
    // Path text
    canvas.setTextColor(k_color_header_text, k_color_header_bg);
    canvas.setCursor(4, 1);
    
    std::string display_path = _current_path;
    int max_path_chars = (w - 8) / FONT_REPL_WIDTH;
    if ((int)display_path.length() > max_path_chars) {
        display_path = "..." + display_path.substr(display_path.length() - max_path_chars + 3);
    }
    canvas.print(display_path.c_str());
    
    // Separator line under header
    canvas.drawFastHLine(0, k_header_height, w, k_color_separator);
    
    // === File list area ===
    const int list_start_y = k_header_height + 1;
    const int list_height = h - list_start_y;
    const int visible_items = calc_browser_visible_items();
    const int scrollbar_x = w - k_scrollbar_width;
    const int content_width = scrollbar_x - k_content_margin;
    
    // Helper to truncate text
    auto truncate = [](const std::string& text, int max_chars) -> std::string {
        if (max_chars <= 0) return "";
        if ((int)text.size() <= max_chars) return text;
        if (max_chars <= 3) return text.substr(0, max_chars);
        return text.substr(0, max_chars - 3) + "...";
    };
    
    // Draw items
    for (int i = 0; i < visible_items && (i + _scroll_offset) < (int)_file_list.size(); i++) {
        int idx = i + _scroll_offset;
        const FileItem& item = _file_list[idx];
        
        int item_y = list_start_y + i * k_item_height;
        bool is_selected = (idx == _selected_index);
        
        // Item background
        if (is_selected) {
            // Selected: filled rounded rect with border
            canvas.fillRoundRect(k_content_margin, item_y + 1, 
                                 content_width - k_content_margin, k_item_height - 2, 
                                 3, k_color_selected_bg);
            canvas.drawRoundRect(k_content_margin, item_y + 1, 
                                 content_width - k_content_margin, k_item_height - 2, 
                                 3, k_color_selected_border);
        }
        
        // Icon/prefix and name
        int text_x = k_content_margin + 4;
        int text_y = item_y + (k_item_height - FONT_REPL_HEIGHT) / 2;
        
        if (item.is_directory) {
            // Folder icon (unicode folder or bracket notation)
            if (is_selected) {
                canvas.setTextColor(k_color_folder, k_color_selected_bg);
            } else {
                canvas.setTextColor(k_color_folder, k_color_bg);
            }
            canvas.setCursor(text_x, text_y);
            canvas.print("[");
            
            int name_max = (content_width - text_x - 20) / FONT_REPL_WIDTH;
            std::string name = truncate(item.name, name_max);
            canvas.print(name.c_str());
            canvas.print("]");
        } else {
            // File with size on right
            if (is_selected) {
                canvas.setTextColor(k_color_file, k_color_selected_bg);
            } else {
                canvas.setTextColor(k_color_file, k_color_bg);
            }
            
            // Format file size
            char size_buf[12];
            if (item.size < 1024) {
                snprintf(size_buf, sizeof(size_buf), "%dB", (int)item.size);
            } else if (item.size < 1024 * 1024) {
                snprintf(size_buf, sizeof(size_buf), "%.1fK", item.size / 1024.0f);
            } else {
                snprintf(size_buf, sizeof(size_buf), "%.1fM", item.size / (1024.0f * 1024.0f));
            }
            
            int size_width = strlen(size_buf) * FONT_REPL_WIDTH;
            int size_x = content_width - size_width - 4;
            int name_max = (size_x - text_x - 8) / FONT_REPL_WIDTH;
            
            canvas.setCursor(text_x, text_y);
            std::string name = truncate(item.name, name_max);
            canvas.print(name.c_str());
            
            // Size in gray
            if (is_selected) {
                canvas.setTextColor(k_color_size, k_color_selected_bg);
            } else {
                canvas.setTextColor(k_color_size, k_color_bg);
            }
            canvas.setCursor(size_x, text_y);
            canvas.print(size_buf);
        }
    }
    
    // === Scrollbar ===
    if ((int)_file_list.size() > visible_items) {
        int scrollbar_y = list_start_y;
        int scrollbar_h = list_height;
        
        // Track
        canvas.fillRect(scrollbar_x, scrollbar_y, k_scrollbar_width, scrollbar_h, k_color_scrollbar_bg);
        
        // Thumb
        int thumb_h = std::max(8, (visible_items * scrollbar_h) / (int)_file_list.size());
        int max_scroll = (int)_file_list.size() - visible_items;
        int thumb_y = scrollbar_y;
        if (max_scroll > 0) {
            thumb_y = scrollbar_y + (_scroll_offset * (scrollbar_h - thumb_h)) / max_scroll;
        }
        
        canvas.fillRoundRect(scrollbar_x, thumb_y, k_scrollbar_width, thumb_h, 2, k_color_scrollbar_thumb);
    }
    
    // === Empty folder message ===
    if (_file_list.empty()) {
        canvas.setTextColor(k_color_size, k_color_bg);
        canvas.setCursor(w / 2 - 40, h / 2);
        canvas.print("Empty folder");
    }
    
    GetHAL().pushCanvas();
}

void AppFileExplorer::render_viewer()
{
    auto& canvas = GetHAL().canvas;
    const int w = canvas.width();
    const int h = canvas.height();
    
    // Clear with background
    canvas.fillScreen(k_color_bg);
    canvas.setFont(FONT_REPL);
    canvas.setTextSize(1);
    
    // === Header ===
    canvas.fillRect(0, 0, w, k_header_height, k_color_header_bg);
    canvas.setTextColor(k_color_header_text, k_color_header_bg);
    canvas.setCursor(4, 1);
    
    // Show filename in header
    std::string filename = "File Viewer";
    if (_selected_index >= 0 && _selected_index < (int)_file_list.size()) {
        filename = _file_list[_selected_index].name;
        int max_chars = (w - 8) / FONT_REPL_WIDTH;
        if ((int)filename.length() > max_chars) {
            filename = filename.substr(0, max_chars - 3) + "...";
        }
    }
    canvas.print(filename.c_str());
    
    // Separator
    canvas.drawFastHLine(0, k_header_height, w, k_color_separator);
    
    // === Content area ===
    const int content_start_y = k_header_height + 1;
    const int content_height = h - content_start_y;
    const int visible_lines = calc_viewer_visible_items();
    const int scrollbar_x = w - k_scrollbar_width;
    const int content_width = scrollbar_x - k_content_margin;
    const int max_chars = content_width / FONT_REPL_WIDTH;
    
    // Draw lines
    canvas.setTextColor(k_color_file, k_color_bg);
    
    for (int i = 0; i < visible_lines && (i + _viewer_scroll) < (int)_file_content.size(); i++) {
        int line_idx = i + _viewer_scroll;
        int y = content_start_y + i * k_item_height + (k_item_height - FONT_REPL_HEIGHT) / 2;
        
        canvas.setCursor(k_content_margin + 2, y);
        
        std::string line = _file_content[line_idx];
        if ((int)line.length() > max_chars) {
            line = line.substr(0, max_chars - 1);
        }
        canvas.print(line.c_str());
    }
    
    // === Scrollbar ===
    if ((int)_file_content.size() > visible_lines) {
        int scrollbar_h = content_height;
        
        // Track
        canvas.fillRect(scrollbar_x, content_start_y, k_scrollbar_width, scrollbar_h, k_color_scrollbar_bg);
        
        // Thumb
        int thumb_h = std::max(8, (visible_lines * scrollbar_h) / (int)_file_content.size());
        int max_scroll = (int)_file_content.size() - visible_lines;
        int thumb_y = content_start_y;
        if (max_scroll > 0) {
            thumb_y = content_start_y + (_viewer_scroll * (scrollbar_h - thumb_h)) / max_scroll;
        }
        
        canvas.fillRoundRect(scrollbar_x, thumb_y, k_scrollbar_width, thumb_h, 2, k_color_scrollbar_thumb);
    }
    
    // === Empty content message ===
    if (_file_content.empty()) {
        canvas.setTextColor(k_color_size, k_color_bg);
        canvas.setCursor(w / 2 - 32, h / 2);
        canvas.print("No content");
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

int AppFileExplorer::calc_browser_visible_items() const
{
    int available_height = GetHAL().canvas.height() - k_header_height - 1;
    return std::max(1, available_height / k_item_height);
}

int AppFileExplorer::calc_viewer_visible_items() const
{
    int available_height = GetHAL().canvas.height() - k_header_height - 1;
    return std::max(1, available_height / k_item_height);
}
