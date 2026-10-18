#include "provisioning.h"

#include <ArduinoJson.h>
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <ctype.h>
#include <stdarg.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "voice_contracts.h"

static const char *NVS_NS = "va_cfg";
static VoiceConfig g_cfg;
static bool g_loaded = false;
static String g_line;
static int g_bundle_remaining = 0;
static bool g_setup_mode = false;
static uint32_t g_setup_deadline_ms = 0;
static uint32_t g_last_bundle_rx_ms = 0;
static char *g_bundle = nullptr;
static const int kBundleMax = 16384 + 1;

static bool valid_uuid(const char *s) {
    if (!s || strlen(s) != 36) return false;
    for (int i = 0; i < 36; ++i) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            if (s[i] != '-') return false;
        } else if (!isxdigit((unsigned char)s[i])) {
            return false;
        }
    }
    return true;
}

static bool valid_token(const char *s) {
    if (!s || strlen(s) != 64) return false;
    for (int i = 0; i < 64; ++i) if (!isxdigit((unsigned char)s[i])) return false;
    return true;
}

static void copy_field(char *dst, size_t cap, const char *src) {
    if (!dst || cap == 0) return;
    if (!src) { dst[0] = 0; return; }
    strncpy(dst, src, cap - 1);
    dst[cap - 1] = 0;
}

void vc_config_load(VoiceConfig *out) {
    memset(&g_cfg, 0, sizeof(g_cfg));
    Preferences prefs;
    if (!prefs.begin(NVS_NS, true)) {
        g_loaded = true;
        if (out) *out = g_cfg;
        return;
    }
    copy_field(g_cfg.device_id, sizeof(g_cfg.device_id), prefs.getString("dev_id", "").c_str());
    copy_field(g_cfg.server_id, sizeof(g_cfg.server_id), prefs.getString("srv_id", "").c_str());
    copy_field(g_cfg.token, sizeof(g_cfg.token), prefs.getString("token", "").c_str());
    copy_field(g_cfg.tls_name, sizeof(g_cfg.tls_name), prefs.getString("tls", "").c_str());
    copy_field(g_cfg.ssid, sizeof(g_cfg.ssid), prefs.getString("ssid", "").c_str());
    copy_field(g_cfg.password, sizeof(g_cfg.password), prefs.getString("pass", "").c_str());
    copy_field(g_cfg.manual_host, sizeof(g_cfg.manual_host), prefs.getString("host", "").c_str());
    copy_field(g_cfg.ca_pem, sizeof(g_cfg.ca_pem), prefs.getString("ca", "").c_str());
    g_cfg.port = prefs.getUShort("port", 8765);
    g_cfg.trusted_time_unix_s = (int64_t)prefs.getLong64("time", 0);
    prefs.end();
    g_cfg.valid = valid_uuid(g_cfg.device_id) && valid_uuid(g_cfg.server_id) &&
                  valid_token(g_cfg.token) && strlen(g_cfg.tls_name) >= 6 &&
                  strstr(g_cfg.ca_pem, "BEGIN CERTIFICATE") != nullptr &&
                  g_cfg.port >= 1 && g_cfg.port <= 65535;
    g_loaded = true;
    if (out) *out = g_cfg;
}

bool vc_config_valid() {
    if (!g_loaded) vc_config_load(&g_cfg);
    return g_cfg.valid;
}
const VoiceConfig *vc_config_peek() {
    if (!g_loaded) vc_config_load(&g_cfg);
    return &g_cfg;
}

void vc_config_set_time(int64_t unix_s) {
    g_cfg.trusted_time_unix_s = unix_s;
}

void vc_time_apply() {
    if (!g_loaded) vc_config_load(&g_cfg);
    if (g_cfg.trusted_time_unix_s <= 1700000000LL) return;
    struct timeval tv;
    tv.tv_sec = (time_t)g_cfg.trusted_time_unix_s;
    tv.tv_usec = 0;
    settimeofday(&tv, nullptr);
}

static bool save_config(JsonDocument &doc) {
    JsonObject wifi = doc["wifi"].as<JsonObject>();
    JsonObject server = doc["server"].as<JsonObject>();
    if (wifi.isNull() || server.isNull()) return false;
    const char *ssid = wifi["ssid"] | "";
    const char *password = wifi["password"] | "";
    const char *manual = server["manual_host"] | "";
    const char *ca = server["ca_pem"] | "";
    const char *dev_id = doc["device_id"] | "";
    const char *srv_id = doc["server_id"] | "";
    const char *token = doc["device_token"] | "";
    const char *tls = server["tls_name"] | "";
    uint16_t port = (uint16_t)(server["port"] | 8765);
    int64_t trusted = (int64_t)(doc["trusted_time_unix_s"] | 0);
    if (!valid_uuid(dev_id) || !valid_uuid(srv_id) || !valid_token(token)) return false;
    if (strlen(tls) < 6 || strlen(tls) >= sizeof(g_cfg.tls_name)) return false;
    if (strlen(ca) < 64 || strlen(ca) >= sizeof(g_cfg.ca_pem)) return false;
    if (strstr(ca, "BEGIN CERTIFICATE") == nullptr) return false;
    if (strlen(ssid) >= sizeof(g_cfg.ssid) || strlen(password) >= sizeof(g_cfg.password)) return false;
    if (strlen(manual) >= sizeof(g_cfg.manual_host)) return false;
    if (port == 0) return false;
    if (trusted < 1700000000LL) return false;

    Preferences prefs;
    if (!prefs.begin(NVS_NS, false)) return false;
    bool ok = true;
    ok &= prefs.putString("dev_id", dev_id) > 0;
    ok &= prefs.putString("srv_id", srv_id) > 0;
    ok &= prefs.putString("token", token) > 0;
    ok &= prefs.putString("tls", tls) > 0;
    ok &= prefs.putString("ca", ca) > 0;
    ok &= prefs.putString("ssid", ssid) >= 0;
    ok &= prefs.putString("pass", password) >= 0;
    ok &= prefs.putString("host", manual) >= 0;
    ok &= prefs.putUShort("port", port) > 0;
    ok &= prefs.putLong64("time", trusted) > 0;
    prefs.end();
    if (!ok) return false;
    vc_config_load(&g_cfg);
    return g_cfg.valid;
}

static void send_line(const char *fmt, ...) {
    char buf[192];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    Serial.println(buf);
    Serial.flush();
}

static void release_bundle() {
    if (g_bundle) { free(g_bundle); g_bundle = nullptr; }
    g_bundle_remaining = 0;
}

static void handle_bundle() {
    if (!g_bundle) { send_line("VA_ERR BUNDLE"); return; }
    Serial.printf("VA: parsing bundle len=%u heap=%u\n", (unsigned)strlen(g_bundle), ESP.getFreeHeap());
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, g_bundle, strlen(g_bundle));
    if (err) { send_line("VA_ERR JSON"); release_bundle(); return; }
    if ((int)(doc["schema_version"] | 0) != 1) { send_line("VA_ERR SCHEMA"); release_bundle(); return; }
    if (!save_config(doc)) { send_line("VA_ERR CONFIG"); release_bundle(); return; }
    Serial.println("VA: config saved");
    vc_time_apply();
    send_line("VA_OK %s %s", g_cfg.device_id, g_cfg.server_id);
    Serial.printf("VA: paired device=%s server=%s tls=%s\n", g_cfg.device_id, g_cfg.server_id, g_cfg.tls_name);
    release_bundle();
}

static void provisioning_task(void *arg) {
    (void)arg;
    for (;;) {
        vc_provisioning_poll();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

void vc_provisioning_begin() {
    if (!g_loaded) vc_config_load(&g_cfg);
    // Single serial reader. The UI loop can be delayed by LVGL rendering, so
    // bundle bytes must be drained by a dedicated high-priority task.
    xTaskCreate(provisioning_task, "voice_provision", 4096, nullptr, 5, nullptr);
}

void vc_provisioning_poll() {
    // USB CDC line-oriented setup: "VA_SETUP", then "VA_BUNDLE <len>" and
    // exactly <len> bytes of UTF-8 JSON. No secret is echoed.
    while (Serial.available() > 0) {
        int c = Serial.read();
        if (c < 0) break;
        if (g_bundle_remaining > 0) {
            g_last_bundle_rx_ms = millis();
            static int idx = 0;
            if (!g_bundle) { g_bundle_remaining = 0; g_setup_mode = false; continue; }
            g_bundle[idx++] = (char)c;
            g_bundle[idx] = 0;
            g_bundle_remaining--;
            if (g_bundle_remaining == 0) {
                handle_bundle();
                g_setup_mode = false;
                idx = 0;
            }
            continue;
        }
        if (c == '\n' || c == '\r') {
            if (g_line.length() > 0) {
                if (g_line == "VA_SETUP") {
                    release_bundle();
                    g_setup_mode = true;
                    g_setup_deadline_ms = millis() + 120000;
                    g_line = "";
                    send_line("VA_SETUP_READY");
                    continue;
                }
                if (g_setup_mode && g_line.startsWith("VA_BUNDLE ")) {
                    int len = g_line.substring(10).toInt();
                    if (len > 0 && len <= kBundleMax - 1) {
                        release_bundle();
                        if (!g_bundle) g_bundle = (char *)malloc(kBundleMax);
                        if (!g_bundle) { send_line("VA_ERR NOMEM"); g_line = ""; continue; }
                        g_bundle_remaining = len;
                        g_bundle[0] = 0;
                        g_last_bundle_rx_ms = millis();
                        g_line = "";
                        Serial.printf("VA: bundle %d bytes\n", len);
                        continue;
                    }
                    send_line("VA_ERR LENGTH");
                }
            }
            g_line = "";
        } else if (g_line.length() < 200) {
            g_line += (char)c;
        }
    }
    if (g_bundle_remaining > 0 && (millis() - g_last_bundle_rx_ms) > 5000) {
        Serial.printf("VA: bundle incomplete remaining=%d got=%u\n", g_bundle_remaining,
                      (unsigned)(g_bundle ? strlen(g_bundle) : 0));
        send_line("VA_ERR BUNDLE_TIMEOUT");
        release_bundle();
    }
    if (g_setup_mode && (int32_t)(millis() - g_setup_deadline_ms) > 0) {
        g_setup_mode = false;
        release_bundle();
    }
}

bool vc_config_clear_voice() {
    Preferences prefs;
    if (!prefs.begin(NVS_NS, false)) return false;
    bool ok = prefs.clear();
    prefs.end();
    memset(&g_cfg, 0, sizeof(g_cfg));
    g_loaded = true;
    return ok;
}
