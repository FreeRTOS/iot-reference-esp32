/* Includes */
#include "gap.h"
#include "common.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "gatt_svc.h"
#include "host/ble_gap.h"
#include "utils.h"

/* Private variables */
static uint8_t own_addr_type;
static uint16_t ble_conn_handle = BLE_HS_CONN_HANDLE_NONE; // Static variable to store connection handle

static const char *TAG = "GAP";

/* Private functions */
uint16_t gap_get_conn_handle(void) {
    if (ble_conn_handle != BLE_HS_CONN_HANDLE_NONE) {
        ESP_LOGI(TAG, "Active BLE connection handle: %d\n", ble_conn_handle);
        return ble_conn_handle;
    } else {
        ESP_LOGW(TAG, "No active BLE connection\n");
        return 0; // Return 0 if no connection is active
    }
}

void gap_security_init(uint16_t conn_handle) {
    int rc;

    // Initiate security for the connection
    rc = ble_gap_security_initiate(conn_handle);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to initiate security: %d", rc);
    } else {
        ESP_LOGI(TAG, "Security initiated for conn_handle: %d", conn_handle);
    }
}

inline static void format_addr(char *addr_str, uint8_t addr[]) {
    sprintf(addr_str, "%02X:%02X:%02X:%02X:%02X:%02X", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
}

static void print_conn_desc(struct ble_gap_conn_desc *desc) {
    char addr_str[18] = {0};
    ESP_LOGI(TAG, "connection handle: %d", desc->conn_handle);
    format_addr(addr_str, desc->our_id_addr.val);
    ESP_LOGI(TAG, "device id address: type=%d, value=%s", desc->our_id_addr.type, addr_str);
    format_addr(addr_str, desc->peer_id_addr.val);
    ESP_LOGI(TAG, "peer id address: type=%d, value=%s", desc->peer_id_addr.type, addr_str);
    ESP_LOGI(TAG, "conn_itvl=%d, conn_latency=%d, supervision_timeout=%d, encrypted=%d, authenticated=%d, bonded=%d",
             desc->conn_itvl, desc->conn_latency, desc->supervision_timeout, desc->sec_state.encrypted,
             desc->sec_state.authenticated, desc->sec_state.bonded);
}

/* Event Handler */
static int gap_event_handler(struct ble_gap_event *event, void *arg) {

    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            ble_conn_handle = event->connect.conn_handle; // Set the connection handle
            ESP_LOGW(TAG, "BLE client connected, handle: %d\n", ble_conn_handle);
        } else {
            ESP_LOGE(TAG, "BLE connection failed\n");
        }
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGE(TAG, "Device disconnected. Reason: %s Handle: %d",
                 lookup_ble_disconnection_reason(event->disconnect.reason), ble_conn_handle);
        ble_conn_handle = BLE_HS_CONN_HANDLE_NONE; // Reset the handle
        break;

    case BLE_GAP_EVENT_CONN_UPDATE:
        ESP_LOGI(TAG, "Connection update complete for handle: %d, status: %d", event->conn_update.conn_handle,
                 event->conn_update.status);
        break;

    case BLE_GAP_EVENT_CONN_UPDATE_REQ:
        ESP_LOGI(TAG, "Connection update request received for handle: %d", event->conn_update_req.conn_handle);
        return 0; // Accept the update
        break;

    case BLE_GAP_EVENT_ENC_CHANGE:
        ESP_LOGI(TAG, "Encryption change event: status=%d", event->enc_change.status);
        break;

    case BLE_GAP_EVENT_SUBSCRIBE:
        ESP_LOGI(TAG, "Subscribe event: conn_handle=%d value_handle=%d reason=%d", event->subscribe.conn_handle,
                 event->subscribe.attr_handle, event->subscribe.reason);
        break;

    case BLE_GAP_EVENT_NOTIFY_TX:
        ESP_LOGI(TAG, "Notification sent: conn_handle=%d attr_handle=%d status=%d", event->notify_tx.conn_handle,
                 event->notify_tx.attr_handle, event->notify_tx.status);
        break;

    case BLE_GAP_EVENT_DATA_LEN_CHG:
        ESP_LOGI(TAG, "Data length change: conn_handle=%d max_tx_octets=%d max_rx_octets=%d",
                 event->data_len_chg.conn_handle, event->data_len_chg.max_tx_octets, event->data_len_chg.max_rx_octets);
        break;

    case BLE_GAP_EVENT_LINK_ESTAB:
        ESP_LOGI(TAG, "Link established: conn_handle=%d", event->link_estab.conn_handle);
        break;

    case BLE_GAP_EVENT_MTU:
        ESP_LOGI(TAG, "MTU updated: conn_handle=%d cid=%d mtu=%d", event->mtu.conn_handle, event->mtu.channel_id,
                 event->mtu.value);
        break;

    case BLE_GAP_EVENT_PARING_COMPLETE:
        ESP_LOGI(TAG, "Pairing complete: conn_handle=%d status=%d", event->pairing_complete.conn_handle,
                 event->pairing_complete.status);
        break;

    default:
        ESP_LOGW(TAG, "Unhandled event type: %d", event->type);
        break;
    }
    return 0;
}
void start_advertising(void) {
    struct ble_gap_adv_params adv_params;
    memset(&adv_params, 0, sizeof(adv_params));

    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND; // Undirected connectable
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN; // General discoverable

    int rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER, &adv_params, gap_event_handler, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Error starting advertisement; rc=%d", rc);
        return;
    }

    ESP_LOGI(TAG, "Advertising started successfully.");
}

void adv_init(void) {
    // Ensure the BLE address is set
    ble_hs_util_ensure_addr(0);
    int rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "Error inferring BLE address; rc=%d", rc);
        return;
    }

    // Copy the address for logging purposes
    uint8_t addr_val[6];
    ble_hs_id_copy_addr(own_addr_type, addr_val, NULL);
    ESP_LOGI(TAG, "BLE address: %02X:%02X:%02X:%02X:%02X:%02X", addr_val[5], addr_val[4], addr_val[3], addr_val[2],
             addr_val[1], addr_val[0]);

    // Configure advertisement data
    struct ble_hs_adv_fields fields;
    memset(&fields, 0, sizeof(fields));

    // Include the 128-bit service UUID in the advertisement
    static ble_uuid128_t service_uuid;
    memcpy(service_uuid.value, &SERVICE_UUID.u, sizeof(&SERVICE_UUID.u));

    fields.uuids128 = &service_uuid;
    fields.num_uuids128 = 1;
    fields.uuids128_is_complete = 1;

    // Include the device name from common.h
    fields.name = (uint8_t *)DEVICE_NAME;
    fields.name_len = strlen(DEVICE_NAME);

    // If the total advertisement size exceeds 31 bytes, truncate the name
    if ((fields.name_len + 2 + 16) > 31) { // 2 bytes for type/length + 16 bytes for UUID
        fields.name_len = 31 - (2 + 16);   // Adjust name length to fit within the limit
        fields.name_is_complete = 0;       // Mark as a shortened name
    } else {
        fields.name_is_complete = 1; // Mark as a complete name
    }

    // Set the advertisement data
    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "Error setting advertisement data; rc=%d", rc);
        return;
    }

    ESP_LOGI(TAG, "Advertisement data set successfully.");

    // Start advertising
    start_advertising();
}

int gap_init(void) {
    // Initialize GAP service
    ble_svc_gap_init();
    ble_svc_gap_device_name_set(DEVICE_NAME);

    return 0;
}
