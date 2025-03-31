#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include "esp_err.h"
#include "mqtt_client.h" // Include MQTT client header for esp_mqtt_event_handle_t

// Tag for logging
#define MQTT_URL_MAX_LENGTH 256
#define MQTT_ROOT_CA_MAX_LENGTH 2048
#define MQTT_CLIENT_CERT_MAX_LENGTH 4096
#define MQTT_CLIENT_PRIVATE_KEY_MAX_LENGTH 4096

#define MQTT_CREATE_TOPIC "$aws/certificates/create/json"
#define MQTT_CREATE_ACCEPTED_TOPIC "$aws/certificates/create/json/accepted"
#define MQTT_CREATE_REJECTED_TOPIC "$aws/certificates/create/json/rejected"

#define MQTT_PROVISION_TOPIC "$aws/provisioning-templates/CoopCopProvisioningTemplate/provision/json"
#define MQTT_PROVISION_ACCEPTED_TOPIC "$aws/provisioning-templates/CoopCopProvisioningTemplate/provision/json/accepted"
#define MQTT_PROVISION_REJECTED_TOPIC "$aws/provisioning-templates/CoopCopProvisioningTemplate/provision/json/rejected"

// OTA Topic Templates
#define MQTT_JOBS_NOTIFY_BASE "$aws/things/"
#define MQTT_JOBS_NOTIFY_SUFFIX "/jobs/notify"
#define MQTT_JOBS_WILDCARD_SUFFIX "/jobs/#"
#define MQTT_STREAMS_WILDCARD_SUFFIX "/streams/#"

#define MAX_TOPIC_LENGTH 256

// Function to initialize the MQTT client
void init_mqtt_client(void);

esp_mqtt_client_handle_t get_mqtt_client();

// Path to the Root CA certificate
extern const char *ROOT_CA_PATH;

#endif // MQTT_HANDLER_H
