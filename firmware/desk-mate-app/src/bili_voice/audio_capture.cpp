#include "audio_capture.h"

#include <driver/i2c_master.h>
#include <driver/i2s_std.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "audio_convert.h"
#include <string.h>
#include "user_config.h"

static pcm_ring_t *s_ring = nullptr;
static vc_audio_error_cb_t s_on_error = nullptr;
static i2c_master_dev_handle_t s_es7210 = nullptr;
static i2s_chan_handle_t s_rx = nullptr;
static volatile bool s_active = false;
static volatile bool s_ready = false;
static volatile float s_rms = 0.0f;
static TaskHandle_t s_task = nullptr;

static const uint8_t kInputChannels = 2;
static const uint16_t kInputFrames = VOICE_FRAME_BYTES / 2;
static int32_t s_raw[kInputFrames * kInputChannels];
static int16_t s_mono[kInputFrames];
static int16_t s_mono_alt[kInputFrames];

static bool es_write(uint8_t reg, uint8_t value) {
    uint8_t buf[2] = {reg, value};
    return i2c_master_transmit(s_es7210, buf, sizeof(buf), 100) == ESP_OK;
}

static bool es_read(uint8_t reg, uint8_t *value) {
    return i2c_master_transmit_receive(s_es7210, &reg, 1, value, 1, 100) == ESP_OK;
}

static bool es_update(uint8_t reg, uint8_t mask, uint8_t value) {
    uint8_t current = 0;
    if (!es_read(reg, &current)) return false;
    current = (uint8_t)((current & ~mask) | (mask & value));
    return es_write(reg, current);
}

static bool es7210_init() {
    i2c_master_bus_handle_t bus = nullptr;
    if (i2c_master_get_bus_handle(I2C_NUM_0, &bus) != ESP_OK || !bus) return false;
    i2c_device_config_t dev = {};
    dev.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev.device_address = 0x40;
    dev.scl_speed_hz = 400000;
    if (i2c_master_bus_add_device(bus, &dev, &s_es7210) != ESP_OK) return false;

    // Sequence derived from the Espressif ES7210 driver (slave mode, MCLK pad).
    bool ok = true;
    ok &= es_write(0x00, 0xff);
    ok &= es_write(0x00, 0x41);
    ok &= es_write(0x01, 0x3f);
    ok &= es_write(0x09, 0x30);
    ok &= es_write(0x0a, 0x30);
    ok &= es_write(0x23, 0x2a);
    ok &= es_write(0x22, 0x0a);
    ok &= es_write(0x20, 0x0a);
    ok &= es_write(0x21, 0x2a);
    ok &= es_update(0x08, 0x01, 0x00);   // slave mode
    ok &= es_update(0x03, 0x80, 0x00);   // MCLK from pad
    ok &= es_write(0x40, 0x43);
    ok &= es_write(0x41, 0x70);
    ok &= es_write(0x42, 0x70);
    ok &= es_write(0x07, 0x20);
    ok &= es_write(0x02, 0xc1);
    // Select MIC1 and MIC2, 30 dB, no TDM.
    for (int i = 0; i < 4; ++i) ok &= es_update(0x43 + i, 0x10, 0x00);
    ok &= es_write(0x4b, 0xff);
    ok &= es_write(0x4c, 0xff);
    ok &= es_update(0x01, 0x0b, 0x00);
    ok &= es_write(0x4b, 0x00);
    ok &= es_update(0x43, 0x10, 0x10);
    ok &= es_update(0x43, 0x0f, 0x0f);
    ok &= es_update(0x01, 0x0b, 0x00);
    ok &= es_write(0x4b, 0x00);
    ok &= es_update(0x44, 0x10, 0x10);
    ok &= es_update(0x44, 0x0f, 0x0f);
    ok &= es_write(0x12, 0x00);          // normal I2S, no TDM
    uint8_t off = 0x3f;
    es_read(0x01, &off);
    ok &= es_write(0x01, off);
    ok &= es_write(0x06, 0x00);
    ok &= es_write(0x40, 0x43);
    for (int i = 0; i < 4; ++i) ok &= es_write(0x47 + i, 0x08);
    ok &= es_update(0x01, 0x0b, 0x00);
    ok &= es_write(0x4b, 0x00);
    ok &= es_update(0x43, 0x10, 0x10);
    ok &= es_update(0x43, 0x0f, 0x0f);
    ok &= es_update(0x44, 0x10, 0x10);
    ok &= es_update(0x44, 0x0f, 0x0f);
    ok &= es_write(0x40, 0x43);
    ok &= es_write(0x00, 0x71);
    ok &= es_write(0x00, 0x41);
    uint8_t iface = 0;
    if (es_read(0x11, &iface)) {
        iface = (uint8_t)((iface & 0x1f) | 0x60);  // 16-bit
        iface = (uint8_t)(iface & 0xfc);            // I2S normal
        ok &= es_write(0x11, iface);
    } else {
        ok = false;
    }
    return ok;
}

static bool i2s_init() {
    i2s_chan_config_t chan = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan.auto_clear = false;
    chan.dma_desc_num = 4;
    chan.dma_frame_num = 240;
    if (i2s_new_channel(&chan, nullptr, &s_rx) != ESP_OK) return false;
    i2s_std_config_t cfg = {};
    cfg.clk_cfg.sample_rate_hz = VOICE_PCM_SAMPLE_RATE;
    cfg.clk_cfg.clk_src = I2S_CLK_SRC_DEFAULT;
    cfg.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;
    cfg.slot_cfg.data_bit_width = I2S_DATA_BIT_WIDTH_32BIT;
    cfg.slot_cfg.slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO;
    cfg.slot_cfg.slot_mode = I2S_SLOT_MODE_STEREO;
    cfg.slot_cfg.slot_mask = I2S_STD_SLOT_BOTH;
    cfg.slot_cfg.ws_width = I2S_DATA_BIT_WIDTH_32BIT;
    cfg.slot_cfg.ws_pol = false;
    cfg.slot_cfg.bit_shift = true;
    cfg.gpio_cfg.mclk = BSP_I2S_MCLK;
    cfg.gpio_cfg.bclk = BSP_I2S_SCLK;
    cfg.gpio_cfg.ws = BSP_I2S_LCLK;
    cfg.gpio_cfg.dout = I2S_GPIO_UNUSED;
    cfg.gpio_cfg.din = BSP_I2S_DSIN;
    cfg.gpio_cfg.invert_flags.mclk_inv = false;
    cfg.gpio_cfg.invert_flags.bclk_inv = false;
    cfg.gpio_cfg.invert_flags.ws_inv = false;
    if (i2s_channel_init_std_mode(s_rx, &cfg) != ESP_OK) return false;
    if (i2s_channel_enable(s_rx) != ESP_OK) return false;
    return true;
}

static void report_error(int code, const char *summary) {
    s_active = false;
    if (s_on_error) s_on_error(code, summary);
}

static void audio_task(void *arg) {
    (void)arg;
    for (;;) {
        if (!s_active) {
            vTaskDelay(pdMS_TO_TICKS(5));
            continue;
        }
        size_t bytes = 0;
        esp_err_t err = i2s_channel_read(s_rx, s_raw, sizeof(s_raw), &bytes, pdMS_TO_TICKS(100));
        if (err != ESP_OK || bytes != sizeof(s_raw)) {
            report_error(2, "I2S read failed");
            continue;
        }
        audio_pcm32_to_s16_mono(s_raw, kInputFrames, kInputChannels, 0, s_mono);
        audio_pcm32_to_s16_mono(s_raw, kInputFrames, kInputChannels, 1, s_mono_alt);
        float rms0 = 0, peak0 = 0, rms1 = 0, peak1 = 0;
        audio_levels_s16(s_mono, kInputFrames, &rms0, &peak0);
        audio_levels_s16(s_mono_alt, kInputFrames, &rms1, &peak1);
        // The two ES7210 mics can differ in level/phase; averaging can cancel
        // speech. Pick the stronger channel per 20 ms frame instead.
        if (rms1 > rms0) {
            memcpy(s_mono, s_mono_alt, sizeof(s_mono));
            s_rms = rms1;
        } else {
            s_rms = rms0;
        }
        // ES7210 is already at its 37.5 dB maximum; the board's MEMS mic still
        // delivers a low digital level. Apply 12 dB software gain with
        // saturation so the server/STT sees normal speech level.
        for (size_t i = 0; i < kInputFrames; ++i) {
            int32_t v = (int32_t)s_mono[i] * 4;
            if (v > 32767) v = 32767;
            if (v < -32768) v = -32768;
            s_mono[i] = (int16_t)v;
        }
        s_rms *= 4.0f;
        float rms = s_rms, peak = peak0 > peak1 ? peak0 : peak1;
        if (!pcm_ring_push(s_ring, (const uint8_t *)s_mono, VOICE_FRAME_BYTES)) {
            // Keep the newest audio; a transient slow WSS write must not turn
            // the whole PTT into a fatal command failure.
            uint8_t drop[VOICE_FRAME_BYTES];
            uint16_t drop_len = 0;
            pcm_ring_pop(s_ring, drop, &drop_len);
            if (!pcm_ring_push(s_ring, (const uint8_t *)s_mono, VOICE_FRAME_BYTES)) {
                report_error(3, "PCM ring full");
            }
        }
    }
}

bool vc_audio_init(pcm_ring_t *ring, vc_audio_error_cb_t on_error) {
    s_ring = ring;
    s_on_error = on_error;
    Serial.println("VOICE: es7210 init");
    if (!es7210_init()) {
        Serial.println("VOICE: ES7210 init failed");
        return false;
    }
    Serial.println("VOICE: i2s init");
    if (!i2s_init()) {
        Serial.println("VOICE: I2S init failed");
        return false;
    }
    s_ready = true;
    xTaskCreate(audio_task, "voice_audio", 4096, nullptr, 5, &s_task);
    Serial.printf("VOICE: microphone ready heap=%u\n", ESP.getFreeHeap());
    return true;
}

bool vc_audio_start() {
    if (!s_ready) return false;
    s_active = true;
    return true;
}
void vc_audio_stop() { s_active = false; }
bool vc_audio_active() { return s_active; }
void vc_audio_reset() {
    s_active = false;
    if (s_ring) pcm_ring_reset(s_ring);
    s_rms = 0.0f;
}
float vc_audio_last_rms() { return s_rms; }
bool vc_audio_ready() { return s_ready; }
