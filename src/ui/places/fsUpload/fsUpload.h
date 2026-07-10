#pragma once

#include "defines.h"

#if FS_UPLOAD

// LittleFS Web Uploader (ESP-IDF http_server)
// - BasicAuth for all endpoints
// - Drag & drop + progress UI

void initFsUploadDisplay();
void loopFsUploadDisplay();
void exitFsUpload();

#endif
