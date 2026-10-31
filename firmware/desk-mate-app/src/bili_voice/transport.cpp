#include "transport.h"

#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <NetworkClientSecure.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <esp_random.h>
#include <esp_timer.h>
#include <mbedtls/base64.h>
#include <mbedtls/sha1.h>
#include <mbedtls/sha256.h>

#include "provisioning.h"

static QueueHandle_t s_actions = nullptr;
static QueueHandle_t s_events = nullptr;
static pcm_ring_t *s_ring = nullptr;
static NetworkClientSecure s_client;
static volatile bool s_connected = false;
static volatile bool s_ready = false;
static volatile uint32_t s_generation = 0;
static volatile bool s_pending = false;
static char s_status[64] = "OFFLINE";

static const char *WS_PATH = "/v1/remote";
static const char *WS_GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

struct Pending {
    char command_id[37];
    char sha256[65];
    char server_id[37];
    bool exists;
};

struct TransportState {
    char command_id[37];
    char sha256[65];
    bool command_active;
    bool begin_sent;
    bool begin_ack;
    bool seal_pending;
    bool seal_sent;
    bool seal_ack;
    bool commit_sent;
    bool replay;
    uint32_t frame_count;
    uint32_t pcm_bytes;
    uint32_t last_ack_ms;
    uint32_t last_send_ms;
    uint32_t connect_ms;
    uint32_t ping_ms;
    uint32_t last_pong_ms;
    uint32_t generation;
    bool hello_done;
    bool hello_ready;
    Pending pending;
    mbedtls_sha256_context sha;
    bool sha_active;
    bool pending_query;
    bool pending_query_done;
};

static TransportState s_ts;
static bool send_doc(JsonDocument &doc);

/* One durable feedback slot. Preferences are used so a feedback tap survives
 * a reboot before the WSS link is available; the server is idempotent. */
static bool s_fb_pending = false;
static char s_fb_command[37] = {};
static char s_fb_rating[16] = {};
static char s_fb_dismissed[24] = {};
static int s_fb_touches = 0;
static int s_fb_countdown = 0;

static void load_feedback_nvs() {
    Preferences prefs;
    if (!prefs.begin("va_feedback", true)) return;
    String cmd = prefs.getString("cmd", "");
    String rating = prefs.getString("rating", "");
    String dismissed = prefs.getString("dismissed", "");
    int touches = prefs.getInt("touches", 0);
    int countdown = prefs.getInt("countdown", 0);
    prefs.end();
    if (cmd.length() == 36 && rating.length() && dismissed.length()) {
        snprintf(s_fb_command, sizeof(s_fb_command), "%s", cmd.c_str());
        snprintf(s_fb_rating, sizeof(s_fb_rating), "%s", rating.c_str());
        snprintf(s_fb_dismissed, sizeof(s_fb_dismissed), "%s", dismissed.c_str());
        s_fb_touches = touches; s_fb_countdown = countdown; s_fb_pending = true;
    }
}
static void save_feedback_nvs() {
    Preferences prefs;
    if (!prefs.begin("va_feedback", false)) return;
    prefs.putString("cmd", s_fb_command);
    prefs.putString("rating", s_fb_rating);
    prefs.putString("dismissed", s_fb_dismissed);
    prefs.putInt("touches", s_fb_touches);
    prefs.putInt("countdown", s_fb_countdown);
    prefs.end();
}
static void clear_feedback_nvs() {
    Preferences prefs;
    if (prefs.begin("va_feedback", false)) { prefs.clear(); prefs.end(); }
    s_fb_pending = false;
}
static bool send_feedback_doc() {
    if (!s_fb_pending || !s_connected || !s_ts.hello_done) return false;
    JsonDocument doc;
    doc["v"] = 1;
    doc["type"] = "feedback";
    doc["msg_id"] = 8;
    doc["command_id"] = s_fb_command;
    doc["rating"] = s_fb_rating;
    doc["dismissed_by"] = s_fb_dismissed;
    doc["touch_count"] = s_fb_touches;
    doc["countdown_at_dismiss"] = s_fb_countdown;
    doc["page"] = 2;
    if (!send_doc(doc)) return false;
    return true;
}

static void set_status(const char *text) {
    snprintf(s_status, sizeof(s_status), "%s", text ? text : "");
}

static void post_event(voice_event_type_t type, voice_result_t result = VOICE_RESULT_NONE,
                       int code = 0, const char *summary = nullptr, bool replayable = false) {
    if (!s_events) return;
    voice_event_t ev = {};
    ev.type = type;
    ev.result = result;
    ev.code = code;
    ev.replayable = replayable;
    if (s_ts.command_id[0]) snprintf(ev.command_id, sizeof(ev.command_id), "%s", s_ts.command_id);
    if (s_ts.sha256[0]) snprintf(ev.sha256, sizeof(ev.sha256), "%s", s_ts.sha256);
    if (summary) snprintf(ev.summary, sizeof(ev.summary), "%s", summary);
    xQueueSend(s_events, &ev, 0);
}

static int64_t now_us() { return esp_timer_get_time(); }

static void uuid4(char out[37]) {
    uint8_t bytes[16];
    esp_fill_random(bytes, sizeof(bytes));
    voice_fsm_uuid4_format(bytes, out);
}

static bool ws_write_all(const uint8_t *data, size_t len, uint32_t timeout_ms = 1500) {
    size_t sent = 0;
    uint32_t start = millis();
    while (sent < len) {
        if (!s_client.connected()) return false;
        size_t n = s_client.write(data + sent, len - sent);
        if (n == 0) {
            if (millis() - start > timeout_ms) return false;
            delay(1);
            continue;
        }
        sent += n;
    }
    return true;
}

static bool ws_send_frame(uint8_t opcode, const uint8_t *payload, size_t len) {
    static uint8_t frame[8 + VOICE_MAX_CONTROL_BYTES];
    if (len > VOICE_MAX_CONTROL_BYTES) return false;
    uint8_t header[14];
    size_t h = 0;
    header[h++] = 0x80 | (opcode & 0x0F);
    if (len <= 125) {
        header[h++] = 0x80 | (uint8_t)len;
    } else if (len <= 65535) {
        header[h++] = 0x80 | 126;
        header[h++] = (uint8_t)(len >> 8);
        header[h++] = (uint8_t)len;
    } else {
        return false;
    }
    uint8_t mask[4];
    esp_fill_random(mask, sizeof(mask));
    memcpy(header + h, mask, 4);
    h += 4;
    memcpy(frame, header, h);
    for (size_t i = 0; i < len; ++i) frame[h + i] = payload[i] ^ mask[i & 3];
    return ws_write_all(frame, h + len);
}

static bool ws_send_text(const char *text) {
    return ws_send_frame(0x1, (const uint8_t *)text, strlen(text));
}

static bool send_doc(JsonDocument &doc) {
    char buf[VOICE_MAX_CONTROL_BYTES + 1];
    size_t n = serializeJson(doc, buf, sizeof(buf));
    if (n == 0 || n >= sizeof(buf)) {
        Serial.printf("VOICE: send_doc serialize failed n=%u heap=%u\n", (unsigned)n, ESP.getFreeHeap());
        return false;
    }
    bool ok = ws_send_text(buf);
    if (!ok) Serial.printf("VOICE: send_doc write failed len=%u heap=%u\n", (unsigned)n, ESP.getFreeHeap());
    return ok;
}

static bool read_exact(uint8_t *buf, size_t len, uint32_t timeout_ms) {
    size_t got = 0;
    uint32_t start = millis();
    while (got < len) {
        int avail = s_client.available();
        if (avail > 0) {
            int n = s_client.read(buf + got, len - got);
            if (n > 0) { got += n; start = millis(); continue; }
        }
        if (!s_client.connected()) return false;
        if (millis() - start > timeout_ms) return false;
        delay(2);
    }
    return true;
}

static bool ws_handshake(const VoiceConfig &cfg, IPAddress ip) {
    uint8_t nonce[16];
    esp_fill_random(nonce, sizeof(nonce));
    char key[32];
    size_t key_len = 0;
    if (mbedtls_base64_encode((unsigned char *)key, sizeof(key), &key_len, nonce, sizeof(nonce)) != 0) return false;
    key[key_len] = 0;
    char request[1024];
    int req_len = snprintf(request, sizeof(request),
        "GET %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Upgrade: websocket\r\nConnection: Upgrade\r\n"
        "Sec-WebSocket-Key: %s\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "Authorization: Bearer %s\r\n"
        "X-Device-ID: %s\r\n\r\n",
        WS_PATH, cfg.tls_name, key, cfg.token, cfg.device_id);
    if (req_len <= 0 || req_len >= (int)sizeof(request)) {
        Serial.printf("VOICE: ws request overflow %d\n", req_len);
        return false;
    }
    if (!ws_write_all((const uint8_t *)request, (size_t)req_len, 4000)) {
        Serial.printf("VOICE: ws request write failed heap=%u\n", ESP.getFreeHeap());
        return false;
    }
    char response[1024];
    size_t used = 0;
    uint32_t start = millis();
    while (used < sizeof(response) - 1 && millis() - start < 5000) {
        int avail = s_client.available();
        if (avail <= 0) { delay(2); continue; }
        int n = s_client.read((uint8_t *)response + used, sizeof(response) - 1 - used);
        if (n <= 0) continue;
        used += n;
        response[used] = 0;
        if (strstr(response, "\r\n\r\n")) break;
    }
    response[used] = 0;
    if (!strstr(response, " 101 ")) {
        Serial.printf("VOICE: ws upgrade rejected heap=%u resp=%.80s\n", ESP.getFreeHeap(), response);
        return false;
    }
    char expect_src[128];
    snprintf(expect_src, sizeof(expect_src), "%s%s", key, WS_GUID);
    unsigned char digest[20];
    mbedtls_sha1((const unsigned char *)expect_src, strlen(expect_src), digest);
    char accept[64];
    size_t accept_len = 0;
    if (mbedtls_base64_encode((unsigned char *)accept, sizeof(accept), &accept_len, digest, sizeof(digest)) != 0) return false;
    accept[accept_len] = 0;
    if (!strstr(response, accept)) {
        Serial.printf("VOICE: ws accept mismatch heap=%u\n", ESP.getFreeHeap());
        return false;
    }
    Serial.printf("VOICE: ws upgraded heap=%u\n", ESP.getFreeHeap());
    return true;
}
static uint8_t s_ws_rx[3072];
static size_t s_ws_rx_len = 0;
static uint8_t s_ws_msg[VOICE_MAX_CONTROL_BYTES + 1];
static size_t s_ws_msg_len = 0;

static void reset_ws_rx() {
    s_ws_rx_len = 0;
    s_ws_msg_len = 0;
}

static bool connect_ws(const VoiceConfig &cfg) {
    IPAddress ip;
    const char *host = cfg.manual_host;
    if (host[0]) {
        IPAddress parsed;
        if (parsed.fromString(host)) ip = parsed;
        else ip = MDNS.queryHost(host, 2000);
    }
    int port = cfg.port;
    if ((uint32_t)ip == 0) {
        int n = MDNS.queryService("voice-assistant", "tcp");
        for (int i = 0; i < n; ++i) {
            String sid = MDNS.txt(i, "server_id");
            if (sid.length() == 0 || sid == cfg.server_id) {
                ip = MDNS.address(i);
                port = MDNS.port(i);
                if ((uint32_t)ip != 0) break;
            }
        }
    }
    if ((uint32_t)ip == 0) {
        ip = MDNS.queryHost(cfg.tls_name, 2000);
    }
    if ((uint32_t)ip == 0) {
        set_status("DISCOVERY_FAILED");
        return false;
    }
    set_status("CONNECTING");
    s_client.stop();
    // A fresh socket must never inherit a previous command's frame counter or
    // seal flags, otherwise queued audio is sent outside a begin.
    s_ts.command_active = false;
    s_ts.begin_ack = false;
    s_ts.begin_sent = false;
    s_ts.seal_pending = false;
    s_ts.seal_sent = false;
    s_ts.commit_sent = false;
    s_ts.hello_done = false;
    s_ts.hello_ready = false;
    s_client.setCACert(cfg.ca_pem);
    s_client.setHandshakeTimeout(10);
    s_client.setTimeout(3);
    Serial.printf("VOICE: TLS connect %s:%d host=%s heap=%u dma=%u dma_largest=%u\n",
                  ip.toString().c_str(), port, cfg.tls_name, ESP.getFreeHeap(),
                  (unsigned)heap_caps_get_free_size(MALLOC_CAP_DMA),
                  (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_DMA));
    int rc = s_client.connect(ip, port, cfg.tls_name, cfg.ca_pem, nullptr, nullptr);
    if (rc != 1) {
        char err[160] = {0};
        s_client.lastError(err, sizeof(err));
        Serial.printf("VOICE: TLS connect failed rc=%d err=%s heap=%u dma=%u dma_largest=%u\n",
                      rc, err, ESP.getFreeHeap(),
                      (unsigned)heap_caps_get_free_size(MALLOC_CAP_DMA),
                      (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_DMA));
        s_client.stop();
        set_status("TLS_RETRY");
        return false;
    }
    reset_ws_rx();
    if (!ws_handshake(cfg, ip)) {
        s_client.stop();
        set_status("TLS_ERROR");
        return false;
    }
    reset_ws_rx();
    s_generation++;
    s_ts.generation = s_generation;
    s_ts.hello_done = false;
    s_ts.hello_ready = false;
    s_connected = true;
    s_ready = false;
    set_status("CONNECTED");
    return true;
}

static void send_hello(const VoiceConfig &cfg) {
    JsonDocument doc;
    doc["v"] = 1;
    doc["type"] = "hello";
    doc["msg_id"] = 1;
    doc["device_id"] = cfg.device_id;
    doc["server_id"] = cfg.server_id;
    doc["firmware_version"] = "desk-mate-bili/1.0";
    char boot[37];
    Preferences prefs;
    if (prefs.begin("va_boot", false)) {
        String b = prefs.getString("boot", "");
        if (b.length() != 36) { uuid4(boot); prefs.putString("boot", boot); }
        else snprintf(boot, sizeof(boot), "%s", b.c_str());
        prefs.end();
    } else {
        uuid4(boot);
    }
    doc["boot_id"] = boot;
    send_doc(doc);
}

static void clear_pending() {
    Preferences prefs;
    if (prefs.begin("va_pending", false)) { prefs.clear(); prefs.end(); }
    memset(&s_ts.pending, 0, sizeof(s_ts.pending));
    s_pending = false;
}

static void load_pending() {
    memset(&s_ts.pending, 0, sizeof(s_ts.pending));
    Preferences prefs;
    if (!prefs.begin("va_pending", true)) return;
    String id = prefs.getString("cmd", "");
    String sha = prefs.getString("sha", "");
    String sid = prefs.getString("srv", "");
    prefs.end();
    if (id.length() == 36 && sha.length() == 64) {
        snprintf(s_ts.pending.command_id, sizeof(s_ts.pending.command_id), "%s", id.c_str());
        snprintf(s_ts.pending.sha256, sizeof(s_ts.pending.sha256), "%s", sha.c_str());
        snprintf(s_ts.pending.server_id, sizeof(s_ts.pending.server_id), "%s", sid.c_str());
        s_ts.pending.exists = true;
        s_pending = true;
    }
}

static void save_pending(const char *id, const char *sha, const char *sid) {
    Preferences prefs;
    if (!prefs.begin("va_pending", false)) return;
    prefs.putString("cmd", id);
    prefs.putString("sha", sha);
    prefs.putString("srv", sid);
    prefs.end();
    snprintf(s_ts.pending.command_id, sizeof(s_ts.pending.command_id), "%s", id);
    snprintf(s_ts.pending.sha256, sizeof(s_ts.pending.sha256), "%s", sha);
    snprintf(s_ts.pending.server_id, sizeof(s_ts.pending.server_id), "%s", sid);
    s_ts.pending.exists = true;
    s_pending = true;
}

static void start_sha() {
    if (s_ts.sha_active) mbedtls_sha256_free(&s_ts.sha);
    mbedtls_sha256_init(&s_ts.sha);
    mbedtls_sha256_starts(&s_ts.sha, 0);
    s_ts.sha_active = true;
}

static void finish_sha() {
    unsigned char digest[32];
    mbedtls_sha256_finish(&s_ts.sha, digest);
    mbedtls_sha256_free(&s_ts.sha);
    s_ts.sha_active = false;
    for (int i = 0; i < 32; ++i) snprintf(s_ts.sha256 + i * 2, 3, "%02x", digest[i]);
    s_ts.sha256[64] = 0;
}

static void handle_text(const char *text, size_t len);

static void send_begin(const voice_action_t &a) {
    snprintf(s_ts.command_id, sizeof(s_ts.command_id), "%s", a.command_id);
    s_ts.command_active = true;
    s_ts.begin_sent = true;
    s_ts.begin_ack = false;
    s_ts.seal_pending = false;
    s_ts.seal_sent = false;
    s_ts.seal_ack = false;
    s_ts.commit_sent = false;
    s_ts.frame_count = 0;
    s_ts.pcm_bytes = 0;
    s_ts.last_ack_ms = millis();
    s_ts.last_send_ms = millis();
    s_ts.replay = false;
    start_sha();
    JsonDocument doc;
    doc["v"] = 1;
    doc["type"] = "begin";
    doc["msg_id"] = 2;
    doc["command_id"] = a.command_id;
    doc["kind"] = "audio";
    JsonObject fmt = doc["format"].to<JsonObject>();
    fmt["encoding"] = "s16le";
    fmt["rate"] = 16000;
    fmt["channels"] = 1;
    doc["trigger"] = "esp32_ptt";
    if (!send_doc(doc)) {
        s_connected = false;
        s_client.stop();
    }
}

static void send_seal() {
    if (!s_ts.begin_ack || s_ts.seal_sent) return;
    finish_sha();
    JsonDocument doc;
    doc["v"] = 1;
    doc["type"] = "seal";
    doc["msg_id"] = 3;
    doc["command_id"] = s_ts.command_id;
    doc["frame_count"] = s_ts.frame_count;
    doc["pcm_bytes"] = s_ts.pcm_bytes;
    doc["sha256"] = s_ts.sha256;
    s_ts.seal_sent = true;
    s_ts.seal_pending = false;
    if (!send_doc(doc)) {
        s_connected = false;
        s_client.stop();
    }
}

static void send_commit(bool replay) {
    if (replay && s_ts.sha256[0] == 0) return;
    if (!replay && s_ts.sha_active) finish_sha();
    const VoiceConfig *cfg = vc_config_peek();
    save_pending(s_ts.command_id, s_ts.sha256, cfg->server_id);
    JsonDocument doc;
    doc["v"] = 1;
    doc["type"] = "commit";
    doc["msg_id"] = 4;
    doc["command_id"] = s_ts.command_id;
    doc["sha256"] = s_ts.sha256;
    s_ts.commit_sent = true;
    if (!send_doc(doc)) {
        s_connected = false;
        s_client.stop();
    }
}

static void send_cancel(const char *reason) {
    JsonDocument doc;
    doc["v"] = 1;
    doc["type"] = "cancel";
    doc["msg_id"] = 5;
    doc["command_id"] = s_ts.command_id;
    doc["reason"] = reason ? reason : "user";
    send_doc(doc);
}

static void send_feedback(const voice_action_t &a) {
    char work[sizeof(a.summary)];
    snprintf(work, sizeof(work), "%s", a.summary);
    char *rating = work;
    char *dismissed = strchr(work, '|');
    if (!dismissed) return;
    *dismissed++ = 0;
    char *touches = strchr(dismissed, '|');
    if (!touches) return;
    *touches++ = 0;
    char *countdown = strchr(touches, '|');
    if (!countdown) return;
    *countdown++ = 0;
    snprintf(s_fb_command, sizeof(s_fb_command), "%s", a.command_id);
    snprintf(s_fb_rating, sizeof(s_fb_rating), "%s", rating);
    snprintf(s_fb_dismissed, sizeof(s_fb_dismissed), "%s", dismissed);
    s_fb_touches = atoi(touches);
    s_fb_countdown = atoi(countdown);
    s_fb_pending = true;
    save_feedback_nvs();
    if (!send_feedback_doc()) {
        s_connected = false;
        s_client.stop();
    }
}

static void send_status() {
    JsonDocument doc;
    doc["v"] = 1;
    doc["type"] = "status";
    doc["msg_id"] = 6;
    const char *id = s_ts.pending.exists ? s_ts.pending.command_id : s_ts.command_id;
    doc["command_id"] = id;
    send_doc(doc);
}

static void send_replay_prepare(const voice_action_t &a) {
    snprintf(s_ts.command_id, sizeof(s_ts.command_id), "%s", a.command_id);
    s_ts.command_active = false;
    s_ts.replay = true;
    s_ts.seal_pending = false;
    s_ts.commit_sent = false;
    JsonDocument doc;
    doc["v"] = 1;
    doc["type"] = "replay_prepare";
    doc["msg_id"] = 7;
    doc["command_id"] = a.command_id;
    doc["kind"] = "replay";
    doc["replay_of"] = a.replay_of;
    send_doc(doc);
}

static void send_replay_commit(const voice_action_t &a) {
    snprintf(s_ts.command_id, sizeof(s_ts.command_id), "%s", a.command_id);
    snprintf(s_ts.sha256, sizeof(s_ts.sha256), "%s", a.sha256);
    send_commit(true);
}

static void drain_ring() {
    if (!s_ts.command_active || !s_ts.begin_ack || !s_ring || s_ts.seal_sent) return;
    // Audio arrives at 50 frames/s. Pace network writes to slightly faster
    // than real time instead of bursting the entire ring into TLS; this keeps
    // the server's receive/ack loop responsive and prevents PCM ring overflow.
    if (s_ts.last_send_ms != 0 && millis() - s_ts.last_send_ms < 10) return;
    uint8_t frame[VOICE_FRAME_BYTES];
    uint16_t len = 0;
    int sent_this_round = 0;
    while (pcm_ring_pop(s_ring, frame, &len) && sent_this_round < 1) {
        if (len == 0 || len > VOICE_FRAME_BYTES) return;
        uint8_t msg[8 + VOICE_FRAME_BYTES];
        msg[0] = (uint8_t)(s_ts.frame_count & 0xff);
        msg[1] = (uint8_t)((s_ts.frame_count >> 8) & 0xff);
        msg[2] = (uint8_t)((s_ts.frame_count >> 16) & 0xff);
        msg[3] = (uint8_t)((s_ts.frame_count >> 24) & 0xff);
        msg[4] = (uint8_t)(len & 0xff);
        msg[5] = (uint8_t)((len >> 8) & 0xff);
        msg[6] = 0;
        msg[7] = 0;
        memcpy(msg + 8, frame, len);
        if (!ws_send_frame(0x2, msg, 8 + len)) {
            Serial.printf("VOICE: audio frame send failed seq=%u len=%u heap=%u\n",
                          (unsigned)s_ts.frame_count, (unsigned)len, ESP.getFreeHeap());
            s_connected = false;
            s_client.stop();
            return;
        }
        if (s_ts.sha_active) mbedtls_sha256_update(&s_ts.sha, frame, len);
        s_ts.frame_count++;
        s_ts.pcm_bytes += len;
        s_ts.last_send_ms = millis();
        sent_this_round++;
        if (s_ts.frame_count > VOICE_MAX_FRAMES || s_ts.pcm_bytes > VOICE_MAX_PCM_BYTES) {
            post_event(VOICE_EV_ERROR, VOICE_RESULT_FAILED, 4, "audio overrun");
            s_ts.command_active = false;
            return;
        }
    }
    if (s_ts.seal_pending && pcm_ring_used(s_ring) == 0 && !s_ts.seal_sent) {
        send_seal();
    }
}

static void process_action(const voice_action_t &a) {
    switch (a.type) {
    case VOICE_ACT_BEGIN: send_begin(a); break;
    case VOICE_ACT_SEAL: s_ts.seal_pending = true; if (pcm_ring_used(s_ring) == 0) send_seal(); break;
    case VOICE_ACT_COMMIT: send_commit(false); break;
    case VOICE_ACT_CANCEL: {
        if (a.command_id[0]) snprintf(s_ts.command_id, sizeof(s_ts.command_id), "%s", a.command_id);
        if (s_ts.begin_sent || s_ts.command_active) send_cancel(a.reason == VOICE_REASON_TIMEOUT ? "timeout" :
            a.reason == VOICE_REASON_PAGE_EXIT ? "page_exit" :
            a.reason == VOICE_REASON_AUDIO_ERROR ? "audio_error" :
            a.reason == VOICE_REASON_NETWORK_ERROR ? "network_error" : "user");
        else post_event(VOICE_EV_CANCEL_ACK);
        s_ts.command_active = false;
        s_ts.begin_ack = false;
        s_ts.seal_pending = false;
        s_ts.seal_sent = false;
        break;
    }
    case VOICE_ACT_STATUS: send_status(); break;
    case VOICE_ACT_REPLAY_PREPARE: send_replay_prepare(a); break;
    case VOICE_ACT_REPLAY_COMMIT: send_replay_commit(a); break;
    case VOICE_ACT_FEEDBACK: send_feedback(a); break;
    default: break;
    }
}

static voice_result_t map_result(const char *state) {
    if (!state) return VOICE_RESULT_UNKNOWN;
    if (strcmp(state, "SUCCEEDED") == 0) return VOICE_RESULT_SUCCESS;
    if (strcmp(state, "PARTIAL") == 0) return VOICE_RESULT_PARTIAL;
    if (strcmp(state, "FAILED") == 0) return VOICE_RESULT_FAILED;
    if (strcmp(state, "CANCELED") == 0 || strcmp(state, "ABANDONED") == 0) return VOICE_RESULT_CANCELED;
    return VOICE_RESULT_UNKNOWN;
}

static void handle_result(JsonDocument &doc, bool from_status) {
    const char *id = doc["command_id"] | "";
    if (id[0] && s_ts.command_id[0] && strcmp(id, s_ts.command_id) != 0 &&
        !(s_ts.pending.exists && strcmp(id, s_ts.pending.command_id) == 0)) {
        return;
    }
    const char *state = doc["state"] | "";
    if (from_status) {
        JsonObject result = doc["result"].as<JsonObject>();
        if (result.isNull()) return;
        state = result["state"] | "";
    }
    voice_result_t r = map_result(state);
    const char *code = doc["code"] | "";
    const char *summary = doc["summary"] | "";
    bool replayable = doc["replayable"] | false;
    if (from_status) {
        JsonObject rr = doc["result"].as<JsonObject>();
        if (!rr.isNull()) {
            code = rr["code"] | "";
            summary = rr["summary"] | "";
            replayable = rr["replayable"] | false;
        }
    }
    if (r != VOICE_RESULT_UNKNOWN) {
        clear_pending();
        s_ts.pending_query_done = true;
        s_ts.command_active = false;
        s_ts.begin_sent = false;
        s_ts.begin_ack = false;
        s_ts.seal_pending = false;
        s_ts.seal_sent = false;
    }
    post_event(VOICE_EV_RESULT, r, code[0] ? 1 : 0, summary[0] ? summary : nullptr, replayable);
}

static void handle_text(const char *text, size_t len) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, text, len);
    if (err) return;
    const char *type = doc["type"] | "";
    if (strcmp(type, "hello_ack") == 0) {
        s_ts.hello_done = true;
        s_ts.hello_ready = doc["ready"] | false;
        s_ready = s_ts.hello_ready;
        if (s_ts.hello_ready) {
            set_status("READY");
            post_event(VOICE_EV_TRANSPORT_READY);
            if (s_fb_pending) send_feedback_doc();
            if (s_ts.pending.exists) {
                s_ts.pending_query = true;
                send_status();
            }
        } else {
            set_status((doc["code"] | "MODEL_UNAVAILABLE"));
        }
    } else if (strcmp(type, "begin_ack") == 0) {
        s_ts.begin_ack = true;
        s_ts.last_ack_ms = millis();
        post_event(VOICE_EV_BEGIN_ACK);
    } else if (strcmp(type, "append_ack") == 0) {
        s_ts.last_ack_ms = millis();
    } else if (strcmp(type, "seal_ack") == 0) {
        s_ts.seal_ack = true;
        const char *sha = doc["sha256"] | "";
        snprintf(s_ts.sha256, sizeof(s_ts.sha256), "%s", sha);
        post_event(VOICE_EV_SEAL_ACK);
    } else if (strcmp(type, "commit_ack") == 0) {
        post_event(VOICE_EV_COMMIT_ACK);
    } else if (strcmp(type, "feedback_ack") == 0) {
        const char *cid = doc["command_id"] | "";
        if (!cid[0] || strcmp(cid, s_fb_command) == 0) clear_feedback_nvs();
    } else if (strcmp(type, "cancel_ack") == 0) {
        clear_pending();
        post_event(VOICE_EV_CANCEL_ACK);
    } else if (strcmp(type, "replay_ready") == 0) {
        const char *sha = doc["sha256"] | "";
        snprintf(s_ts.sha256, sizeof(s_ts.sha256), "%s", sha);
        s_ts.replay = true;
        post_event(VOICE_EV_REPLAY_READY);
    } else if (strcmp(type, "result") == 0) {
        handle_result(doc, false);
    } else if (strcmp(type, "status_snapshot") == 0) {
        if (!(doc["known"] | false)) {
            clear_pending();
            return;
        }
        if (doc["terminal"] | false) handle_result(doc, true);
    } else if (strcmp(type, "error") == 0) {
        const char *code = doc["code"] | "PROTOCOL_ERROR";
        const char *summary = doc["summary"] | "server error";
        Serial.printf("VOICE: server error code=%s summary=%s heap=%u\n", code, summary, ESP.getFreeHeap());
        post_event(VOICE_EV_ERROR, VOICE_RESULT_FAILED, 1, summary);
        if (strcmp(code, "BUSY") == 0 || strcmp(code, "AUTH_FAILED") == 0) {
            s_connected = false;
            s_client.stop();
        }
    } else if (strcmp(type, "pong") == 0) {
        s_ts.last_pong_ms = millis();
    } else if (strcmp(type, "progress") == 0) {
        s_ts.last_pong_ms = millis();
    }
}

static void receive_ws() {
    int avail = s_client.available();
    if (avail <= 0) return;
    if (avail > (int)(sizeof(s_ws_rx) - s_ws_rx_len)) avail = (int)(sizeof(s_ws_rx) - s_ws_rx_len);
    int n = s_client.read(s_ws_rx + s_ws_rx_len, avail);
    if (n <= 0) return;
    s_ws_rx_len += n;
    size_t off = 0;
    while (s_ws_rx_len - off >= 2) {
        uint8_t b0 = s_ws_rx[off], b1 = s_ws_rx[off + 1];
        uint8_t opcode = b0 & 0x0F;
        bool masked = b1 & 0x80;
        uint64_t plen = b1 & 0x7F;
        size_t hlen = 2;
        if (plen == 126) {
            if (s_ws_rx_len - off < 4) break;
            plen = ((uint64_t)s_ws_rx[off + 2] << 8) | s_ws_rx[off + 3];
            hlen = 4;
        } else if (plen == 127) {
            if (s_ws_rx_len - off < 10) break;
            plen = 0;
            for (int i = 0; i < 8; ++i) plen = (plen << 8) | s_ws_rx[off + 2 + i];
            hlen = 10;
        }
        if (masked) hlen += 4;
        if (s_ws_rx_len - off < hlen + plen) break;
        const uint8_t *payload = s_ws_rx + off + hlen;
        if (opcode == 0x9) {
            ws_send_frame(0xA, payload, (size_t)plen);
        } else if (opcode == 0x8) {
            Serial.printf("VOICE: ws close frame heap=%u\n", ESP.getFreeHeap());
            s_connected = false;
            s_client.stop();
            return;
        } else if (opcode == 0x1 || opcode == 0x0) {
            if (s_ws_msg_len + plen > sizeof(s_ws_msg) - 1) { s_ws_msg_len = 0; }
            else {
                memcpy(s_ws_msg + s_ws_msg_len, payload, (size_t)plen);
                s_ws_msg_len += plen;
            }
            bool fin = b0 & 0x80;
            if (fin) {
                s_ws_msg[s_ws_msg_len] = 0;
                handle_text((const char *)s_ws_msg, s_ws_msg_len);
                s_ws_msg_len = 0;
            }
        }
        off += hlen + plen;
    }
    if (off > 0) {
        memmove(s_ws_rx, s_ws_rx + off, s_ws_rx_len - off);
        s_ws_rx_len -= off;
    }
}

static bool wifi_connect(const VoiceConfig &cfg) {
    if (strlen(cfg.ssid) == 0) {
        set_status("AP_MODE");
        WiFi.mode(WIFI_AP);
        WiFi.softAP("Desk-Mate-Voice", "deskmate");
        return true;
    }
    if (WiFi.status() == WL_CONNECTED) return true;
    set_status("WIFI_CONNECTING");
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.begin(cfg.ssid, cfg.password);
    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) delay(100);
    if (WiFi.status() != WL_CONNECTED) {
        set_status("WIFI_LOST");
        return false;
    }
    Serial.printf("VOICE: WiFi %s\n", WiFi.localIP().toString().c_str());
    return true;
}

static void transport_task(void *arg) {
    (void)arg;
    vc_config_load(nullptr);
    const VoiceConfig *cfg = vc_config_peek();
    vc_time_apply();
    uint32_t backoff = 1000;
    if (!cfg->valid) {
        set_status("UNPAIRED");
        for (;;) {
            vc_config_load(nullptr);
            cfg = vc_config_peek();
            if (cfg->valid) break;
            vTaskDelay(pdMS_TO_TICKS(200));
        }
        set_status("CONFIGURED");
        vc_time_apply();
    }
    load_pending();
    MDNS.begin("desk-mate-voice");
    for (;;) {
        if (!wifi_connect(*cfg)) {
            vTaskDelay(pdMS_TO_TICKS(backoff));
            backoff = backoff < 30000 ? backoff * 2 : 30000;
            continue;
        }
        if (!connect_ws(*cfg)) {
            vTaskDelay(pdMS_TO_TICKS(backoff));
            backoff = backoff < 30000 ? backoff * 2 : 30000;
            continue;
        }
        backoff = 1000;
        s_ts.last_pong_ms = millis();
        send_hello(*cfg);
        s_ts.connect_ms = millis();
        while (s_connected) {
            receive_ws();
            if (s_ts.hello_done) {
                voice_action_t action;
                while (xQueueReceive(s_actions, &action, 0) == pdTRUE) {
                    process_action(action);
                }
                if (s_ts.seal_pending && pcm_ring_used(s_ring) == 0 && !s_ts.seal_sent) send_seal();
                drain_ring();
                // Do not abort a capture merely because append_ack is late.
                // TCP/TLS send failures and the 16 s pong watchdog detect a
                // genuinely dead link; a slow ack must not cancel audio that
                // the server may already be staging.
                if (millis() - s_ts.ping_ms >= 5000) {
                    s_ts.ping_ms = millis();
                    if (millis() - s_ts.last_pong_ms > 16000) {
                        s_connected = false;
                        s_client.stop();
                    } else {
                        JsonDocument ping;
                        ping["v"] = 1;
                        ping["type"] = "ping";
                        ping["msg_id"] = 100;
                        send_doc(ping);
                    }
                }
            }
            if (millis() - s_ts.connect_ms > 300000) {
                s_connected = false;
                s_client.stop();
            }
            delay(2);
        }
        s_ready = false;
        set_status("OFFLINE");
        post_event(VOICE_EV_TRANSPORT_LOST);
        vTaskDelay(pdMS_TO_TICKS(backoff));
        backoff = backoff < 30000 ? backoff * 2 : 30000;
    }
}

bool vc_transport_begin(QueueHandle_t action_queue, QueueHandle_t event_queue, pcm_ring_t *ring) {
    load_feedback_nvs();
    s_actions = action_queue;
    s_events = event_queue;
    s_ring = ring;
    xTaskCreate(transport_task, "voice_transport", 10240, nullptr, 6, nullptr);
    return true;
}

bool vc_transport_connected() { return s_connected; }
bool vc_transport_ready() { return s_ready; }
bool vc_transport_pending() { return s_pending; }
uint32_t vc_transport_generation() { return s_generation; }
const char *vc_transport_status() { return s_status; }
