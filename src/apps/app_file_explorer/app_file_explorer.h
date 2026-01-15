/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <mooncake.h>
#include <cstdint>
#include <string>
#include <vector>

/**
 * @brief File Explorer App - Browse and view files on SD card
 *
 */
class AppFileExplorer : public mooncake::AppAbility {
public:
    AppFileExplorer();
    ~AppFileExplorer();

    void onOpen() override;
    void onRunning() override;
    void onClose() override;

private:
    enum class Mode {
        BROWSER,   // Browsing files and folders
        VIEWER     // Viewing file content
    };

    struct FileItem {
        std::string name;
        bool is_directory;
        size_t size;
    };

    Mode _mode;
    std::string _current_path;
    std::vector<FileItem> _file_list;
    int _selected_index;
    int _scroll_offset;
    
    // For file viewer
    std::vector<std::string> _file_content;
    int _viewer_scroll;
    
    static constexpr int k_item_height = 16;
    
    void list_directory(const std::string& path);
    void render_browser();
    void render_viewer();
    void open_selected_item();
    void read_file_content(const std::string& filepath);
    void go_back();
    int calc_browser_visible_items() const;
    int calc_viewer_visible_items() const;
};
