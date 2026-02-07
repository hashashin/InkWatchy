#include "haControl.h"

#if HA_CONTROL

#include <PubSubClient.h>

// ---------- MQTT config ----------
static const char* kTopicCmd = "home/inkwatchy/cmd";
static const char* kTopicAck = "home/inkwatchy/ack";

// ---------- UI / commands ----------
struct CmdItem { const char* label; const char* cmd; };
static constexpr CmdItem kItems[] = {
    { HA_S_CMD_PRINTER_OFF,  "printer_off" },
    { HA_S_CMD_ROOMBA_START, "roomba_start" },
    { HA_S_CMD_ROOMBA_DOCK,  "roomba_dock"  },
};
static constexpr int kItemCount = (int)(sizeof(kItems) / sizeof(kItems[0]));

static int g_sel = 0;

enum class HaState : uint8_t {
    Idle,
    ConnectingWifi,
    Sending,
    Ok,
    BadSig,
    NoAck,
    WifiFail,
    MqttFail,
    PublishFail,
    TimeBad
};
static volatile HaState g_state = HaState::Idle;

// “dirty flag” para que SOLO el loop dibuje
static volatile bool g_dirty = true;

// Para detectar que hay que repintar solo lo que cambió
static int g_lastSel = -1;
static HaState g_lastState = HaState::Idle;

// request shared with wifiTask callback
static const char* g_pendingCmd = nullptr;
static volatile uint32_t g_pendingId = 0;

// ack shared from mqtt callback
static volatile bool g_ackReceived = false;
static volatile bool g_ackOk = false;
static volatile bool g_ackBadSig = false;

// ---------- Layout constants (coherentes con tu diseño actual) ----------
static constexpr int kYTitle  = 14;
static constexpr int kYState  = 30;
static constexpr int kYList0  = 52;
static constexpr int kLineH   = 16;   // tu spacing actual
static constexpr int kClearH  = 16;   // alto de borrado para una línea
static constexpr int kClearDy = 12;   // y - 12 como en tu UI previa

// ---------- helpers de dibujo (estilo menu.cpp) ----------
static void uiSetupFont()
{
    // Igual que el menu: fuente y size estables
    setFont(&FreeSansBold9pt7b);
    setTextSize(1);
    dis->setTextColor(SCBlack);
}

static int centerX(const char* txt)
{
    int16_t x1, y1;
    uint16_t w, h;
    dis->getTextBounds(txt, 0, 0, &x1, &y1, &w, &h);
    return (dis->width() - w) / 2;
}

static void clearLineAtY(int y)
{
    dis->fillRect(0, y - kClearDy, dis->width(), kClearH, SCWhite);
}

static void drawTitleOnce()
{
    dis->fillScreen(SCWhite);
    uiSetupFont();

    int x = centerX(HA_S_TITLE);
    dis->setCursor(x, kYTitle);
    dis->print(HA_S_TITLE);
}

static void drawStateLine()
{
    uiSetupFont();
    clearLineAtY(kYState);

    const char* txt = "";

    switch (g_state) {
        case HaState::Idle:           txt = HA_S_READY; break;
        case HaState::ConnectingWifi: txt = HA_S_WIFI; break;
        case HaState::Sending:        txt = HA_S_SENDING; break;
        case HaState::Ok:             txt = HA_S_OK; break;
        case HaState::BadSig:         txt = HA_S_BAD_SIG; break;
        case HaState::NoAck:          txt = HA_S_NO_ACK; break;
        case HaState::WifiFail:       txt = HA_S_WIFI_FAIL; break;
        case HaState::MqttFail:       txt = HA_S_MQTT_FAIL; break;
        case HaState::PublishFail:    txt = HA_S_PUBLISH_FAIL; break;
        case HaState::TimeBad:        txt = HA_S_NO_TIME; break;
    }

    int x = centerX(txt);
    dis->setCursor(x, kYState);
    dis->print(txt);
}

static void drawItemLine(int idx)
{
    if (idx < 0 || idx >= kItemCount) return;

    uiSetupFont();

    int y = kYList0 + idx * kLineH;
    clearLineAtY(y);

    dis->setCursor(0, y);
    dis->print(idx == g_sel ? "> " : "  ");
    dis->print(kItems[idx].label);
}

// Render inicial completo (sin firstPage/nextPage, como menu.cpp)
static void renderFullInitial()
{
    if (!dis) return;

    drawTitleOnce();
    drawStateLine();

    for (int i = 0; i < kItemCount; i++) {
        drawItemLine(i);
    }

    dUChange = true;
}

// Render “delta”: solo lo que cambia (estado + 2 líneas selección)
static void renderDeltaIfNeeded()
{
    if (!dis) return;

    bool changed = false;

    // Estado cambió
    if (g_lastState != g_state) {
        drawStateLine();
        g_lastState = g_state;
        changed = true;
    }

    // Selección cambió (repintar vieja y nueva)
    if (g_lastSel != g_sel) {
        drawItemLine(g_lastSel);
        drawItemLine(g_sel);
        g_lastSel = g_sel;
        changed = true;
    }

    if (changed) {
        dUChange = true;
    }
}

// ---------- SHA256 helpers ----------
static String toHex(const uint8_t* data, size_t len)
{
    static const char* hex = "0123456789abcdef";
    String out; out.reserve(len * 2);
    for (size_t i = 0; i < len; i++) {
        out += hex[(data[i] >> 4) & 0xF];
        out += hex[data[i] & 0xF];
    }
    return out;
}

static String sha256Hex(const String& s)
{
    uint8_t hash[32];
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);

    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, (const unsigned char*)s.c_str(), s.length());
    mbedtls_sha256_finish(&ctx, hash);

    mbedtls_sha256_free(&ctx);
    return toHex(hash, 32);
}

static int64_t nowUnix()
{
    time_t t = time(nullptr);
    return (int64_t)t;
}

static String sigCmd(const char* cmd, int64_t ts, uint32_t id)
{
    String base = String(cmd) + "|" + String((long long)ts) + "|" + String((unsigned long)id) + "|" + String(HA_MQTT_SECRET);
    return sha256Hex(base);
}

static String sigAck(uint32_t id, bool ok, int64_t ts)
{
    String base = String((unsigned long)id) + "|" + String(ok ? "true" : "false") + "|" + String((long long)ts) + "|" + String(HA_MQTT_SECRET);
    return sha256Hex(base);
}

// ---------- MQTT runtime inside wifiTask ----------
static WiFiClient g_wifi;
static PubSubClient g_mqtt(g_wifi);

static void mqttCallback(char* topic, uint8_t* payload, unsigned int length)
{
    if (strcmp(topic, kTopicAck) != 0) return;

    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, payload, length) != DeserializationError::Ok) return;

    uint32_t id = doc["id"] | 0;
    bool ok = doc["ok"] | false;
    int64_t ts = (int64_t)(doc["ts"] | 0);
    const char* sig = doc["sig"] | "";

    if (id != g_pendingId) return;

    // ventana anti-replay para ACK
    int64_t now = nowUnix();
    if (now > 100000 && ts > 0) {
        if (llabs(now - ts) > 60) return;
    }

    String expected = sigAck(id, ok, ts);
    if (expected != String(sig)) {
        g_ackBadSig = true;
        return;
    }

    g_ackReceived = true;
    g_ackOk = ok;
}

static bool mqttConnectAndSub()
{
    g_mqtt.setServer(HA_MQTT_HOST, HA_MQTT_PORT);
    g_mqtt.setCallback(mqttCallback);

    if (!g_mqtt.connect("inkwatchy-ha")) return false;
    return g_mqtt.subscribe(kTopicAck);
}

// IMPORTANTE: este task NO dibuja NUNCA.
static void wifiTaskSendMqtt()
{
    g_state = HaState::Sending;
    g_dirty = true;

    const char* cmd = g_pendingCmd;
    if (!cmd) {
        g_state = HaState::PublishFail;
        g_dirty = true;
        return;
    }

    // Prepara ID/flags ANTES de conectar/subscribir (evita race)
    uint32_t id = (uint32_t)esp_random();
    g_pendingId = id;

    g_ackReceived = false;
    g_ackOk = false;
    g_ackBadSig = false;

    syncNtp(false);

    if (!mqttConnectAndSub()) {
        g_state = HaState::MqttFail;
        g_dirty = true;
        return;
    }

    int64_t ts = nowUnix();
    if (ts < 100000) {
        g_state = HaState::TimeBad;
        g_mqtt.disconnect();
        g_dirty = true;
        return;
    }

    String sig = sigCmd(cmd, ts, id);

    StaticJsonDocument<256> doc;
    doc["cmd"] = cmd;
    doc["ts"]  = (long long)ts;
    doc["id"]  = (unsigned long)id;
    doc["sig"] = sig;

    char out[256];
    size_t n = serializeJson(doc, out, sizeof(out));

    if (!g_mqtt.publish(kTopicCmd, (const uint8_t*)out, n, false)) {
        g_state = HaState::PublishFail;
        g_mqtt.disconnect();
        g_dirty = true;
        return;
    }

    // Espera ACK hasta 6s (WiFi/MQTT a veces tardan)
    uint32_t start = millis();
    while ((millis() - start) < 6000) {
        g_mqtt.loop();
        if (g_ackBadSig) {
            g_state = HaState::BadSig;
            g_mqtt.disconnect();
            g_dirty = true;
            return;
        }
        if (g_ackReceived) {
            g_state = g_ackOk ? HaState::Ok : HaState::NoAck;
            g_mqtt.disconnect();
            g_dirty = true;
            return;
        }
        delay(10);
    }

    g_state = HaState::NoAck;
    g_mqtt.disconnect();
    g_dirty = true;
}

// ---------- place lifecycle ----------
void initHaControl()
{
    g_sel = 0;
    g_state = HaState::Idle;
    g_pendingCmd = nullptr;
    g_pendingId = 0;

    g_lastSel = g_sel;
    g_lastState = g_state;

    renderFullInitial();
    g_dirty = false;
}

void loopHaControl()
{
    resetSleepDelay();

    // Detectar fallo de wifi
    if (g_state == HaState::ConnectingWifi && !isWifiTaskCheck() && WiFi.status() != WL_CONNECTED) {
        g_state = HaState::WifiFail;
        g_dirty = true;
    }

    // Si el task cambió estado, dibuja SOLO delta
    if (g_dirty) {
        renderDeltaIfNeeded();
        g_dirty = false;
    }

    buttonState b = useButton();
    if (b == buttonState::None || b == buttonState::Unknown) {
        disUp(); // igual que loopMenu()
        return;
    }

    if (b == buttonState::Back || b == buttonState::LongBack) {
        exitHaControl();
        return;
    }

    if (b == buttonState::Up) {
        int old = g_sel;
        g_sel = (g_sel - 1 + kItemCount) % kItemCount;
        // dibuja solo 2 lineas
        drawItemLine(old);
        drawItemLine(g_sel);
        g_lastSel = g_sel;
        dUChange = true;
    }

    if (b == buttonState::Down) {
        int old = g_sel;
        g_sel = (g_sel + 1) % kItemCount;
        drawItemLine(old);
        drawItemLine(g_sel);
        g_lastSel = g_sel;
        dUChange = true;
    }

    if (b == buttonState::Menu || b == buttonState::LongMenu) {
        if (!isWifiTaskCheck()) {
            g_pendingCmd = kItems[g_sel].cmd;
            g_state = HaState::ConnectingWifi;

            // actualiza solo status
            drawStateLine();
            g_lastState = g_state;
            dUChange = true;

            createWifiTask(WIFI_CONNECTION_TRIES, wifiTaskSendMqtt, WIFI_PRIORITY_REGULAR);
        }
    }

    // Igual que loopMenu(): siempre al final
    disUp();
}

void exitHaControl()
{
    switchBack();
}

#endif
/* ---- Automation example ----

alias: InkWatchy signed MQTT commands + ack
description: ""
triggers:
  - topic: home/inkwatchy/cmd
    trigger: mqtt
conditions:
  - condition: template
    value_template: >
      {% set p = trigger.payload_json %} {% set ts = (p.ts | int(0)) %} {% set
      id = (p.id | int(0)) %} {% set cmd = (p.cmd | string) %} {% set sig =
      (p.sig | string) %} {% set expected = sha256(cmd ~ '|' ~ ts ~ '|' ~ id ~
      '|' ~ key) %} {{ sig == expected and (as_timestamp(now()) - ts) | abs < 60
      }}
actions:
  - action: mqtt.publish
    data:
      topic: home/inkwatchy/ack
      payload: >
        {% set p = trigger.payload_json %} {% set id = (p.id | int(0)) %} {% set
        ok = true %} {% set ok_str = 'true' if ok else 'false' %} {% set ts =
        as_timestamp(now()) | int %} {% set sig = sha256(id ~ '|' ~ ok_str ~ '|'
        ~ ts ~ '|' ~ key) %} {{ {"id": id, "ok": ok, "ts": ts, "sig": sig} |
        to_json }}
  - choose:
      - conditions:
          - condition: template
            value_template: "{{ trigger.payload_json.cmd == 'printer_off' }}"
        sequence:
          - action: switch.turn_off
            data: {}
            target:
              entity_id:
                - switch.smart_plug
      - conditions:
          - condition: template
            value_template: "{{ trigger.payload_json.cmd == 'roomba_start' }}"
        sequence:
          - action: vacuum.start
            data: {}
            target:
              device_id: <redacted>
      - conditions:
          - condition: template
            value_template: "{{ trigger.payload_json.cmd == 'roomba_dock' }}"
        sequence:
          - action: vacuum.return_to_base
            data: {}
            target:
              device_id: <redacted>
variables:
  key: YOUR_SUPER_SECRET
mode: queued
*/
