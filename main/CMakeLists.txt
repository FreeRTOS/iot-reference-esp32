set(MAIN_SRCS
    "main.c"
    "app_driver.c"
    "networking/wifi/app_wifi.c"
    "networking/mqtt/mqtt.c"
    "networking/mqtt/subscription_manager.c"
    "networking/network_manager/core_mqtt_agent_network_manager.c"
    "networking/network_manager/core_mqtt_agent_events.c"
    "demo_tasks/simple_sub_pub_demo.c"
    "demo_tasks/ota_over_mqtt_demo.c"
    "demo_tasks/temp_sub_pub_demo.c"
)

set(MAIN_INCLUDE_DIRS
    "."
    "networking/wifi"
    "networking/mqtt"
    "networking/network_manager"
)

set(MAIN_REQUIRES
    qrcode
    wifi_provisioning
    ws2812_led
    coreMQTT
    coreMQTT-Agent
    corePKCS11
    coreJSON
    ota-for-aws-iot-embedded-sdk
)

idf_component_register(
    SRCS
        ${MAIN_SRCS}
    INCLUDE_DIRS
        ${MAIN_INCLUDE_DIRS}
    REQUIRES
        ${MAIN_REQUIRES}
)

target_add_binary_data(${COMPONENT_TARGET} "certs/root_cert_auth.pem" TEXT)
target_add_binary_data(${COMPONENT_TARGET} "certs/client.crt" TEXT)
target_add_binary_data(${COMPONENT_TARGET} "certs/client.key" TEXT)
