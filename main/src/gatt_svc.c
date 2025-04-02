/* Includes */
#include "gatt_svc.h"
#include "ble_handler.h"
#include "cJSON.h"
#include "common.h"
#include "ctype.h"
#include "device_id.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "firmware_data.h"
#include "gap.h"
#include "gecl-nvs-manager.h"
#include "host/ble_hs.h"
#include "host/ble_sm.h"
#include "mbedtls/base64.h"
#include "mbedtls/sha256.h"
#include "secure_connections.h"
#include "utils.h"
#include "wifi_handler.h"

const char *TAG = "GATT_SVC";

/* Private Variables */
static char *cached_base64_ssid_json = NULL;
static char *cached_signature = NULL;
static char *cached_device_id = NULL;
static char *cached_hash = NULL;
static char mqtt_url[MQTT_URL_MAX_LENGTH] = {0};
static uint16_t notify_status_handle; // Stores the handle for notifications
static uint8_t *certificate_buffer = NULL;
static size_t certificate_length = 0;
static size_t expected_length = 0; // Holds expected total size from first 2 bytes
cJSON *json_root = NULL;
char iot_device_name[IOT_DEVICE_NAME_MAX_LEN + 1] = {0}; // Global storage

/* Callback Prototypes */
static int gatt_svc_r_device_type_cb(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt,
                                     void *arg);
static int gatt_svc_r_hash_cb(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg);
static int gatt_svc_r_signature_cb(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt,
                                   void *arg);
static int gatt_svc_rw_mobile_ack_cb(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt,
                                     void *arg);
static int gatt_svc_r_device_id_cb(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt,
                                   void *arg);
static int gatt_svc_r_wifi_config_cb(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt,
                                     void *arg);
static int gatt_svc_r_wifi_ssid_list_cb(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt,
                                        void *arg);
static int gatt_svc_rw_ota_certificate_cb(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt,
                                          void *arg);
static int gatt_svc_r_mqtt_url_cb(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt,
                                  void *arg);
static int gatt_svc_r_device_mqtt_status_cb(uint16_t conn_handle, uint16_t attr_handle,
                                            struct ble_gatt_access_ctxt *ctxt, void *arg);
static int gatt_svc_w_iot_device_name_cb(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt,
                                         void *arg);
// Private Functions

// GATT service definition
static const struct ble_gatt_svc_def gatt_svr_svc[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &SERVICE_UUID.u,
        .characteristics =
            (struct ble_gatt_chr_def[]){
                {
                    .uuid = &READ_DEVICE_TYPE_UUID.u,
                    .access_cb = gatt_svc_r_device_type_cb,
                    .flags = BLE_GATT_CHR_F_READ,
                },
                {
                    .uuid = &READ_HASH_UUID.u,
                    .access_cb = gatt_svc_r_hash_cb,
                    .flags = BLE_GATT_CHR_F_READ,
                },
                {
                    .uuid = &READ_SIGNATURE_UUID.u,
                    .access_cb = gatt_svc_r_signature_cb,
                    .flags = BLE_GATT_CHR_F_READ,
                },
                {
                    .uuid = &WRITE_ACK_UUID.u,
                    .access_cb = gatt_svc_rw_mobile_ack_cb,
                    .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
                },
                {
                    .uuid = &READ_DEVICE_ID_UUID.u,
                    .access_cb = gatt_svc_r_device_id_cb,
                    .flags = BLE_GATT_CHR_F_READ,
                },
                {
                    .uuid = &WRITE_WIFI_CONFIG_UUID.u,
                    .access_cb = gatt_svc_r_wifi_config_cb,
                    .flags = BLE_GATT_CHR_F_WRITE,
                },
                {
                    .uuid = &READ_WIFI_SSID_LIST_UUID.u,
                    .access_cb = gatt_svc_r_wifi_ssid_list_cb,
                    .flags = BLE_GATT_CHR_F_READ,
                },
                {
                    .uuid = &WRITE_OTA_CERTIFICATE_UUID.u,
                    .access_cb = gatt_svc_rw_ota_certificate_cb,
                    .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_RELIABLE_WRITE,
                },
                {
                    .uuid = &WRITE_MQTT_URL_UUID.u,
                    .access_cb = gatt_svc_r_mqtt_url_cb,
                    .flags = BLE_GATT_CHR_F_WRITE,
                },
                {
                    .uuid = &READ_DEVICE_MQTT_STATUS_UUID.u,
                    .access_cb = gatt_svc_r_device_mqtt_status_cb,
                    .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                    .val_handle = &notify_status_handle,
                },
                {
                    .uuid = &WRITE_IOT_DEVICE_NAME_UUID.u,
                    .access_cb = gatt_svc_w_iot_device_name_cb,
                    .flags = BLE_GATT_CHR_F_WRITE,
                },
                {0}, // End of characteristics
            },
    },
    {0}, // End of services
};

static int gatt_svc_w_iot_device_name_cb(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt,
                                         void *arg) {
    ESP_LOGI(TAG, "IoT device name write request received: conn_handle=%d", conn_handle);

    // Check if data exists
    if (ctxt->om == NULL || ctxt->om->om_len == 0) {
        ESP_LOGE(TAG, "Received empty device name");
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }

    ESP_LOGI(TAG, "Received data of length: %d", ctxt->om->om_len);
    ESP_LOG_BUFFER_HEXDUMP(TAG, ctxt->om->om_data, ctxt->om->om_len, ESP_LOG_INFO);

    // Ensure name length does not exceed max
    size_t name_len = ctxt->om->om_len;
    if (name_len > IOT_DEVICE_NAME_MAX_LEN) {
        name_len = IOT_DEVICE_NAME_MAX_LEN; // Truncate if too long
    }

    // Copy received name to global variable
    memset(iot_device_name, 0, sizeof(iot_device_name)); // Clear old name
    memcpy(iot_device_name, ctxt->om->om_data, name_len);
    iot_device_name[name_len] = '\0'; // Ensure null termination

    save_to_nvs("iot_device_name", iot_device_name);

    // Log the stored name
    ESP_LOGI(TAG, "Iot device name stored to NVS: %s", iot_device_name);

    return 0; // Return success
}

static int gatt_svc_r_device_mqtt_status_cb(uint16_t conn_handle, uint16_t attr_handle,
                                            struct ble_gatt_access_ctxt *ctxt, void *arg) {
    ESP_LOGI(TAG, "Device MQTT status characteristic read request received: conn_handle=%d", conn_handle);
    const char *status = "MQTT Ready"; // Example status message
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        // Handle read request
        return os_mbuf_append(ctxt->om, status, strlen(status));
    }
    return 0;
}

static int gatt_svc_r_mqtt_url_cb(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt,
                                  void *arg) {
    ESP_LOGI(TAG, "MQTT URL characteristic write request received: conn_handle=%d", conn_handle);
    // Ensure data length is within bounds
    if (ctxt->om->om_len >= MQTT_URL_MAX_LENGTH) {
        ESP_LOGE(TAG, "MQTT URL too long");
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }

    // Copy the MQTT URL into the buffer
    memset(mqtt_url, 0, sizeof(mqtt_url));
    memcpy(mqtt_url, ctxt->om->om_data, ctxt->om->om_len);

    // Null-terminate the string
    mqtt_url[ctxt->om->om_len] = '\0';

    ESP_LOGI(TAG, "MQTT URL updated: %s", mqtt_url);

    if (save_to_nvs("mqtt_url", mqtt_url) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save MQTT URL to NVS");
        return BLE_ATT_ERR_UNLIKELY;
    }

    return 0; // Success
}

static int gatt_svc_rw_ota_certificate_cb(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt,
                                          void *arg) {
    if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    size_t len = OS_MBUF_PKTLEN(ctxt->om);

    if (certificate_buffer == NULL) {
        // Ensure at least 2 bytes for expected length
        if (len < 2) {
            ESP_LOGE(TAG, "Insufficient data for length header.");
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }

        // Extract expected length from first two bytes (Little-Endian)
        uint16_t received_length;
        os_mbuf_copydata(ctxt->om, 0, 2, &received_length);
        expected_length = received_length;

        if (expected_length > CERTIFICATE_MAX_LENGTH) {
            ESP_LOGE(TAG, "Expected length exceeds max buffer size.");
            return BLE_ATT_ERR_INSUFFICIENT_RES;
        }

        // Allocate memory
        certificate_buffer = (uint8_t *)calloc(expected_length, sizeof(uint8_t));
        if (!certificate_buffer) {
            ESP_LOGE(TAG, "Memory allocation failed.");
            return BLE_ATT_ERR_INSUFFICIENT_RES;
        }

        certificate_length = 0; // Reset received length
        ESP_LOGI(TAG, "OTA certificate handler invoked. Expecting %d bytes of binary data.", expected_length);
        return 0;
    }

    // Get MTU dynamically
    uint16_t mtu = ble_att_mtu(conn_handle) - 3; // Subtract 3 for ATT overhead
    size_t total_chunks = (expected_length + mtu - 1) / mtu;
    size_t chunk_number = (certificate_length / mtu) + 1; // Current chunk being processed

    ESP_LOGI(TAG, "Receiving chunk %d of %d (MTU: %d)", chunk_number, total_chunks, mtu);

    size_t received_data_len = certificate_length + len;

    ESP_LOGI(TAG, "Received data length: %d", received_data_len);

    if (received_data_len > expected_length) {
        ESP_LOGE(TAG, "Buffer overflow! Received data (%d) exceeds declared size (%d).", received_data_len,
                 expected_length);
        free(certificate_buffer);
        certificate_buffer = NULL;
        certificate_length = 0;
        expected_length = 0;
        return BLE_ATT_ERR_INSUFFICIENT_RES;
    }

    // Copy chunk data into buffer
    os_mbuf_copydata(ctxt->om, 0, len, &certificate_buffer[certificate_length]);
    certificate_length += len;

    if (received_data_len == expected_length) {
        ESP_LOGI(TAG, "Full binary payload of %d bytes received! Processing...", expected_length);

        ESP_LOG_BUFFER_HEXDUMP(TAG, certificate_buffer, expected_length, ESP_LOG_INFO);

        json_root = cJSON_Parse((char *)certificate_buffer);
        if (!json_root) {
            ESP_LOGE(TAG, "JSON parsing failed.");
            goto cleanup;
        }

        const char *certificate = cJSON_GetObjectItem(json_root, "certificatePem")
                                      ? cJSON_GetObjectItem(json_root, "certificatePem")->valuestring
                                      : NULL;
        if (!certificate) {
            ESP_LOGE(TAG, "Invalid JSON structure: certificate not found");
            goto cleanup;
        }

        const char *cert_id = cJSON_GetObjectItem(json_root, "certificateId")
                                  ? cJSON_GetObjectItem(json_root, "certificateId")->valuestring
                                  : NULL;
        if (!cert_id) {
            ESP_LOGE(TAG, "Invalid JSON structure: cert_id not found");
            goto cleanup;
        }

        const char *cert_arn = cJSON_GetObjectItem(json_root, "certificateArn")
                                   ? cJSON_GetObjectItem(json_root, "certificateArn")->valuestring
                                   : NULL;
        if (!cert_arn) {
            ESP_LOGE(TAG, "Invalid JSON structure: cert_arn not found");
            goto cleanup;
        }

        const char *root_ca =
            cJSON_GetObjectItem(json_root, "rootCa") ? cJSON_GetObjectItem(json_root, "rootCa")->valuestring : NULL;
        if (!root_ca) {
            ESP_LOGE(TAG, "Invalid JSON structure: root_ca not found");
            goto cleanup;
        }

        // Get the 'keypair' object
        cJSON *keypair = cJSON_GetObjectItem(json_root, "keypair");
        if (!keypair) {
            ESP_LOGE(TAG, "Invalid JSON structure: keypair not found");
            goto cleanup;
        }

        // Get 'PrivateKey' from within 'keypair'
        const char *private_key =
            cJSON_GetObjectItem(keypair, "PrivateKey") ? cJSON_GetObjectItem(keypair, "PrivateKey")->valuestring : NULL;
        if (!private_key) {
            ESP_LOGE(TAG, "Invalid JSON structure: PrivateKey not found in keypair");
            goto cleanup;
        }

        ESP_LOGI(TAG, "Certificate ID: %s", cert_id);
        ESP_LOGI(TAG, "Certificate ARN: %s", cert_arn);
        ESP_LOGI(TAG, "Private Key: %s", private_key); // Optional: Log for debugging

        // Save data to NVS
        if (save_to_nvs("p1_cert", certificate) != ESP_OK || save_to_nvs("p1_certId", cert_id) != ESP_OK ||
            save_to_nvs("p1_certArn", cert_arn) != ESP_OK || save_to_nvs("p1_rootCa", root_ca) != ESP_OK ||
            save_to_nvs("p1_key", private_key) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to save data to NVS.");
            goto cleanup;
        }

        ESP_LOGI(TAG, "Phase one certificate data saved successfully.");

    cleanup:
        if (json_root)
            cJSON_Delete(json_root);
        if (certificate_buffer)
            free(certificate_buffer);
        certificate_buffer = NULL;
        certificate_length = 0;
        expected_length = 0;
    }

    return 0; // Success
}

static int gatt_svc_r_wifi_ssid_list_cb(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt,
                                        void *arg) {

    ESP_LOGI(TAG, "WiFi SSID list characteristic read request received");

    if (cached_base64_ssid_json == NULL) {
        char *json_string = create_ssid_json();
        if (!json_string) {
            ESP_LOGE(TAG, "Failed to create JSON string");
            return BLE_ATT_ERR_UNLIKELY;
        }

        ESP_LOGI(TAG, "SSID List:\n%s", json_string);

        cached_base64_ssid_json = base64_encode_json(json_string);
        free(json_string); // Free JSON string memory
        if (!cached_base64_ssid_json) {
            ESP_LOGE(TAG, "Failed to encode JSON to Base64");
            return BLE_ATT_ERR_UNLIKELY;
        }
    }

    return os_mbuf_append(ctxt->om, cached_base64_ssid_json, (uint16_t)strlen(cached_base64_ssid_json));
}

static int gatt_svc_r_wifi_config_cb(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt,
                                     void *arg) {
    ESP_LOGI(TAG, "Wi-Fi configuration characteristic read/write request received: conn_handle=%d", conn_handle);

    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        ESP_LOGI(TAG, "Encrypted Wi-Fi configuration data received");

        // Step 1: Derive session key using firmware hash
        const uint8_t *firmware_hash = get_firmware_hash();
        derive_session_key(firmware_hash, HASH_SIZE);
        ESP_LOGI(TAG, "Session key derived successfully");

        // Step 2: Read the incoming encrypted data
        size_t len = OS_MBUF_PKTLEN(ctxt->om);
        if (len < IV_SIZE) {
            ESP_LOGE(TAG, "Received data too short for IV and payload");
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }

        uint8_t encrypted_data[len];
        int rc = os_mbuf_copydata(ctxt->om, 0, len, encrypted_data);
        if (rc != 0) {
            ESP_LOGE(TAG, "Failed to copy encrypted data from mbuf");
            return BLE_ATT_ERR_UNLIKELY;
        }
        ESP_LOGI(TAG, "Encrypted data of length %d received:", len);
        ESP_LOG_BUFFER_HEXDUMP(TAG, encrypted_data, len, ESP_LOG_INFO);

        // Step 3: Extract the IV and ciphertext
        uint8_t iv[IV_SIZE];
        memcpy(iv, encrypted_data, IV_SIZE); // First 16 bytes are the IV
        uint8_t ciphertext[len - IV_SIZE];
        memcpy(ciphertext, encrypted_data + IV_SIZE, len - IV_SIZE);

        // Step 4: Decrypt the ciphertext
        uint8_t plaintext[len - IV_SIZE];
        rc = decrypt_message(ciphertext, len - IV_SIZE, iv, plaintext);
        if (rc != 0) {
            ESP_LOGE(TAG, "Decryption failed");
            return BLE_ATT_ERR_UNLIKELY;
        }

        ESP_LOGI(TAG, "Decrypted Wi-Fi credentials received");
        ESP_LOG_BUFFER_HEXDUMP(TAG, plaintext, len - IV_SIZE, ESP_LOG_INFO);

        // Step 5: Parse JSON payload
        cJSON *json = cJSON_ParseWithLength((const char *)plaintext, len - IV_SIZE);
        if (json == NULL) {
            ESP_LOGE(TAG, "Failed to parse JSON payload");
            return BLE_ATT_ERR_UNLIKELY;
        }

        const cJSON *ssid_item = cJSON_GetObjectItem(json, "ssid");
        const cJSON *password_item = cJSON_GetObjectItem(json, "password");

        if (!cJSON_IsString(ssid_item) || !cJSON_IsString(password_item)) {
            ESP_LOGE(TAG, "Invalid JSON format: Missing or invalid 'ssid' or 'password'");
            cJSON_Delete(json);
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }

        const char *ssid = ssid_item->valuestring;
        const char *password = password_item->valuestring;

        ESP_LOGI(TAG, "SSID: %s, Password: %s", ssid, password);

        // Step 6: Call wifi_connect_to_ssid() with the credentials
        esp_err_t ret = wifi_connect_to_ssid(ssid, password);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to connect to Wi-Fi: %s", esp_err_to_name(ret));
        }

        cJSON_Delete(json); // Free JSON object memory

        ESP_LOGI(TAG, "Wi-Fi connection initiated. Awaiting IP...");
        return 0; // Success
    }

    ESP_LOGE(TAG, "Invalid operation: %d", ctxt->op);
    return BLE_ATT_ERR_UNLIKELY;
}

static int gatt_svc_r_device_id_cb(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt,
                                   void *arg) {
    ESP_LOGI(TAG, "Device ID characteristic read request received");

    if (cached_device_id == NULL) {
        ESP_LOGI(TAG, "Device ID characteristic read request received");
        create_device_id();
        cached_device_id = get_device_id();
    }

    return os_mbuf_append(ctxt->om, cached_device_id, MAC_ADDRESS_SIZE);
}

static int gatt_svc_rw_mobile_ack_cb(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt,
                                     void *arg) {
    ESP_LOGI(TAG, "Mobile ACK characteristic read/write request received: conn_handle=%d", conn_handle);

    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        ESP_LOGI(TAG, "Encrypted data received from mobile app");

        // Step 1: Derive session key using firmware hash
        const uint8_t *firmware_hash = get_firmware_hash();
        derive_session_key(firmware_hash, HASH_SIZE);
        ESP_LOGI(TAG, "Session key derived successfully");

        // Step 2: Read the incoming encrypted data
        size_t len = OS_MBUF_PKTLEN(ctxt->om);
        if (len < IV_SIZE) {
            ESP_LOGE(TAG, "Received data too short for IV and payload");
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }

        uint8_t encrypted_data[len];
        int rc = os_mbuf_copydata(ctxt->om, 0, len, encrypted_data);
        if (rc != 0) {
            ESP_LOGE(TAG, "Failed to copy encrypted data from mbuf");
            return BLE_ATT_ERR_UNLIKELY;
        }
        ESP_LOGI(TAG, "Encrypted data of length %d received from mobile app:", len);
        ESP_LOG_BUFFER_HEXDUMP(TAG, encrypted_data, len, ESP_LOG_INFO);

        // Step 3: Extract the IV and ciphertext
        uint8_t iv[IV_SIZE];
        memcpy(iv, encrypted_data, IV_SIZE); // First 16 bytes are the IV
        uint8_t ciphertext[len - IV_SIZE];
        memcpy(ciphertext, encrypted_data + IV_SIZE, len - IV_SIZE);

        // Step 4: Decrypt the ciphertext
        uint8_t plaintext[len - IV_SIZE];
        rc = decrypt_message(ciphertext, len - IV_SIZE, iv, plaintext);
        if (rc != 0) {
            ESP_LOGE(TAG, "Decryption failed");
            return BLE_ATT_ERR_UNLIKELY;
        }

        ESP_LOGI(TAG, "Decrypted message: %.*s", len - IV_SIZE, plaintext);

        // Step 5: Validate the decrypted message
        const char expected_ack[] = "ACK";
        if (memcmp(plaintext, expected_ack, sizeof(expected_ack) - 1) == 0) {
            ESP_LOGI(TAG, "ACK: Valid message received from mobile app");
        } else {
            ESP_LOGE(TAG, "ACK: Invalid message received");
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }

        return 0; // Success
    }

    ESP_LOGE(TAG, "Invalid operation: %d", ctxt->op);
    return BLE_ATT_ERR_UNLIKELY;
}

static int gatt_svc_r_device_type_cb(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt,
                                     void *arg) {
    ESP_LOGI(TAG, "Device type query");
    const uint8_t device_type = get_firmware_device_type();
    return os_mbuf_append(ctxt->om, &device_type, 1);
}

static int gatt_svc_r_hash_cb(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt,
                              void *arg) {

    ESP_LOGI(TAG, "Hash characteristic read request received: conn_handle=%d", conn_handle);

    if (cached_hash == NULL) {
        // Get the firmware hash
        cached_hash = get_firmware_hash();
        ESP_LOGI(TAG, "Firmware hash:");
        ESP_LOG_BUFFER_HEXDUMP(TAG, cached_hash, HASH_SIZE, ESP_LOG_INFO);
    }

    return os_mbuf_append(ctxt->om, cached_hash, HASH_SIZE);
}

static int gatt_svc_r_signature_cb(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt,
                                   void *arg) {
    ESP_LOGI(TAG, "Signature characteristic read request received: conn_handle=%d", conn_handle);
    if (ctxt->op != BLE_GATT_ACCESS_OP_READ_CHR) {
        ESP_LOGE(TAG, "Invalid operation: %d", ctxt->op);
        return BLE_ATT_ERR_READ_NOT_PERMITTED;
    }

    ESP_LOGI(TAG, "Signature characteristic read request received: conn_handle=%d", conn_handle);
    if (cached_signature == NULL) {
        cached_signature = get_firmware_signature();
        ESP_LOGI(TAG, "Firmware signature retrieved successfully");
    }

    return os_mbuf_append(ctxt->om, cached_signature, SIG_SIZE);
}

void notify_mqtt_status() {
    const char *status = "MQTT Connected";
    size_t status_len = strlen(status);

    uint16_t conn_handle = gap_get_conn_handle();

    // Validate the characteristic handle
    if (notify_status_handle == 0) {
        ESP_LOGE(TAG, "MQTT status handle not available!");
        return;
    }

    // Validate the connection handle
    if (ble_gap_conn_find(conn_handle, NULL) != 0) {
        ESP_LOGE(TAG, "Invalid BLE connection handle: %d", conn_handle);
        return;
    }

    // Ensure the payload size does not exceed MTU limits
    uint16_t mtu = ble_att_mtu(conn_handle);
    if (status_len > (mtu - 3)) { // Subtract 3 bytes for GATT notification overhead
        ESP_LOGE(TAG, "Payload exceeds MTU size. Max: %d, Actual: %zu", mtu - 3, status_len);
        return;
    }

    // Allocate buffer for BLE notification
    struct os_mbuf *om = ble_hs_mbuf_from_flat(status, status_len);
    if (!om) {
        ESP_LOGE(TAG, "Failed to allocate BLE buffer for notification");
        return;
    }

    // Send the notification
    int rc = ble_gatts_notify_custom(conn_handle, notify_status_handle, om);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to send notification, rc=%d", rc);
        os_mbuf_free_chain(om);
    } else {
        ESP_LOGI(TAG, "Notification sent: %s", status);
    }
}

const char *get_iot_device_name(void) { return iot_device_name; }

/* Public Functions */
esp_err_t gatt_svc_init(void) {

    // Load firmware data once
    load_firmware_data();

    ESP_LOGI(TAG, "Free heap: %lu bytes before svcs config", esp_get_free_heap_size());

    // Log full UUIDs
    print_uuids();

    // Add GATT services
    int rc = ble_gatts_count_cfg(gatt_svr_svc);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to count GATT services: %d", rc);
        return rc;
    }

    rc = ble_gatts_add_svcs(gatt_svr_svc);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to add GATT services: %d", rc);
        return rc;
    }

    ESP_LOGI(TAG, "GATT services initialized successfully.");
    return 0;
}