#include "bt_scanner.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_bt_main.h"
#include "esp_log.h"
#include <string.h>
#include <math.h>

static const char *TAG = "BLE";

#define RSSI_THRESHOLD  -60
#define TX_POWER_NONE   127

// ── Parsed advertisement info ─────────────────────────────────────────────────

typedef struct {
    char   flags[64];
    char   data[256];
    char   device[64];
    char   screen[8];
    char   locked[8];
    int8_t tx_power;
} adv_info_t;

static void buf_append(char *buf, size_t size, const char *s) {
    if (buf[0]) strncat(buf, ", ", size - strlen(buf) - 1);
    strncat(buf, s, size - strlen(buf) - 1);
}

// ── Service UUID descriptions ─────────────────────────────────────────────────

static const char *service_uuid_name(uint16_t uuid) {
    switch (uuid) {
        case 0x1800: return "Generic Access";
        case 0x1801: return "Generic Attribute";
        case 0x180A: return "Device Information";
        case 0x180D: return "Heart Rate Monitor";
        case 0x180F: return "Battery";
        case 0x1812: return "HID (keyboard / mouse / gamepad)";
        case 0x1816: return "Cycling Speed & Cadence";
        case 0x1818: return "Cycling Power";
        case 0x181A: return "Environmental Sensing";
        case 0xFE9F: return "Google Fast Pair";
        case 0xFD5A: return "Google Nearby Share";
        case 0xFEAA: return "Eddystone Beacon";
        case 0xFEBE: return "Tile Tracker";
        case 0xFD6F: return "COVID-19 Exposure Notification";
        case 0xFE95: return "Xiaomi";
        case 0xFE8A: return "Apple ANCS";
        default:     return NULL;
    }
}

// ── Apple manufacturer-specific subtypes ─────────────────────────────────────

static const char *apple_subtype_name(uint8_t type) {
    switch (type) {
        case 0x02: return "iBeacon";
        case 0x05: return "AirDrop";
        case 0x07: return "AirPods";
        case 0x09: return "AirPlay Target";
        case 0x0A: return "AirPlay Source";
        case 0x0C: return "Handoff (iPhone / Mac continuity)";
        case 0x0F: return "Nearby Action (AirDrop / Siri)";
        case 0x10: return "Nearby Info (iPhone status)";
        case 0x12: return "Find My";
        case 0x15: return "Proximity Pairing (AirPods)";
        case 0x1E: return "HomeKit";
        default:   return "Apple (unknown subtype)";
    }
}

// ── Advertising payload parser ────────────────────────────────────────────────

static void parse_adv_payload(const uint8_t *data, uint8_t len, adv_info_t *out) {
    if (len == 0) return;

    uint8_t i = 0;
    while (i < len) {
        uint8_t ad_len = data[i];
        if (ad_len == 0 || (i + ad_len) >= len) break;

        uint8_t        ad_type = data[i + 1];
        const uint8_t *val     = &data[i + 2];
        uint8_t        val_len = ad_len - 1;
        char           tmp[64];

        switch (ad_type) {

            case 0x01:
                if (val_len >= 1) {
                    uint8_t f = val[0];
                    snprintf(out->flags, sizeof(out->flags), "0x%02x%s%s%s", f,
                        (f & 0x02) ? " [General Discoverable]" : "",
                        (f & 0x01) ? " [Limited Discoverable]" : "",
                        (f & 0x04) ? " [BR/EDR Not Supported]"  : "");
                }
                break;

            case 0x08:
            case 0x09:
                if (!out->device[0]) {
                    size_t n = val_len < sizeof(out->device) - 1 ? val_len : sizeof(out->device) - 1;
                    memcpy(out->device, val, n);
                    out->device[n] = '\0';
                }
                break;

            case 0x0A:
                out->tx_power = (int8_t)val[0];
                break;

            case 0x02:
            case 0x03:
                for (int j = 0; j + 1 < val_len; j += 2) {
                    uint16_t uuid = val[j] | (val[j + 1] << 8);
                    const char *name = service_uuid_name(uuid);
                    if (name)
                        snprintf(tmp, sizeof(tmp), "0x%04x (%s)", uuid, name);
                    else
                        snprintf(tmp, sizeof(tmp), "0x%04x", uuid);
                    buf_append(out->data, sizeof(out->data), tmp);
                }
                break;

            case 0x06:
            case 0x07:
                if (val_len >= 16) {
                    snprintf(tmp, sizeof(tmp),
                        "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                        val[15],val[14],val[13],val[12],
                        val[11],val[10],val[9], val[8],
                        val[7], val[6], val[5], val[4],
                        val[3], val[2], val[1], val[0]);
                    buf_append(out->data, sizeof(out->data), tmp);
                }
                break;

            case 0x16:
                if (val_len >= 2) {
                    uint16_t uuid = val[0] | (val[1] << 8);
                    const char *name = service_uuid_name(uuid);
                    if (name)
                        snprintf(tmp, sizeof(tmp), "SvcData:0x%04x (%s)", uuid, name);
                    else
                        snprintf(tmp, sizeof(tmp), "SvcData:0x%04x", uuid);
                    buf_append(out->data, sizeof(out->data), tmp);
                }
                break;

            case 0x19:
                if (val_len >= 2 && !out->device[0]) {
                    uint16_t app = val[0] | (val[1] << 8);
                    const char *desc =
                        app == 0x0040 ? "Phone"             :
                        app == 0x0041 ? "Computer"          :
                        app == 0x00C0 ? "Watch"             :
                        app == 0x00C1 ? "Sports Watch"      :
                        app == 0x0180 ? "Heart Rate Sensor" :
                        app == 0x0300 ? "Hearing Aid"       :
                        app == 0x0340 ? "Speaker"           :
                        app == 0x03C0 ? "Cycling Computer"  : NULL;
                    if (desc)
                        snprintf(out->device, sizeof(out->device), "%s", desc);
                }
                break;

            case 0xFF:
                if (val_len >= 2) {
                    uint16_t company = val[0] | (val[1] << 8);
                    if (company == 0x004C) {
                        if (val_len >= 3) {
                            uint8_t subtype = val[2];
                            if (!out->device[0])
                                snprintf(out->device, sizeof(out->device),
                                    "Apple: %s", apple_subtype_name(subtype));
                            if (subtype == 0x10 && val_len >= 5) {
                                uint8_t status = val[4];
                                snprintf(out->screen, sizeof(out->screen), "%s",
                                    (status & 0x40) ? "On" : "Off");
                                snprintf(out->locked, sizeof(out->locked), "%s",
                                    (status & 0x20) ? "Yes" : "No");
                            }
                        }
                    } else if (!out->device[0]) {
                        const char *vendor =
                            company == 0x00E0 ? "Google"    :
                            company == 0x0006 ? "Microsoft" :
                            company == 0x0075 ? "Samsung"   :
                            company == 0x0310 ? "Fitbit"    :
                            company == 0x0499 ? "Ruuvi"     :
                            company == 0x0059 ? "Nordic"    : "Unknown";
                        snprintf(out->device, sizeof(out->device), "%s (0x%04x)", vendor, company);
                    }
                }
                break;

            default:
                break;
        }

        i += 1 + ad_len;
    }
}

// ── Hex dump helper ───────────────────────────────────────────────────────────

static void bytes_to_hex(const uint8_t *data, uint8_t len, char *out, size_t out_size) {
    out[0] = '\0';
    for (uint8_t i = 0; i < len && (i * 3 + 3) < out_size; i++)
        snprintf(out + i * 3, 4, "%02x ", data[i]);
}

// ── GAP event handler ─────────────────────────────────────────────────────────

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    if (event == ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT) {
        ESP_ERROR_CHECK(esp_ble_gap_start_scanning(0));
        ESP_LOGI(TAG, "BLE scanning started");
        return;
    }

    if (event != ESP_GAP_BLE_SCAN_RESULT_EVT) return;
    if (param->scan_rst.search_evt != ESP_GAP_SEARCH_INQ_RES_EVT) return;

    int8_t rssi = param->scan_rst.rssi;
    if (rssi < RSSI_THRESHOLD) return;

    uint8_t *a       = param->scan_rst.bda;
    uint8_t  adv_len = param->scan_rst.adv_data_len;
    uint8_t  rsp_len = param->scan_rst.scan_rsp_len;
    uint8_t  atype   = param->scan_rst.ble_addr_type;

    adv_info_t info;
    memset(&info, 0, sizeof(info));
    info.tx_power = TX_POWER_NONE;

    parse_adv_payload(param->scan_rst.ble_adv,           adv_len, &info);
    parse_adv_payload(param->scan_rst.ble_adv + adv_len, rsp_len, &info);

    char dist_str[32] = "-";
    if (info.tx_power != TX_POWER_NONE) {
        float dist = powf(10.0f, (float)(info.tx_power - rssi) / 20.0f);
        snprintf(dist_str, sizeof(dist_str), "%.2f m", dist);
    }

    // 31 bytes max per payload → 31*3 + 1 = 94 chars each
    char hex_adv[94] = "-";
    char hex_rsp[94] = "-";
    if (adv_len > 0) bytes_to_hex(param->scan_rst.ble_adv,           adv_len, hex_adv, sizeof(hex_adv));
    if (rsp_len > 0) bytes_to_hex(param->scan_rst.ble_adv + adv_len, rsp_len, hex_rsp, sizeof(hex_rsp));

    ESP_LOGI(TAG, "---");
    ESP_LOGI(TAG, "mac:      %02x:%02x:%02x:%02x:%02x:%02x [%s]",
        a[0], a[1], a[2], a[3], a[4], a[5],
        atype == BLE_ADDR_TYPE_PUBLIC ? "Public" :
        atype == BLE_ADDR_TYPE_RANDOM ? "Random" : "Other");
    ESP_LOGI(TAG, "rssi:     %d dBm", rssi);
    ESP_LOGI(TAG, "flags:    %s", info.flags[0]  ? info.flags  : "-");
    ESP_LOGI(TAG, "data:     %s", info.data[0]   ? info.data   : "-");
    ESP_LOGI(TAG, "device:   %s", info.device[0] ? info.device : "-");
    ESP_LOGI(TAG, "screen:   %s", info.screen[0] ? info.screen : "-");
    ESP_LOGI(TAG, "locked:   %s", info.locked[0] ? info.locked : "-");
    ESP_LOGI(TAG, "distance: %s", dist_str);
    ESP_LOGI(TAG, "raw_adv:  %s", hex_adv);
    ESP_LOGI(TAG, "raw_rsp:  %s", hex_rsp);
}

// ── Public API ────────────────────────────────────────────────────────────────

void ble_scanner_init(void) {
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));
    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());
    ESP_ERROR_CHECK(esp_ble_gap_register_callback(gap_event_handler));
    ESP_LOGI(TAG, "BLE scanner ready  (RSSI >= %d dBm)", RSSI_THRESHOLD);
}

void ble_scanner_start(void) {
    esp_ble_scan_params_t scan_params = {
        .scan_type          = BLE_SCAN_TYPE_ACTIVE,
        .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
        .scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL,
        .scan_interval      = 0x50,
        .scan_window        = 0x30,
        .scan_duplicate     = BLE_SCAN_DUPLICATE_ENABLE,
    };
    ESP_ERROR_CHECK(esp_ble_gap_set_scan_params(&scan_params));
}

void ble_scanner_stop(void) {
    ESP_ERROR_CHECK(esp_ble_gap_stop_scanning());
}

uint16_t ble_scanner_get_device_count(void) { return 0; }
void ble_scanner_clean_expired(void) {}
