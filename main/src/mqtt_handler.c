#include "mqtt_handler.h"
#include "cJSON.h"
#include "device_id.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_wifi.h"
#include "gap.h"
#include "gatt_svc.h"
#include "gecl-nvs-manager.h"
#include "mbedtls/base64.h"
#include "ota_over_mqtt_demo.h"
#include "ota_state.h"
#include "provisioning_state.h"
#include "string.h"
#include "utils.h"

static const char *TAG = "MQTT_HANDLER";

static esp_mqtt_client_handle_t mqtt_client = NULL;
static esp_ota_handle_t ota_handle = 0;
static size_t ota_total_written = 0;
static size_t ota_expected_size = 0;
static bool ota_in_progress = false;
static char current_job_id[128] = {0};
static char *thing_name = NULL;              // Assuming this was already global in your code
static esp_netif_t *s_wifi_sta_netif = NULL; // Track the STA interface

esp_mqtt_client_handle_t get_mqtt_client() { return mqtt_client; }

void subscribe_to_ota_topics(esp_mqtt_client_handle_t my_client, char *thing_name) {
    if (thing_name == NULL) {
        printf("Thing name not set, cannot subscribe to OTA topics\n");
        return;
    }

    char jobs_notify_topic[MAX_TOPIC_LENGTH];
    char jobs_wildcard_topic[MAX_TOPIC_LENGTH];
    char streams_wildcard_topic[MAX_TOPIC_LENGTH];

    snprintf(jobs_notify_topic, MAX_TOPIC_LENGTH, "%s%s%s", MQTT_JOBS_NOTIFY_BASE, thing_name, MQTT_JOBS_NOTIFY_SUFFIX);
    snprintf(jobs_wildcard_topic, MAX_TOPIC_LENGTH, "%s%s%s", MQTT_JOBS_NOTIFY_BASE, thing_name,
             MQTT_JOBS_WILDCARD_SUFFIX);
    snprintf(streams_wildcard_topic, MAX_TOPIC_LENGTH, "%s%s%s", MQTT_JOBS_NOTIFY_BASE, thing_name,
             MQTT_STREAMS_WILDCARD_SUFFIX);

    esp_mqtt_client_subscribe(my_client, jobs_notify_topic, 0);
    esp_mqtt_client_subscribe(my_client, jobs_wildcard_topic, 0);
    esp_mqtt_client_subscribe(my_client, streams_wildcard_topic, 0);

    printf("Subscribed to OTA topics:\n");
    printf("  %s\n", jobs_notify_topic);
    printf("  %s\n", jobs_wildcard_topic);
    printf("  %s\n", streams_wildcard_topic);

    list_keys_in_nvs();
}

static void cleanup_and_reboot(void *arg) {
    esp_mqtt_client_handle_t client = (esp_mqtt_client_handle_t)arg;

    provisioning_complete = true;

    // Stop and destroy MQTT client only
    esp_err_t ret = esp_mqtt_client_stop(client);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to stop MQTT client: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "MQTT client stopped successfully");
    }

    ret = esp_mqtt_client_destroy(client);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to destroy MQTT client: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "MQTT client destroyed successfully");
    }
    mqtt_client = NULL;

    // Leave WiFi and esp_netif running
    ESP_LOGI(TAG, "Keeping WiFi connection active for second phase");

    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_LOGI(TAG, "Rebooting now...");
    esp_restart();
}

// MQTT Event Handler
static void bootstrap_mqtt_event_handler_cb(void *handler_args, esp_event_base_t base, int32_t event_id,
                                            void *event_data) {
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    esp_mqtt_client_handle_t client = event->client;
    static char ownership_token[1024] = {0};
    static char certificate_id[65] = {0}; // 64 chars + null terminator for CertificateId
    static char *thing_name = NULL;
    static bool cert_requested = false;
    static char response_buffer[4096] = {0}; // Buffer for full response
    static size_t response_len = 0;
    static bool provisioning_sent = false; // Track provisioning request

    switch (event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        if (key_found_in_nvs("provisioned")) {
            ESP_LOGI(TAG, "MQTT connected, setting xSuspendOta to pdFALSE");
            xSuspendOta = pdFALSE; // Resume OTA operations
            ESP_LOGI(TAG, "Device is already provisioned, skipping provisioning");
            read_from_nvs("iot_device_name", &thing_name);
            subscribe_to_ota_topics(client, thing_name);
            return;
        }
        notify_mqtt_status();

        response_len = 0; // Reset buffer on new connection
        memset(response_buffer, 0, sizeof(response_buffer));
        provisioning_sent = false;

        if (!thing_name) {
            esp_err_t err = read_from_nvs("iot_device_name", &thing_name);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to read iot_device_name from NVS");
                thing_name = strdup("coop_cop_2_7bbf803b_27dc4bfc");
            }
            ESP_LOGI(TAG, "Loaded thing_name: %s", thing_name);
        }

        if (!cert_requested) {
            int sub1 = esp_mqtt_client_subscribe(client, MQTT_CREATE_ACCEPTED_TOPIC, 0);
            int sub2 = esp_mqtt_client_subscribe(client, MQTT_CREATE_REJECTED_TOPIC, 0);
            ESP_LOGI(TAG, "✅ Subscribe accepted=%d, rejected=%d", sub1, sub2);
            vTaskDelay(pdMS_TO_TICKS(500));
            int pub = esp_mqtt_client_publish(client, MQTT_CREATE_TOPIC, "{}", 0, 0, 0);
            ESP_LOGI(TAG, "📡 Published to %s, msg_id=%d", MQTT_CREATE_TOPIC, pub);
            cert_requested = true;
        }
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        cert_requested = false;
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_mqtt_client_start(client);
        ESP_LOGI(TAG, "MQTT disconnected, setting xSuspendOta to pdTRUE");
        xSuspendOta = pdTRUE; // Suspend OTA operations
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        if (currentSubscribeSemaphore) {
            xSemaphoreGive(currentSubscribeSemaphore);
        }
        break;

    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED");
        break;

    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic ? event->topic : "NULL");
        printf("DATA=%.*s\r\n", event->data_len, event->data);

        if (key_found_in_nvs("provisioned")) {
            read_from_nvs("iot_device_name", &thing_name);
        }
        // Check if the topic is related to OTA
        if (event->topic && thing_name && strncmp(event->topic, "$aws/things/", 12) == 0) {
            ESP_LOGI(TAG, "Received OTA message on topic: %.*s", event->topic_len, event->topic);

            char ota_jobs_prefix[128];
            char ota_streams_prefix[128];
            snprintf(ota_jobs_prefix, sizeof(ota_jobs_prefix), "$aws/things/%s/jobs/", thing_name);
            snprintf(ota_streams_prefix, sizeof(ota_streams_prefix), "$aws/things/%s/streams/", thing_name);

            MQTTPublishInfo_t publishInfo = {
                .pTopicName = event->topic,
                .topicNameLength = event->topic_len,
                .pPayload = event->data,
                .payloadLength = event->data_len,
            };

            // Check if the topic matches OTA job or stream prefixes
            if (strncmp(event->topic, ota_jobs_prefix, strlen(ota_jobs_prefix)) == 0 ||
                strncmp(event->topic, ota_streams_prefix, strlen(ota_streams_prefix)) == 0) {
                if (vOTAProcessMessage(NULL, &publishInfo)) {
                    ESP_LOGI(TAG, "Message processed by OTA");
                    return; // OTA handled the message
                } else {
                    ESP_LOGI(TAG, "Message not processed by OTA");
                }
            } else {
                ESP_LOGI(TAG, "Not an OTA message, ignoring");
            }
        }

        // Handle provisioning messages
        if (event->topic && strcmp(event->topic, MQTT_PROVISION_ACCEPTED_TOPIC) == 0) {
            ESP_LOGI(TAG, "✅ Provisioning set, rebooting to use permanent certificate");
            save_to_nvs("provisioned", "true");
            xTaskCreate(cleanup_and_reboot, "cleanup_task", 4096, client, 10, NULL);
        } else if (event->topic && strcmp(event->topic, MQTT_CREATE_ACCEPTED_TOPIC) == 0) {
            // Start of new message
            response_len = 0;
            if (event->data_len >= sizeof(response_buffer)) {
                ESP_LOGE(TAG, "Data too large for buffer: %d", event->data_len);
                return;
            }
            memcpy(response_buffer, event->data, event->data_len);
            response_len = event->data_len;
        } else if (response_len > 0) {
            // Continuation of previous message
            if (response_len + event->data_len >= sizeof(response_buffer)) {
                ESP_LOGE(TAG, "Buffer overflow: %d + %d", response_len, event->data_len);
                response_len = 0; // Reset on overflow
                return;
            }
            memcpy(response_buffer + response_len, event->data, event->data_len);
            response_len += event->data_len;
        }

        if (response_len > 0 && response_buffer[response_len - 1] == '}') {
            ESP_LOGI(TAG, "✅ Received complete certificate response");
            cJSON *json = cJSON_ParseWithLength(response_buffer, response_len);
            if (!json) {
                ESP_LOGE(TAG, "Failed to parse JSON: %.*s", (int)response_len, response_buffer);
                response_len = 0;
                return;
            }

            const char *cert_pem = cJSON_GetObjectItem(json, "certificatePem")->valuestring;
            const char *private_key = cJSON_GetObjectItem(json, "privateKey")->valuestring;
            const char *token = cJSON_GetObjectItem(json, "certificateOwnershipToken")->valuestring;
            const char *cert_id = cJSON_GetObjectItem(json, "certificateId")->valuestring;

            ESP_LOGI(TAG, "Certificate ID: %s", cert_id); // Debug certificate ID

            save_to_nvs("p2_cert", cert_pem);
            save_to_nvs("p2_key", private_key);
            save_to_nvs("p2_certId", cert_id);
            strncpy(ownership_token, token, sizeof(ownership_token) - 1);
            ownership_token[sizeof(ownership_token) - 1] = '\0';
            strncpy(certificate_id, cert_id, sizeof(certificate_id) - 1);
            certificate_id[sizeof(certificate_id) - 1] = '\0';

            esp_mqtt_client_subscribe(client, MQTT_PROVISION_ACCEPTED_TOPIC, 0);
            esp_mqtt_client_subscribe(client, MQTT_PROVISION_REJECTED_TOPIC, 0);
            ESP_LOGI(TAG, "✅ Subscribed to %s and %s", MQTT_PROVISION_ACCEPTED_TOPIC, MQTT_PROVISION_REJECTED_TOPIC);

            char payload[2048];
            snprintf(
                payload, sizeof(payload),
                "{\"certificateOwnershipToken\":\"%s\",\"parameters\":{\"ThingName\":\"%s\",\"CertificateId\":\"%s\"}}",
                ownership_token, thing_name, certificate_id);
            int pub = esp_mqtt_client_publish(client, MQTT_PROVISION_TOPIC, payload, 0, 0, 0);
            ESP_LOGI(TAG, "✅ Provisioning request sent: %s", payload);
            provisioning_sent = true; // Mark provisioning attempt

            cJSON_Delete(json);
            response_len = 0; // Reset buffer
        }
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle) {
            ESP_LOGE(TAG, "Last error type: 0x%x", event->error_handle->error_type);
            ESP_LOGE(TAG, "Connect return code: 0x%x", event->error_handle->connect_return_code);
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                ESP_LOGE(TAG, "ESP-TLS error code: 0x%x", event->error_handle->esp_tls_last_esp_err);
                ESP_LOGE(TAG, "TLS stack error code: 0x%x", event->error_handle->esp_tls_stack_err);
                ESP_LOGE(TAG, "ESP-TLS error msg: %s", esp_err_to_name(event->error_handle->esp_tls_last_esp_err));
            } else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
                ESP_LOGE(TAG, "MQTT connection refused reason code: 0x%x", event->error_handle->connect_return_code);
            } else {
                ESP_LOGE(TAG, "Unknown error type: 0x%x", event->error_handle->error_type);
            }
        } else {
            ESP_LOGE(TAG, "No error handle provided");
        }
        break;

    default:
        ESP_LOGI(TAG, "Other event id: %ld", event_id);
        break;
    }
}

// Initialize MQTT Client
void init_mqtt_client() {
    char client_name[IOT_DEVICE_NAME_MAX_LEN] = {0};
    char *mqtt_url = NULL;
    char *root_ca = NULL;
    char *client_cert = NULL;
    char *private_key = NULL;
    char *thing_name = NULL;

    static bool is_initialized = false;

    // Check if already initialized
    if (is_initialized) {
        ESP_LOGW(TAG, "MQTT client initialization already performed.");
        return;
    }

    // After WiFi is initialized in first_phase (via wifi_init_for_scan),
    // store the netif handle
    s_wifi_sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (!s_wifi_sta_netif) {
        ESP_LOGE(TAG, "Failed to get STA netif handle after init");
    }

    list_keys_in_nvs();

    if (key_found_in_nvs("provisioned")) {
        if (read_from_nvs("p2_cert", &client_cert) != ESP_OK || read_from_nvs("p2_key", &private_key) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read one or more p2 values from NVS");
            free(client_cert);
            free(private_key);
            return;
        }
    } else {
        if (read_from_nvs("p1_cert", &client_cert) != ESP_OK || read_from_nvs("p1_key", &private_key) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read one or more p1 values from NVS");
            free(client_cert);
            free(private_key);
            return;
        }
    }

    if (read_from_nvs("mqtt_url", &mqtt_url) != ESP_OK || read_from_nvs("p1_rootCa", &root_ca) != ESP_OK ||
        read_from_nvs("iot_device_name", &thing_name) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read one or more values from NVS");
        free(mqtt_url);
        free(root_ca);
        free(thing_name);
        return;
    }

    ESP_LOGW(TAG, "Thing Name: %s", thing_name);

    // ESP_LOG_BUFFER_HEXDUMP(TAG, private_key, strlen(private_key), ESP_LOG_INFO);

    // Configure MQTT client
    esp_mqtt_client_config_t mqtt_cfg = {.broker =
                                             {
                                                 .address =
                                                     {
                                                         .uri = mqtt_url,
                                                     },
                                                 .verification =
                                                     {
                                                         .certificate = root_ca,
                                                     },
                                             },
                                         .credentials =
                                             {
                                                 .client_id = thing_name,
                                                 .authentication =
                                                     {
                                                         .certificate = client_cert,
                                                         .key = private_key,
                                                     },
                                             },
                                         .session =
                                             {
                                                 .keepalive = 60,
                                             },
                                         .buffer = {
                                             .size = 4096,
                                         }};

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (mqtt_client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize MQTT client");
        free(mqtt_url);
        free(root_ca);
        free(client_cert);
        free(private_key);
        return;
    }

    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, bootstrap_mqtt_event_handler_cb, mqtt_client);
    esp_mqtt_client_start(mqtt_client);
    // print_heap_status();
    is_initialized = true;
}
