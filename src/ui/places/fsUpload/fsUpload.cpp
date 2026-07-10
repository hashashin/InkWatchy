#include "fsUpload.h"

#if FS_UPLOAD

#include "esp_http_server.h"
#include "mbedtls/base64.h"

// -----------------------------------------------------------------------------
// UI state (general_page style)
// -----------------------------------------------------------------------------
static int g_lineStatus = -1;
static int g_lineIp = -1;
static int g_lineUrl = -1;
static int g_lineFs = -1;

static String g_lastStatus;
static String g_lastIp;
static String g_lastUrl;
static String g_lastFs;

static bool g_wifiWanted = false;
static bool g_serverWanted = false;
static bool g_wifiTurnedOnByApp = false;

// -----------------------------------------------------------------------------
// HTTP server state
// -----------------------------------------------------------------------------
static httpd_handle_t g_server = nullptr;

// -----------------------------------------------------------------------------
// Config
// -----------------------------------------------------------------------------
#ifndef FS_UPLOAD_HTTP_USER
#define FS_UPLOAD_HTTP_USER "admin"
#endif

#ifndef FS_UPLOAD_HTTP_PASS
#define FS_UPLOAD_HTTP_PASS "inkwatchy"
#endif

#ifndef FS_UPLOAD_MAX_UPLOAD_BYTES
#define FS_UPLOAD_MAX_UPLOAD_BYTES (6 * 1024 * 1024)
#endif

// -----------------------------------------------------------------------------
// BasicAuth
// -----------------------------------------------------------------------------
static bool getHeader(httpd_req_t *req, const char *name, String &out)
{
    size_t len = httpd_req_get_hdr_value_len(req, name);
    if (len == 0)
        return false;
    std::unique_ptr<char[]> buf(new char[len + 1]);
    if (httpd_req_get_hdr_value_str(req, name, buf.get(), len + 1) != ESP_OK)
        return false;
    out = String(buf.get());
    return true;
}

static bool checkBasicAuth(httpd_req_t *req)
{
    String auth;
    if (!getHeader(req, "Authorization", auth))
        return false;

    if (!auth.startsWith("Basic "))
        return false;
    String b64 = auth.substring(6);
    b64.trim();

    while ((b64.length() % 4) != 0)
        b64 += "=";

    size_t olen = 0;
    std::unique_ptr<uint8_t[]> out(new uint8_t[b64.length() + 4]);
    if (mbedtls_base64_decode(out.get(), b64.length() + 4, &olen,
                              (const uint8_t *)b64.c_str(), b64.length()) != 0)
    {
        return false;
    }
    out.get()[olen] = 0;

    String up((const char *)out.get());
    int sep = up.indexOf(':');
    if (sep < 0)
        return false;

    String u = up.substring(0, sep);
    String p = up.substring(sep + 1);

    return (u == FS_UPLOAD_HTTP_USER) && (p == FS_UPLOAD_HTTP_PASS);
}

static void sendAuthChallenge(httpd_req_t *req)
{
    httpd_resp_set_status(req, "401 Unauthorized");
    httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"InkWatchy\"");
    httpd_resp_send(req, "Auth required", HTTPD_RESP_USE_STRLEN);
}

static uint8_t hexNibble(char c)
{
    if (c >= '0' && c <= '9')
        return (uint8_t)(c - '0');
    if (c >= 'a' && c <= 'f')
        return (uint8_t)(c - 'a' + 10);
    if (c >= 'A' && c <= 'F')
        return (uint8_t)(c - 'A' + 10);
    return 0xFF;
}

static void urlDecodeInPlace(String &s)
{
    String out;
    out.reserve(s.length());

    for (size_t i = 0; i < s.length(); i++)
    {
        char c = s[i];
        if (c == '+')
        {
            out += ' ';
        }
        else if (c == '%' && (i + 2) < s.length())
        {
            uint8_t a = hexNibble(s[i + 1]);
            uint8_t b = hexNibble(s[i + 2]);
            if (a != 0xFF && b != 0xFF)
            {
                out += (char)((a << 4) | b);
                i += 2;
            }
            else
            {
                out += c;
            }
        }
        else
        {
            out += c;
        }
    }

    s = out;
}

static bool getQueryParam(httpd_req_t *req, const char *key, String &out)
{
    size_t qlen = httpd_req_get_url_query_len(req);
    if (qlen == 0)
        return false;

    std::unique_ptr<char[]> q(new char[qlen + 1]);
    if (httpd_req_get_url_query_str(req, q.get(), qlen + 1) != ESP_OK)
        return false;

    char val[256];
    if (httpd_query_key_value(q.get(), key, val, sizeof(val)) != ESP_OK)
        return false;

    out = String(val);
    urlDecodeInPlace(out);
    return true;
}

static bool pathLooksSafe(const String &p)
{
    if (p.length() < 1)
        return false;
    if (!p.startsWith("/"))
        return false;
    if (p.indexOf("..") >= 0)
        return false;
    if (p.indexOf("//") >= 0)
        return false;
    return true;
}

// -----------------------------------------------------------------------------
// HTML UI (embedded)
// -----------------------------------------------------------------------------
static const char kIndexHtml[] = R"HTML(
<!doctype html>
<html>
<head>
<meta charset="utf-8"/>
<meta name="viewport" content="width=device-width, initial-scale=1"/>
<title>InkWatchy FS Upload</title>
<style>
body{font-family:system-ui,-apple-system,Segoe UI,Roboto,Ubuntu,sans-serif; margin:16px; max-width:860px;}
.card{border:1px solid #ddd; border-radius:12px; padding:14px; margin:12px 0;}
h2{margin:0 0 10px 0;}
small{color:#666;}
.row{display:flex; gap:10px; flex-wrap:wrap; align-items:center;}
input[type="text"]{padding:8px; border-radius:10px; border:1px solid #ccc; width:260px;}
button{padding:9px 12px; border-radius:10px; border:1px solid #aaa; background:#f7f7f7; cursor:pointer;}
button:active{transform:translateY(1px);}
#drop{border:2px dashed #aaa; border-radius:12px; padding:18px; text-align:center; color:#444;}
#drop.drag{border-color:#000;}
table{width:100%; border-collapse:collapse; margin-top:10px;}
td,th{border-bottom:1px solid #eee; padding:8px; text-align:left;}
.progress{height:10px; background:#eee; border-radius:999px; overflow:hidden;}
.progress > div{height:100%; background:#222; width:0%;}
.mono{font-family:ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, monospace;}
</style>
</head>
<body>
<h2>InkWatchy LittleFS</h2>
<small>BasicAuth required.</small>

<div class="card">
  <div class="row">
    <div>
      <div class="mono">Target dir</div>
      <input id="dir" type="text" value="/" />
    </div>
    <div>
      <div class="mono">&nbsp;</div>
      <button id="save">Save</button>
      <button id="refresh">Refresh list</button>
    </div>
  </div>
</div>

<div class="card">
  <div id="drop">Drop files here or <input id="file" type="file" multiple /></div>
  <div id="jobs"></div>
</div>

<div class="card">
  <div class="row" style="justify-content:space-between">
    <div class="mono">Files</div>
    <div class="mono" id="fsinfo"></div>
  </div>
  <table id="tbl">
    <thead><tr><th>Path</th><th>Size</th><th></th></tr></thead>
    <tbody></tbody>
  </table>
</div>

<script>
const $ = (id)=>document.getElementById(id);

localStorage.removeItem('ink_key');
let currentDir = localStorage.getItem('ink_curdir') || '/';

function setDir(d){
  if (!d.startsWith('/')) d = '/' + d;
  d = d.replace(/\/+/g,'/');
  if (d.length > 1 && d.endsWith('/')) d = d.slice(0,-1);
  currentDir = d;
  localStorage.setItem('ink_curdir', currentDir);
  $('dir').value = currentDir;
}

function parentDir(d){
  if (d === '/') return '/';
  const i = d.lastIndexOf('/');
  return i <= 0 ? '/' : d.slice(0, i);
}

function joinDir(base, name){
  if (base === '/') return '/' + name;
  return base + '/' + name;
}

function fmtSize(n){
  if (n < 1024) return n + ' B';
  if (n < 1024*1024) return (n/1024).toFixed(1) + ' KB';
  return (n/(1024*1024)).toFixed(2) + ' MB';
}

async function authenticatedFetch(method, url, body){
  return fetch(url, {
    method,
    body,
    credentials: 'include',
    headers: {
      'Content-Type': 'application/octet-stream'
    }
  });
}

function addJob(name){
  const wrap = document.createElement('div');
  wrap.style.marginTop = '10px';
  wrap.innerHTML = `
    <div class="row" style="justify-content:space-between">
      <div class="mono">${name}</div>
      <div class="mono status">queued</div>
    </div>
    <div class="progress"><div></div></div>
  `;
  $('jobs').prepend(wrap);
  return {
    setStatus: (t)=>wrap.querySelector('.status').textContent=t,
    setPct: (p)=>wrap.querySelector('.progress > div').style.width = p + '%'
  };
}

async function uploadFile(f){
  const path = joinDir(currentDir, f.name);
  const job = addJob(path);

  const buf = await f.arrayBuffer();
  const url = '/upload?path=' + encodeURIComponent(path);
  job.setStatus('uploading...');
  job.setPct(5);

  const res = await authenticatedFetch('POST', url, buf);

  if (!res.ok) {
    const t = await res.text();
    job.setStatus('FAIL ' + res.status + ' ' + t);
    job.setPct(100);
    return;
  }
  job.setPct(100);
  job.setStatus('OK');
}

async function delFile(path){
  if (!confirm('Delete ' + path + '?')) return;
  try {
    const url = '/delete?path=' + encodeURIComponent(path);
    const res = await authenticatedFetch('POST', url, null);
    if (!res.ok) {
      alert('Delete failed: ' + res.status + ' ' + await res.text());
      return;
    }
    await refreshList();
  } catch(err) {
    alert('Delete failed: ' + err);
  }
}

async function refreshList(){
  const res = await fetch('/ls?dir=' + encodeURIComponent(currentDir), { credentials: 'include' });
  if (!res.ok) {
    $('fsinfo').textContent = 'ls failed';
    return;
  }
  const j = await res.json();
  $('fsinfo').textContent = (j.fs || '') + '   dir: ' + currentDir;

  const tb = $('tbl').querySelector('tbody');
  tb.innerHTML = '';

  if (currentDir !== '/') {
    const trUp = document.createElement('tr');
    trUp.innerHTML = `
      <td class="mono"><button id="upBtn">..</button></td>
      <td class="mono"></td>
      <td></td>
    `;
    trUp.querySelector('#upBtn').onclick = ()=>{
      setDir(parentDir(currentDir));
      refreshList();
    };
    tb.appendChild(trUp);
  }

  (j.items || []).forEach(it=>{
    const tr = document.createElement('tr');

    const name = (it.path || '').split('/').pop();

    if (it.dir) {
      tr.innerHTML = `
        <td class="mono"><button class="openDir">[${name}]</button></td>
        <td class="mono">dir</td>
        <td><button class="delBtn">Delete</button></td>
      `;
      tr.querySelector('.openDir').onclick = ()=>{
        setDir(it.path);
        refreshList();
      };
    } else {
      tr.innerHTML = `
        <td class="mono">${name}</td>
        <td class="mono">${fmtSize(it.size)}</td>
        <td><button class="delBtn">Delete</button></td>
      `;
    }

    tr.querySelector('.delBtn').onclick = ()=>delFile(it.path);
    tb.appendChild(tr);
  });
}

$('save').onclick = ()=>{
  localStorage.setItem('ink_dir', $('dir').value);
  localStorage.setItem('ink_curdir', $('dir').value);
  setDir($('dir').value);
  alert('Saved locally');
};
$('refresh').onclick = refreshList;

$('dir').value = localStorage.getItem('ink_dir') || '/';
setDir($('dir').value);

$('file').addEventListener('change', async (e)=>{
  const files = [...e.target.files];
  for (const f of files) {
    try { await uploadFile(f); } catch(err){ alert(err); }
  }
  await refreshList();
});

const drop = $('drop');
drop.addEventListener('dragover', e=>{ e.preventDefault(); drop.classList.add('drag'); });
drop.addEventListener('dragleave', ()=>drop.classList.remove('drag'));
drop.addEventListener('drop', async (e)=>{
  e.preventDefault(); drop.classList.remove('drag');
  const files = [...e.dataTransfer.files];
  for (const f of files) {
    try { await uploadFile(f); } catch(err){ alert(err); }
  }
  await refreshList();
});

refreshList();
</script>
</body>
</html>
)HTML";

// -----------------------------------------------------------------------------
// /ls JSON
// -----------------------------------------------------------------------------
static void jsonEscape(String &s)
{
    s.replace("\\", "\\\\");
    s.replace("\"", "\\\"");
}

static esp_err_t handle_ls(httpd_req_t *req)
{
    if (!checkBasicAuth(req))
    {
        sendAuthChallenge(req);
        return ESP_OK;
    }

    if (!fsSetup())
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "fs mount fail");
        return ESP_FAIL;
    }

    String dir = "/";
    String qdir;
    if (getQueryParam(req, "dir", qdir) && qdir.length())
        dir = qdir;

    if (!pathLooksSafe(dir))
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad dir");
        return ESP_FAIL;
    }

    File root = LittleFS.open(dir);
    if (!root || !root.isDirectory())
    {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "dir not found");
        return ESP_OK;
    }

    String out = "{";
    String fs = getLittleFsSizeString();
    jsonEscape(fs);
    out += "\"fs\":\"" + fs + "\",\"items\":[";

    bool first = true;
    File f = root.openNextFile();
    while (f)
    {
        String path = String(f.path());
        bool isDir = f.isDirectory();
        size_t sz = isDir ? 0 : (size_t)f.size();

        if (!first)
            out += ",";
        first = false;

        String p = path;
        jsonEscape(p);

        out += "{\"path\":\"" + p + "\",\"size\":" + String((uint32_t)sz) + ",\"dir\":" + (isDir ? "true" : "false") + "}";
        f = root.openNextFile();
    }

    out += "]}";

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, out.c_str(), out.length());
    return ESP_OK;
}

// -----------------------------------------------------------------------------
// / (index)
// -----------------------------------------------------------------------------
static esp_err_t handle_root(httpd_req_t *req)
{
    if (!checkBasicAuth(req))
    {
        sendAuthChallenge(req);
        return ESP_OK;
    }
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_send(req, kIndexHtml, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// -----------------------------------------------------------------------------
// /upload
// -----------------------------------------------------------------------------
static esp_err_t handle_upload(httpd_req_t *req)
{
    if (!checkBasicAuth(req))
    {
        sendAuthChallenge(req);
        return ESP_OK;
    }

    if (!fsSetup())
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "fs mount fail");
        return ESP_FAIL;
    }

    String path;
    if (!getQueryParam(req, "path", path) || !pathLooksSafe(path))
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad path");
        return ESP_FAIL;
    }

    if (req->content_len <= 0 || req->content_len > FS_UPLOAD_MAX_UPLOAD_BYTES)
    {
        httpd_resp_set_status(req, "413 Payload Too Large");
        httpd_resp_set_type(req, "text/plain");
        httpd_resp_send(req, "too big", HTTPD_RESP_USE_STRLEN);
        return ESP_OK;
    }

    File file = LittleFS.open(path, FILE_WRITE, true);
    if (!file)
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "open fail");
        return ESP_FAIL;
    }

    static const int kBuf = 2048;
    std::unique_ptr<uint8_t[]> buf(new uint8_t[kBuf]);

    int remaining = req->content_len;
    while (remaining > 0)
    {
        int toRead = (remaining > kBuf) ? kBuf : remaining;
        int r = httpd_req_recv(req, (char *)buf.get(), toRead);
        if (r <= 0)
        {
            file.close();
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "recv fail");
            return ESP_FAIL;
        }
        size_t w = file.write(buf.get(), (size_t)r);
        if (w != (size_t)r)
        {
            file.close();
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "write fail");
            return ESP_FAIL;
        }
        remaining -= r;
    }

    file.close();

    cleanFontCache();
    cleanImgCache();

    httpd_resp_set_type(req, "text/plain");
    httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// -----------------------------------------------------------------------------
// /delete
// -----------------------------------------------------------------------------
static esp_err_t handle_delete(httpd_req_t *req)
{
    if (!checkBasicAuth(req))
    {
        sendAuthChallenge(req);
        return ESP_OK;
    }

    if (!fsSetup())
    {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "fs mount fail");
        return ESP_FAIL;
    }

    String path;
    if (!getQueryParam(req, "path", path) || !pathLooksSafe(path))
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad path");
        return ESP_FAIL;
    }

    bool ok = false;
    File f = LittleFS.open(path);
    if (f)
    {
        if (f.isDirectory())
        {
            f.close();
            ok = removeDir(path);
        }
        else
        {
            f.close();
            ok = LittleFS.remove(path);
        }
    }

    cleanFontCache();
    cleanImgCache();

    if (!ok)
    {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "not found");
        return ESP_OK;
    }
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// -----------------------------------------------------------------------------
// Start/stop server
// -----------------------------------------------------------------------------
static bool startServer()
{
    if (g_server)
        return true;

    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.max_uri_handlers = 12;
    cfg.stack_size = 8192;
    cfg.recv_wait_timeout = 15;
    cfg.send_wait_timeout = 15;
    cfg.lru_purge_enable = true;

    if (httpd_start(&g_server, &cfg) != ESP_OK)
    {
        g_server = nullptr;
        return false;
    }

    httpd_uri_t root = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = handle_root,
        .user_ctx = NULL};
    httpd_uri_t ls = {
        .uri = "/ls",
        .method = HTTP_GET,
        .handler = handle_ls,
        .user_ctx = NULL};
    httpd_uri_t up = {
        .uri = "/upload",
        .method = HTTP_POST,
        .handler = handle_upload,
        .user_ctx = NULL};
    httpd_uri_t del = {
        .uri = "/delete",
        .method = HTTP_POST,
        .handler = handle_delete,
        .user_ctx = NULL};

    httpd_register_uri_handler(g_server, &root);
    httpd_register_uri_handler(g_server, &ls);
    httpd_register_uri_handler(g_server, &up);
    httpd_register_uri_handler(g_server, &del);

    return true;
}

static void stopServer()
{
    if (g_server)
    {
        httpd_stop(g_server);
        g_server = nullptr;
    }
}

// -----------------------------------------------------------------------------
// Buttons
// -----------------------------------------------------------------------------
static void btnWifiOn()
{
    if (wifiStatusWrap() == wifiStatusSimple::WifiOff)
        g_wifiTurnedOnByApp = true;

    g_wifiWanted = true;
    turnOnWifiRegular();
    vibrateMotor(VIBRATION_ACTION_TIME);
}

static void btnWifiOff()
{
    g_wifiTurnedOnByApp = false;
    g_wifiWanted = false;
    g_serverWanted = false;
    stopServer();
    turnOffWifi();
    vibrateMotor(VIBRATION_ACTION_TIME);
}

static void btnServerOn()
{
    g_serverWanted = true;

    if (wifiStatusWrap() == wifiStatusSimple::WifiOff)
    {
        g_wifiTurnedOnByApp = true;
        g_wifiWanted = true;
        turnOnWifiRegular();
    }
    vibrateMotor(VIBRATION_ACTION_TIME);
}

static void btnServerOff()
{
    g_serverWanted = false;
    stopServer();
    vibrateMotor(VIBRATION_ACTION_TIME);
}

// -----------------------------------------------------------------------------
// Place API
// -----------------------------------------------------------------------------
void initFsUploadDisplay()
{
    g_wifiTurnedOnByApp = false;

    init_general_page(6);
    general_page_set_title(MENU_FS_UPLOAD);
    genpage_set_center();

    GeneralPageButton button[] = {
        GeneralPageButton{"WiFi ON", btnWifiOn},
        GeneralPageButton{"WiFi OFF", btnWifiOff},
        GeneralPageButton{"Server ON", btnServerOn},
        GeneralPageButton{"Server OFF", btnServerOff},
    };
    general_page_set_buttons(button, 4);

    genpage_add("Status");
    g_lineStatus = genpage_add("...");

    g_lineIp = genpage_add("IP: 0.0.0.0");
    g_lineUrl = genpage_add("URL: http://0.0.0.0/");
    g_lineFs = genpage_add("LittleFS: ...");

    g_lastStatus = "";
    g_lastIp = "";
    g_lastUrl = "";
    g_lastFs = "";

    general_page_set_main();
}

void loopFsUploadDisplay()
{
    resetSleepDelay(SLEEP_EVERY_MS);

    String st = wifiStatus();
    if (st != g_lastStatus)
    {
        g_lastStatus = st;
        genpage_change(g_lastStatus.c_str(), g_lineStatus);
    }

    String ip = "0.0.0.0";
    if (wifiStatusWrap() != wifiStatusSimple::WifiOff)
    {
        IPAddress a = WiFi.localIP();
        ip = a.toString();
    }

    String ipLine = "IP: " + ip;
    if (ipLine != g_lastIp)
    {
        g_lastIp = ipLine;
        genpage_change(g_lastIp.c_str(), g_lineIp);
    }

    String urlLine = "URL: http://" + ip + "/";
    if (urlLine != g_lastUrl)
    {
        g_lastUrl = urlLine;
        genpage_change(g_lastUrl.c_str(), g_lineUrl);
    }

    String fsLine = getLittleFsSizeString();
    if (fsLine != g_lastFs)
    {
        g_lastFs = fsLine;
        genpage_change(g_lastFs.c_str(), g_lineFs);
    }

    if (g_serverWanted)
    {
        if (wifiStatusWrap() != wifiStatusSimple::WifiOff)
        {
            if (!g_server)
            {
                startServer();
            }
        }
        resetSleepDelay(SLEEP_EVERY_MS);
    }

    general_page_set_main();
    slint_loop();
}

void exitFsUpload()
{
    g_serverWanted = false;
    stopServer();

    if (g_wifiTurnedOnByApp)
    {
        g_wifiWanted = false;
        turnOffWifi();
        g_wifiTurnedOnByApp = false;
    }
}

#endif // FS_UPLOAD
