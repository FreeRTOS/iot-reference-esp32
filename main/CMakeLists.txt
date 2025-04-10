set(MAIN_SRCS
    "main.c"
    "src/wifi_handler.c"
    "src/mqtt_handler.c"
    "src/ble_handler.c"
    "src/device_id.c"
    "src/efuse_table.c"
    "src/firmware_data.c"
    "src/gap.c"
    "src/gatt_svc.c"
    "src/secure_connections.c"
    "src/utils.c"
    "src/provisioning_state.c"
    "src/ota_over_mqtt_demo.c"
    "src/ota_pal.c"
)

set(MAIN_INCLUDE_DIRS
    "."
    "include"
    "${IDF_PATH}/components/bt/host/bluedroid/api/include/api"
    "${IDF_PATH}/components/bt/host/nimble/nimble/nimble/host/include"
    "networking/mqtt"
)

set(MAIN_REQUIRES
    bt
    nvs_flash
    json
    mbedtls
    mqtt
    gecl-nvs-manager
    esp-tls
    qrcode
    wifi_provisioning
    coreMQTT
    coreMQTT-Agent
    corePKCS11
    coreJSON
    backoffAlgorithm
#    esp_secure_cert_mgr
    aws-iot-core-mqtt-file-streams-embedded-c
#    FreeRTOS-Libraries-Integration-Tests
    unity
    driver
    esp_gdbstub
)

idf_component_register(
    SRCS
        ${MAIN_SRCS}
    INCLUDE_DIRS
        ${MAIN_INCLUDE_DIRS}
    REQUIRES
        ${MAIN_REQUIRES}
)

