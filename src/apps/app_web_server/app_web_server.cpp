/*
 * SPDX-FileCopyrightText: 2025 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "app_web_server.h"
#include <apps/utils/audio/audio.h>
#include <apps/utils/common.h>
#include <apps/utils/theme.h>
#include <mooncake_log.h>
#include <assets.h>
#include <hal.h>
#include <hal/keyboard/keymap.h>
#include <SD.h>
#include <WiFi.h>

using namespace mooncake;

AppWebServer::AppWebServer()
{
    setAppInfo().name = "WebServer";
}

AppWebServer::~AppWebServer()
{
    if (_server) {
        stop_server();
    }
}

void AppWebServer::onOpen()
{
    mclog::tagInfo(getAppInfo().name, "on open");

    // Check SD card status
    auto sd_info = GetHAL().sdCardProbe();
    mclog::tagInfo(getAppInfo().name, "SD Card mounted: %d", sd_info.is_mounted);

    _current_state = STATE_INIT;
    _server_running = false;

    // Setup keyboard event handler
    _key_event_slot_id = GetHAL().keyboard.onKeyEvent.connect(
        [this](const Keyboard::KeyEvent_t& keyEvent) {
            if (keyEvent.state) {  // Key pressed
                if (keyEvent.keyCode == KEY_ENTER) {
                    if (_current_state == STATE_ERROR) {
                        // Try to restart server on error
                        _current_state = STATE_INIT;
                    }
                }
            }
        });

    render_interface();
}

void AppWebServer::onRunning()
{
    _time_count++;

    // Process state machine
    switch (_current_state) {
        case STATE_INIT:
            if (GetHAL().isWifiConnected()) {
                _current_state = STATE_RUNNING;
                start_server();
            } else {
                _current_state = STATE_WAITING_WIFI;
            }
            render_interface();
            break;

        case STATE_WAITING_WIFI:
            if (GetHAL().isWifiConnected()) {
                _current_state = STATE_RUNNING;
                start_server();
                render_interface();
            }
            break;

        case STATE_RUNNING:
            if (_server) {
                _server->handleClient();
            }
            // Update display every 500ms
            if (_time_count % 50 == 0) {
                render_interface();
            }
            break;

        case STATE_ERROR:
            break;
    }

    // Close app when home button clicked
    if (GetHAL().homeButton.wasClicked()) {
        audio::play_random_tone();
        close();
    }
}

void AppWebServer::onClose()
{
    mclog::tagInfo(getAppInfo().name, "on close");

    stop_server();

    // Disconnect keyboard event handler
    if (_key_event_slot_id >= 0) {
        GetHAL().keyboard.onKeyEvent.disconnect(_key_event_slot_id);
    }
}

void AppWebServer::render_interface()
{
    GetHAL().canvas.fillScreen(THEME_COLOR_BG);
    GetHAL().canvas.setTextScroll(true);
    GetHAL().canvas.setBaseColor(THEME_COLOR_BG);
    GetHAL().canvas.setTextColor(TFT_WHITE, THEME_COLOR_BG);
    GetHAL().canvas.setTextSize(1);
    GetHAL().canvas.setCursor(0, 0);

    GetHAL().canvas.setTextColor(TFT_YELLOW, THEME_COLOR_BG);
    GetHAL().canvas.println("WebServer");
    GetHAL().canvas.setTextColor(TFT_WHITE, THEME_COLOR_BG);

    switch (_current_state) {
        case STATE_INIT:
            GetHAL().canvas.println("Initializing...");
            break;

        case STATE_WAITING_WIFI:
            GetHAL().canvas.setTextColor(TFT_ORANGE, THEME_COLOR_BG);
            GetHAL().canvas.println("WiFi not connected");
            GetHAL().canvas.setTextColor(TFT_WHITE, THEME_COLOR_BG);
            GetHAL().canvas.println("Please connect to WiFi");
            break;

        case STATE_RUNNING:
            GetHAL().canvas.setTextColor(TFT_GREEN, THEME_COLOR_BG);
            GetHAL().canvas.println("Server Running");
            GetHAL().canvas.setTextColor(TFT_WHITE, THEME_COLOR_BG);
            GetHAL().canvas.printf("IP: %s\n", GetHAL().getWifiIpAddress().c_str());
            GetHAL().canvas.printf("Port: 80\n");
            GetHAL().canvas.println("");
            GetHAL().canvas.println("Access via browser:");
            GetHAL().canvas.printf("http://%s\n", GetHAL().getWifiIpAddress().c_str());
            break;

        case STATE_ERROR:
            GetHAL().canvas.setTextColor(TFT_RED, THEME_COLOR_BG);
            GetHAL().canvas.println("Error starting server");
            GetHAL().canvas.setTextColor(TFT_WHITE, THEME_COLOR_BG);
            GetHAL().canvas.println("Press Enter to retry");
            break;
    }

    GetHAL().canvas.setTextColor(TFT_CYAN, THEME_COLOR_BG);
    GetHAL().canvas.println("");
    GetHAL().canvas.println("Home: Exit");
    GetHAL().pushCanvas();
}

void AppWebServer::start_server()
{
    mclog::tagInfo(getAppInfo().name, "starting web server");

    // Check SD card again
    auto sd_info = GetHAL().sdCardProbe();
    if (!sd_info.is_mounted) {
        mclog::tagWarn(getAppInfo().name, "SD Card not mounted!");
    } else {
        mclog::tagInfo(getAppInfo().name, "SD Card info - Type: %s, Size: %s, Name: %s", 
                       sd_info.type.c_str(), sd_info.size.c_str(), sd_info.name.c_str());
    }

    if (_server) {
        _server->stop();
    }

    _server = std::make_unique<WebServer>(80);
    
    setup_routes();
    
    _server->begin();
    _server_running = true;

    mclog::tagInfo(getAppInfo().name, "web server started on port 80");
    mclog::tagInfo(getAppInfo().name, "access at http://%s", GetHAL().getWifiIpAddress().c_str());
}

void AppWebServer::stop_server()
{
    if (_server && _server_running) {
        mclog::tagInfo(getAppInfo().name, "stopping web server");
        _server->stop();
        _server_running = false;
    }
}

void AppWebServer::setup_routes()
{
    if (!_server) {
        return;
    }

    // Main page
    _server->on("/", HTTP_GET, [this]() {
        render_main_page();
    });

    // File list API
    _server->on("/api/files", HTTP_GET, [this]() {
        render_file_list();
    });

    // File upload
    _server->on("/api/upload", HTTP_POST, [this]() {
        handle_file_upload();
    }, [this]() {
        handle_file_upload_stream();
    });

    // File delete
    _server->on("/api/delete", HTTP_POST, [this]() {
        handle_file_delete();
    });

    // Create directory
    _server->on("/api/mkdir", HTTP_POST, [this]() {
        handle_create_directory();
    });

    // File download
    _server->on("/download", HTTP_GET, [this]() {
        handle_file_download();
    });

    // Not found handler
    _server->onNotFound([this]() {
        handle_not_found();
    });
}

void AppWebServer::render_main_page()
{
    if (!_server) {
        return;
    }

    String html = R"rawhtml(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>CardPuter Web Server</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body { font-family: Arial, sans-serif; background: #1a1a1a; color: #fff; padding: 20px; }
        .container { max-width: 900px; margin: 0 auto; }
        h1 { color: #00ff00; margin-bottom: 20px; }
        .section { background: #2a2a2a; padding: 15px; margin-bottom: 15px; border-radius: 5px; }
        .breadcrumb { background: #1a1a1a; border: 1px solid #444; padding: 10px; margin-bottom: 10px; border-radius: 3px; }
        .breadcrumb a { color: #00ff00; cursor: pointer; text-decoration: none; margin: 0 5px; }
        .breadcrumb a:hover { color: #00cc00; }
        .file-list { background: #1a1a1a; border: 1px solid #444; padding: 10px; max-height: 400px; overflow-y: auto; }
        .file-item { padding: 8px; background: #333; margin: 5px 0; border-radius: 3px; display: flex; justify-content: space-between; align-items: center; }
        .file-item.folder { background: #334455; }
        .file-item.folder .name { color: #00ccff; cursor: pointer; flex: 1; }
        .file-item.folder .name:hover { color: #00ffff; text-decoration: underline; }
        .file-item .name { flex: 1; }
        .actions { display: flex; gap: 5px; }
        button { background: #00ff00; color: #000; border: none; padding: 10px 15px; border-radius: 3px; cursor: pointer; margin: 5px 5px 5px 0; }
        button:hover { background: #00cc00; }
        input[type="file"] { margin: 10px 0; }
        .upload-input { padding: 10px; background: #333; border: 1px solid #555; color: #fff; border-radius: 3px; }
        .delete-btn { background: #ff4444; color: #fff; padding: 5px 10px; font-size: 12px; }
        .delete-btn:hover { background: #cc0000; }
        .status { padding: 10px; margin-bottom: 10px; border-radius: 3px; }
        .status.success { background: #004400; color: #00ff00; }
        .status.error { background: #440000; color: #ff6666; }
    </style>
</head>
<body>
    <div class="container">
        <h1>CardPuter Web Server</h1>
        
        <div class="section">
            <h2>Device Info</h2>
            <p>IP Address: <strong id="ip"></strong></p>
            <p>MAC: <strong id="mac"></strong></p>
        </div>

        <div class="section">
            <h2>Upload File</h2>
            <input type="file" id="fileInput" class="upload-input">
            <br>
            <button onclick="uploadFile()">Upload to Current Folder</button>
            <div id="uploadStatus"></div>
        </div>

        <div class="section">
            <h2>Create New Folder</h2>
            <input type="text" id="folderInput" class="upload-input" placeholder="Enter folder name">
            <br>
            <button onclick="createFolder()">Create Folder</button>
            <div id="createStatus"></div>
        </div>

        <div class="section">
            <h2>SD Card Files</h2>
            <div class="breadcrumb" id="breadcrumb">
                <a onclick="navigate('/')">Root</a>
            </div>
            <div class="file-list" id="fileList">
                <div style="color: #999;">Loading files...</div>
            </div>
        </div>
    </div>

    <script>
        let currentPath = '/';

        function getDeviceInfo() {
            document.getElementById('ip').textContent = location.hostname;
            document.getElementById('mac').textContent = 'N/A';
        }

        function updateBreadcrumb() {
            let parts = currentPath.split('/').filter(p => p);
            let html = '<a onclick="navigate(\'/\')">Root</a>';
            let path = '';
            for (let part of parts) {
                path += '/' + part;
                html += ' / <a onclick="navigate(\'' + path + '\')">' + part + '</a>';
            }
            document.getElementById('breadcrumb').innerHTML = html;
        }

        function navigate(path) {
            currentPath = path;
            updateBreadcrumb();
            loadFiles();
        }

        function loadFiles() {
            let url = '/api/files?dir=' + encodeURIComponent(currentPath);
            fetch(url)
                .then(r => r.json())
                .then(data => {
                    let html = '';
                    if (data.files && data.files.length > 0) {
                        data.files.forEach(file => {
                            if (file.is_dir) {
                                html += '<div class="file-item folder">';
                                html += '<span class="name" onclick="navigate(\'' + file.path.replace(/'/g, "\\'") + '\')">[FOLDER] ' + file.name + '</span>';
                                html += '</div>';
                            } else {
                                let size = file.size > 1024 ? (file.size / 1024).toFixed(2) + ' KB' : file.size + ' B';
                                html += '<div class="file-item">';
                                html += '<span class="name">' + file.name + ' (' + size + ')</span>';
                                html += '<div class="actions">';
                                html += '<button onclick="downloadFile(\'' + file.path.replace(/'/g, "\\'") + '\')" style="padding: 3px 8px; font-size: 11px;">Download</button>';
                                html += '<button class="delete-btn" onclick="deleteFile(\'' + file.path.replace(/'/g, "\\'") + '\')">Delete</button>';
                                html += '</div>';
                                html += '</div>';
                            }
                        });
                    } else {
                        html = '<div style="color: #999;">No files found</div>';
                    }
                    document.getElementById('fileList').innerHTML = html;
                })
                .catch(e => {
                    console.error(e);
                    document.getElementById('fileList').innerHTML = '<div style="color: #ff6666;">Error loading files</div>';
                });
        }

        function uploadFile() {
            let input = document.getElementById('fileInput');
            if (!input.files.length) {
                showStatus('Please select a file', 'error');
                return;
            }

            let formData = new FormData();
            formData.append('file', input.files[0]);
            formData.append('dir', currentPath);

            let statusDiv = document.getElementById('uploadStatus');
            statusDiv.innerHTML = '<div class="status">Uploading...</div>';

            fetch('/api/upload', {
                method: 'POST',
                body: formData
            })
                .then(r => r.json())
                .then(data => {
                    if (data.success) {
                        showStatus('File uploaded successfully', 'success');
                        input.value = '';
                        loadFiles();
                    } else {
                        showStatus('Upload failed: ' + data.error, 'error');
                    }
                })
                .catch(e => showStatus('Error: ' + e, 'error'));
        }

        function downloadFile(filepath) {
            window.location.href = '/download?file=' + encodeURIComponent(filepath);
        }

        function deleteFile(filepath) {
            if (confirm('Delete ' + filepath + '?')) {
                fetch('/api/delete', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
                    body: 'file=' + encodeURIComponent(filepath)
                })
                    .then(r => r.json())
                    .then(data => {
                        if (data.success) {
                            showStatus('File deleted', 'success');
                            loadFiles();
                        } else {
                            showStatus('Delete failed: ' + data.error, 'error');
                        }
                    })
                    .catch(e => showStatus('Error: ' + e, 'error'));
            }
        }

        function createFolder() {
            let input = document.getElementById('folderInput');
            let folderName = input.value.trim();
            
            if (!folderName) {
                showCreateStatus('Please enter a folder name', 'error');
                return;
            }
            
            if (folderName.indexOf('/') !== -1 || folderName.indexOf('..') !== -1) {
                showCreateStatus('Invalid folder name', 'error');
                return;
            }

            let statusDiv = document.getElementById('createStatus');
            statusDiv.innerHTML = '<div class="status">Creating folder...</div>';

            fetch('/api/mkdir', {
                method: 'POST',
                headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
                body: 'name=' + encodeURIComponent(folderName) + '&dir=' + encodeURIComponent(currentPath)
            })
                .then(r => r.json())
                .then(data => {
                    if (data.success) {
                        showCreateStatus('Folder created successfully', 'success');
                        input.value = '';
                        loadFiles();
                    } else {
                        showCreateStatus('Failed to create folder: ' + data.error, 'error');
                    }
                })
                .catch(e => showCreateStatus('Error: ' + e, 'error'));
        }

        function showStatus(msg, type) {
            let statusDiv = document.getElementById('uploadStatus');
            statusDiv.innerHTML = '<div class="status ' + type + '">' + msg + '</div>';
            if (type === 'success') {
                setTimeout(function() { statusDiv.innerHTML = ''; }, 3000);
            }
        }

        function showCreateStatus(msg, type) {
            let statusDiv = document.getElementById('createStatus');
            statusDiv.innerHTML = '<div class="status ' + type + '">' + msg + '</div>';
            if (type === 'success') {
                setTimeout(function() { statusDiv.innerHTML = ''; }, 3000);
            }
        }

        getDeviceInfo();
        updateBreadcrumb();
        loadFiles();
        setInterval(loadFiles, 5000);
    </script>
</body>
</html>
)rawhtml";

    _server->send(200, "text/html", html);
}

void AppWebServer::render_file_list()
{
    if (!_server) {
        return;
    }

    // Get directory path from query parameter
    if (_server->hasArg("dir")) {
        _current_path = _server->arg("dir").c_str();
    }
    
    // Sanitize path
    if (_current_path.empty()) {
        _current_path = "/";
    }
    if (!_current_path.empty() && _current_path[0] != '/') {
        _current_path = "/" + _current_path;
    }
    if (_current_path.find("..") != std::string::npos) {
        _current_path = "/";
    }

    // Build JSON response with file list
    String json = "{\"path\":\"";
    json += _current_path.c_str();
    json += "\",\"files\":[";

    // Try to list directory
    File root = SD.open(_current_path.c_str());
    if (!root) {
        mclog::tagError(getAppInfo().name, "Failed to open directory: %s", _current_path.c_str());
        json += "]}";
        _server->send(200, "application/json", json);
        return;
    }
    
    if (!root.isDirectory()) {
        mclog::tagError(getAppInfo().name, "Not a directory: %s", _current_path.c_str());
        root.close();
        json += "]}";
        _server->send(200, "application/json", json);
        return;
    }
    
    bool first = true;
    File file = root.openNextFile();
    
    mclog::tagInfo(getAppInfo().name, "Listing directory: %s", _current_path.c_str());
    
    int itemCount = 0;
    
    // Add parent directory if not at root
    if (_current_path != "/") {
        json += "{\"name\":\"..\",\"is_dir\":true,\"path\":\"";
        // Get parent path
        std::string parentPath = _current_path;
        size_t lastSlash = parentPath.find_last_of('/');
        if (lastSlash > 0) {
            parentPath = parentPath.substr(0, lastSlash);
        } else {
            parentPath = "/";
        }
        json += parentPath.c_str();
        json += "\"}";
        first = false;
    }
    
    while (file) {
        if (file && file.name()[0] != 0) {  // Ensure valid file
            String fullPath = file.name();
            
            // Extract just the filename from full path
            int lastSlash = fullPath.lastIndexOf('/');
            String filename = (lastSlash >= 0) ? fullPath.substring(lastSlash + 1) : fullPath;
            
            mclog::tagInfo(getAppInfo().name, "Found: %s (isDir=%d, fullPath=%s)", 
                          filename.c_str(), file.isDirectory(), fullPath.c_str());
            
            if (!first) json += ",";
            
            json += "{\"name\":\"";
            json += filename;
            json += "\",\"is_dir\":";
            json += file.isDirectory() ? "true" : "false";
            json += ",\"path\":\"";
            json += fullPath;
            json += "\"";
            
            if (!file.isDirectory()) {
                json += ",\"size\":";
                json += file.size();
            }
            
            json += "}";
            first = false;
            itemCount++;
        }
        file.close();
        file = root.openNextFile();
    }
    
    root.close();
    mclog::tagInfo(getAppInfo().name, "Directory listing complete. Found %d items", itemCount);

    json += "]}";

    _server->send(200, "application/json", json);
}

void AppWebServer::handle_file_upload()
{
    if (!_server) {
        return;
    }

    // Respond based on streaming handler outcome
    if (!_upload_in_progress && !_upload_failed && _upload_target_path.empty()) {
        _server->send(400, "application/json", "{\"success\":false,\"error\":\"No file in upload\"}");
        return;
    }

    if (_upload_failed) {
        String err = _upload_error.c_str();
        _server->send(400, "application/json", "{\"success\":false,\"error\":\"" + err + "\"}");
    } else {
        String path = _upload_target_path.c_str();
        _server->send(200, "application/json", "{\"success\":true,\"path\":\"" + path + "\"}");
        mclog::tagInfo(getAppInfo().name, "file uploaded: %s", _upload_target_path.c_str());
    }

    // Reset state for next upload
    if (_upload_file) {
        _upload_file.close();
    }
    _upload_in_progress = false;
    _upload_failed      = false;
    _upload_error.clear();
    _upload_target_path.clear();
}

void AppWebServer::handle_file_upload_stream()
{
    if (!_server) {
        return;
    }

    HTTPUpload& upload = _server->upload();

    switch (upload.status) {
        case UPLOAD_FILE_START: {
            _upload_in_progress = true;
            _upload_failed      = false;
            _upload_error.clear();
            _upload_target_path.clear();

            // Resolve target directory
            // Prefer explicit dir arg; fall back to last browsed path
            std::string uploadDir = _current_path.empty() ? std::string("/") : _current_path;
            if (_server->hasArg("dir")) {
                uploadDir = _server->arg("dir").c_str();
            }
            if (uploadDir.find("..") != std::string::npos) {
                uploadDir = "/";
            }
            if (!uploadDir.empty() && uploadDir[0] != '/') {
                uploadDir = "/" + uploadDir;
            }
            if (uploadDir.size() > 1 && uploadDir.back() == '/') {
                uploadDir.pop_back();
            }
            mclog::tagInfo(getAppInfo().name, "upload target dir resolved: %s", uploadDir.c_str());

            // Ensure target directory exists
            File dir = SD.open(uploadDir.c_str());
            if (!dir || !dir.isDirectory()) {
                if (dir) {
                    dir.close();
                }
                _upload_failed = true;
                _upload_error  = "Target directory not found";
                mclog::tagError(getAppInfo().name, "upload target missing: %s", uploadDir.c_str());
                return;
            }
            dir.close();

            // Sanitize filename
            String name = upload.filename;
            if (name.startsWith("/")) {
                name.remove(0, 1);
            }
            if (name.indexOf("..") != -1 || name.length() == 0) {
                _upload_failed = true;
                _upload_error  = "Invalid filename";
                mclog::tagError(getAppInfo().name, "invalid upload filename: %s", upload.filename.c_str());
                return;
            }

            _upload_target_path = uploadDir;
            if (_upload_target_path != "/") {
                _upload_target_path += "/";
            }
            _upload_target_path += name.c_str();

            // Open file for writing
            _upload_file = SD.open(_upload_target_path.c_str(), FILE_WRITE);
            if (!_upload_file) {
                _upload_failed = true;
                _upload_error  = "Failed to open file for writing";
                mclog::tagError(getAppInfo().name, "open for write failed: %s", _upload_target_path.c_str());
                return;
            }

            mclog::tagInfo(getAppInfo().name, "upload start: %s", _upload_target_path.c_str());
            break;
        }

        case UPLOAD_FILE_WRITE: {
            if (_upload_failed || !_upload_in_progress) {
                return;
            }
            if (!_upload_file) {
                _upload_failed = true;
                _upload_error  = "No open file";
                return;
            }

            size_t written = _upload_file.write(upload.buf, upload.currentSize);
            if (written != upload.currentSize) {
                _upload_failed = true;
                _upload_error  = "Write failed";
                mclog::tagError(getAppInfo().name, "write failed: %s", _upload_target_path.c_str());
            }
            break;
        }

        case UPLOAD_FILE_END: {
            if (_upload_file) {
                _upload_file.close();
            }
            mclog::tagInfo(getAppInfo().name, "upload end: %s (%u bytes)", _upload_target_path.c_str(), upload.totalSize);
            _upload_in_progress = false;
            break;
        }

        case UPLOAD_FILE_ABORTED: {
            if (_upload_file) {
                _upload_file.close();
            }
            _upload_failed = true;
            _upload_error  = "Upload aborted";
            _upload_in_progress = false;
            mclog::tagError(getAppInfo().name, "upload aborted: %s", _upload_target_path.c_str());
            break;
        }

        default:
            break;
    }
}

void AppWebServer::handle_file_delete()
{
    if (!_server) {
        return;
    }

    if (!_server->hasArg("file")) {
        _server->send(400, "application/json", "{\"success\":false,\"error\":\"No file specified\"}");
        return;
    }

    String filepath = _server->arg("file");
    
    // Sanitize filepath to prevent directory traversal
    if (filepath.indexOf("..") != -1) {
        _server->send(400, "application/json", "{\"success\":false,\"error\":\"Invalid filename\"}");
        return;
    }
    
    // Ensure path starts with /
    if (!filepath.startsWith("/")) {
        filepath = "/" + filepath;
    }
    
    if (SD.remove(filepath)) {
        _server->send(200, "application/json", "{\"success\":true,\"message\":\"File deleted\"}");
        mclog::tagInfo(getAppInfo().name, "file deleted: %s", filepath.c_str());
    } else {
        _server->send(400, "application/json", "{\"success\":false,\"error\":\"Delete failed\"}");
        mclog::tagError(getAppInfo().name, "failed to delete: %s", filepath.c_str());
    }
}

void AppWebServer::handle_file_download()
{
    if (!_server) {
        return;
    }

    if (!_server->hasArg("file")) {
        _server->send(400, "text/plain", "No file specified");
        return;
    }

    String filepath = _server->arg("file");
    
    // Sanitize filepath to prevent directory traversal
    if (filepath.indexOf("..") != -1) {
        _server->send(400, "text/plain", "Invalid filename");
        return;
    }
    
    // Ensure path starts with /
    if (!filepath.startsWith("/")) {
        filepath = "/" + filepath;
    }
    
    File file = SD.open(filepath);

    if (!file) {
        _server->send(404, "text/plain", "File not found");
        mclog::tagWarn(getAppInfo().name, "file not found: %s", filepath.c_str());
        return;
    }

    _server->streamFile(file, "application/octet-stream");
    file.close();
    mclog::tagInfo(getAppInfo().name, "file downloaded: %s", filepath.c_str());
}

void AppWebServer::handle_create_directory()
{
    if (!_server) {
        return;
    }

    if (!_server->hasArg("name")) {
        _server->send(400, "application/json", "{\"success\":false,\"error\":\"No folder name specified\"}");
        return;
    }

    String folderName = _server->arg("name");
    
    // Sanitize folder name
    if (folderName.indexOf("/") != -1 || folderName.indexOf("..") != -1 || folderName.length() == 0) {
        _server->send(400, "application/json", "{\"success\":false,\"error\":\"Invalid folder name\"}");
        return;
    }

    // Get the directory to create the folder in
    std::string targetDir = "/";
    if (_server->hasArg("dir")) {
        targetDir = _server->arg("dir").c_str();
        // Sanitize path
        if (targetDir.find("..") != std::string::npos) {
            targetDir = "/";
        }
        if (!targetDir.empty() && targetDir[0] != '/') {
            targetDir = "/" + targetDir;
        }
    }

    // Build the full path for the new folder
    std::string folderPath = targetDir;
    if (folderPath != "/") {
        folderPath += "/";
    }
    folderPath += folderName.c_str();

    mclog::tagInfo(getAppInfo().name, "Creating folder: %s", folderPath.c_str());

    // Ensure target directory exists and is a directory
    File parentDir = SD.open(targetDir.c_str());
    if (!parentDir || !parentDir.isDirectory()) {
        if (parentDir) {
            parentDir.close();
        }
        _server->send(400, "application/json", "{\"success\":false,\"error\":\"Target directory not found\"}");
        mclog::tagError(getAppInfo().name, "target directory missing: %s", targetDir.c_str());
        return;
    }
    parentDir.close();

    // Check if folder already exists
    File checkFolder = SD.open(folderPath.c_str());
    if (checkFolder) {
        bool isDir = checkFolder.isDirectory();
        checkFolder.close();
        if (isDir) {
            _server->send(400, "application/json", "{\"success\":false,\"error\":\"Folder already exists\"}");
            return;
        }
    }

    // Use SD.mkdir to create the folder directly
    if (SD.mkdir(folderPath.c_str())) {
        _server->send(200, "application/json", "{\"success\":true,\"message\":\"Folder created\"}");
        mclog::tagInfo(getAppInfo().name, "folder created successfully: %s", folderPath.c_str());
        return;
    }

    _server->send(400, "application/json", "{\"success\":false,\"error\":\"Failed to create folder\"}");
    mclog::tagError(getAppInfo().name, "failed to create folder: %s", folderPath.c_str());
}

void AppWebServer::handle_not_found()
{
    if (!_server) {
        return;
    }

    _server->send(404, "text/plain", "Not Found");
}
