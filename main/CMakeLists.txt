set(MAIN_SRCS
    "main.c"
    "app_driver.c"
    "networking/wifi/app_wifi.c"
    "networking/mqtt/mqtt.c"
    "networking/mqtt/subscription_manager.c"
    "networking/network_manager/core_mqtt_agent_network_manager.c"
    "networking/network_manager/core_mqtt_agent_events.c"
    "demo_tasks/temp_sub_pub_demo.c"
)

# Demo enables

# Sub Pub Unsub demo

if(CONFIG_GRI_ENABLE_SUB_PUB_UNSUB_DEMO)
    list(APPEND MAIN_SRCS "demo_tasks/sub_pub_unsub_demo/sub_pub_unsub_demo.c")
endif()

# OTA demo
if(CONFIG_GRI_ENABLE_OTA_DEMO)
    list(APPEND MAIN_SRCS "demo_tasks/ota/ota_over_mqtt_demo.c")
endif()

set(MAIN_INCLUDE_DIRS
    "."
    "demo_tasks/ota"
    "demo_tasks/sub_pub_unsub_demo"
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
    backoffAlgorithm
    esp_secure_cert_mgr
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
