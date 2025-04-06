#ifndef GATT_SVR_H
#define GATT_SVR_H

/* Includes */
/* NimBLE GATT APIs */
#include "host/ble_gatt.h"
#include "services/gatt/ble_svc_gatt.h"

/* NimBLE GAP APIs */
#include "host/ble_gap.h"

#define CERTIFICATE_MAX_LENGTH 8192
#define DELIMITER_CHAR '\0' // Null terminator as the delimiter
#define MQTT_URL_MAX_LENGTH 256
#define IOT_DEVICE_NAME_MAX_LEN 31 // 31 chars + 1 for null termination
#define IOT_DEVICE_NAME_MIN_LEN 20 // No way it could be less than this

/* Public function declarations */
void gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg);
void gatt_svr_subscribe_cb(struct ble_gap_event *event);
void register_mtu_callback();
esp_err_t gatt_svc_init(void);
void notify_mqtt_status(void);
const char *get_thing_name(void);
#endif // GATT_SVR_H
