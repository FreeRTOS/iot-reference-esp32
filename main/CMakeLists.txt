set(MAIN_SRCS
    "main.c"
    "networking/wifi/app_wifi.c"
    "networking/mqtt/mqtt.c"
    "networking/mqtt/subscription_manager.c"
    "networking/network_manager/network_manager.c"
    "demo_tasks/large_message_sub_pub_demo.c"
)

set(MAIN_INCLUDE_DIRS
    "."
    "networking/wifi"
    "networking/mqtt"
    "networking/network_manager"
)

set(MAIN_REQUIRES
)

idf_component_register(
    SRCS
        ${MAIN_SRCS}
    INCLUDE_DIRS
        ${MAIN_INCLUDE_DIRS}
)

target_add_binary_data(${COMPONENT_TARGET} "certs/root_cert_auth.pem" TEXT)
target_add_binary_data(${COMPONENT_TARGET} "certs/client.crt" TEXT)
target_add_binary_data(${COMPONENT_TARGET} "certs/client.key" TEXT)