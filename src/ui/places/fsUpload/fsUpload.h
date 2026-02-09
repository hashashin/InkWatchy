#pragma once

#include "defines.h"

#if FS_UPLOAD

// LittleFS Web Uploader (ESP-IDF http_server)
// - BasicAuth for all endpoints
// - HMAC-SHA256 signed requests for state-changing endpoints (upload/delete)
// - Drag & drop + progress UI

void initFsUploadDisplay();
void loopFsUploadDisplay();
void exitFsUpload();

#endif