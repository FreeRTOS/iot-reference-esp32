set(MAIN_SRCS
    "main.c"
    "networking/wifi/app_wifi.c"
    "networking/mqtt/subscription_manager.c"
    "networking/mqtt/core_mqtt_agent_manager.c"
    "networking/mqtt/core_mqtt_agent_manager_events.c"
)

# Demo enables

# Sub Pub Unsub demo
if(CONFIG_GRI_ENABLE_SUB_PUB_UNSUB_DEMO)
    list(APPEND MAIN_SRCS "demo_tasks/sub_pub_unsub_demo/sub_pub_unsub_demo.c")
endif()

# Temperature Sub Pub and LED control demo
if(CONFIG_GRI_ENABLE_TEMPERATURE_PUB_SUB_AND_LED_CONTROL_DEMO)
    list(APPEND MAIN_SRCS
        "demo_tasks/temp_sub_pub_and_led_control_demo/temp_sub_pub_and_led_control_demo.c"
        "demo_tasks/temp_sub_pub_and_led_control_demo/hardware_drivers/app_driver.c"
    )
endif()

# OTA demo
if(CONFIG_GRI_ENABLE_OTA_DEMO)
    list(APPEND MAIN_SRCS "demo_tasks/ota_over_mqtt_demo/ota_over_mqtt_demo.c")
endif()

# Qualification Test
if( CONFIG_GRI_RUN_QUALIFICATION_TEST )
    list(APPEND MAIN_SRCS
        "qualification_app_main.c"
        "demo_tasks/ota_over_mqtt_demo/ota_over_mqtt_demo.c"
        "demo_tasks/sub_pub_unsub_demo/sub_pub_unsub_demo.c")
endif()

set(MAIN_INCLUDE_DIRS
    "."
    "demo_tasks/ota_over_mqtt_demo"
    "demo_tasks/sub_pub_unsub_demo"
    "demo_tasks/temp_sub_pub_and_led_control_demo"
    "demo_tasks/temp_sub_pub_and_led_control_demo/hardware_drivers"
    "networking/wifi"
    "networking/mqtt"
)

set(MAIN_REQUIRES
    qrcode
    wifi_provisioning
    coreMQTT
    coreMQTT-Agent
    corePKCS11
    coreJSON
    backoffAlgorithm
    esp_secure_cert_mgr
    aws-iot-core-mqtt-file-streams-embedded-c
    FreeRTOS-Libraries-Integration-Tests
    unity
    driver
)

idf_component_register(
    SRCS
        ${MAIN_SRCS}
    INCLUDE_DIRS
        ${MAIN_INCLUDE_DIRS}
    REQUIRES
        ${MAIN_REQUIRES}
)

# OTA demo
if( CONFIG_GRI_ENABLE_OTA_DEMO OR CONFIG_GRI_RUN_QUALIFICATION_TEST )
    target_add_binary_data(${COMPONENT_TARGET} "certs/aws_codesign.crt" TEXT)
endif()

# Root Certificate
target_add_binary_data(${COMPONENT_TARGET} "certs/root_cert_auth.crt" TEXT)
