#pragma once
#include <NimBLEDevice.h>
#include <NimBLEHIDDevice.h>
#include <atomic>

/* Consumer Control HID: native macOS media keys, no companion daemon. */
static NimBLEHIDDevice *media_hid;
static NimBLECharacteristic *media_input;
static std::atomic<bool> media_secure(false), media_subscribed(false);
static uint32_t media_release_at;
static bool media_key_down;

class MediaServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer *s, NimBLEConnInfo &info) override {
        Serial.println("BLE: host connected, securing");
        s->updateConnParams(info.getConnHandle(), 12, 24, 0, 200);
        NimBLEDevice::startSecurity(info.getConnHandle());
    }
    void onAuthenticationComplete(NimBLEConnInfo &info) override {
        media_secure.store(info.isEncrypted());
        Serial.printf("BLE: encrypted=%d bonded=%d\n", info.isEncrypted(), info.isBonded());
        if (!info.isEncrypted()) NimBLEDevice::getServer()->disconnect(info.getConnHandle());
    }
    void onDisconnect(NimBLEServer *, NimBLEConnInfo &, int reason) override {
        media_secure.store(false); media_subscribed.store(false);
        Serial.printf("BLE: disconnected %d; advertising again\n", reason);
    }
};
class MediaInputCallbacks : public NimBLECharacteristicCallbacks {
    void onSubscribe(NimBLECharacteristic *, NimBLEConnInfo &, uint16_t value) override {
        media_subscribed.store((value & 1) != 0);
        Serial.printf("BLE: media notifications=%u\n", value);
    }
};
static bool media_ready(void) { return media_secure.load() && media_subscribed.load(); }
static void media_begin(void) {
    static uint8_t report[] = {
        0x05, 0x0C,       /* Usage page: Consumer */
        0x09, 0x01,       /* Consumer Control */
        0xA1, 0x01,       /* Application collection */
        0x85, 0x01,       /* Report ID 1 */
        0x15, 0x00,       /* Logical min 0 */
        0x26, 0xFF, 0x03, /* Logical max 1023 */
        0x19, 0x00,       /* Usage min 0 */
        0x2A, 0xFF, 0x03, /* Usage max 1023 */
        0x75, 0x10,       /* Report size 16 */
        0x95, 0x01,       /* Report count 1 */
        0x81, 0x00,       /* Input (data, array, absolute) */
        0xC0
    };
    NimBLEDevice::init("Desk-Mate");
    NimBLEDevice::setSecurityAuth(true, false, true);
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
    NimBLEServer *s = NimBLEDevice::createServer();
    static MediaServerCallbacks server_callbacks;
    static MediaInputCallbacks input_callbacks;
    s->setCallbacks(&server_callbacks, false);
    s->advertiseOnDisconnect(true);
    media_hid = new NimBLEHIDDevice(s);
    media_input = media_hid->getInputReport(1);
    media_input->setCallbacks(&input_callbacks);
    media_hid->setManufacturer("Desk-Mate");
    media_hid->setPnp(0x02, 0x303A, 0x4001, 0x0200);
    media_hid->setHidInfo(0x00, 0x02);
    media_hid->setReportMap(report, sizeof(report));
    uint8_t released[2] = {0, 0};
    media_input->setValue(released, sizeof(released));
    s->start();
    NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();
    adv->setAppearance(GENERIC_HID);
    adv->addServiceUUID(media_hid->getHidService()->getUUID());
    adv->setName("Desk-Mate");
    adv->enableScanResponse(true);
    adv->start();
    Serial.println("BLE: Desk-Mate HID advertising");
}
static bool media_send(uint16_t usage) {
    if (!media_ready() || media_key_down) return false;
    uint8_t report[2] = {(uint8_t)usage, (uint8_t)(usage >> 8)};
    media_input->setValue(report, sizeof(report));
    if (!media_input->notify()) return false;
    media_release_at = millis() + 50;
    media_key_down = true;
    Serial.printf("HID: usage=0x%04x\n", usage);
    return true;
}
static void media_poll(void) {
    if (media_key_down && (!media_ready() || (int32_t)(millis() - media_release_at) >= 0)) {
        uint8_t report[2] = {0, 0};
        media_input->setValue(report, sizeof(report));
        if (media_ready() && !media_input->notify()) return; /* retry release */
        media_key_down = false;
    }
}
