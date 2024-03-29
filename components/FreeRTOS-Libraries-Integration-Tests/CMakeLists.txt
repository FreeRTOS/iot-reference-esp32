# This gives QUALIFICATION_TEST_SOURCES, and QUALIFICATION_TEST_INCLUDE_DIRS
include(${CMAKE_CURRENT_LIST_DIR}/FreeRTOS-Libraries-Integration-Tests/qualification_test.cmake)

# This gives TRANSPORT_TEST_SOURCES, and TRANSPORT_TEST_INCLUDE_DIRS
include(${CMAKE_CURRENT_LIST_DIR}/FreeRTOS-Libraries-Integration-Tests/src/transport_interface_test.cmake)

# This gives MQTT_TEST_SOURCES, and MQTT_TEST_INCLUDE_DIRS
include(${CMAKE_CURRENT_LIST_DIR}/FreeRTOS-Libraries-Integration-Tests/src/mqtt_test.cmake)

# This gives OTA_PAL_TEST_SOURCES, and OTA_PAL_TEST_INCLUDE_DIRS
include(${CMAKE_CURRENT_LIST_DIR}/FreeRTOS-Libraries-Integration-Tests/src/ota_pal_test.cmake)

# This gives PKCS11_TEST_SOURCES, and PKCS11_TEST_INCLUDE_DIRS
include(${CMAKE_CURRENT_LIST_DIR}/FreeRTOS-Libraries-Integration-Tests/src/pkcs11_test.cmake)

set(IDT_INCLUDE_DIRS
    ${QUALIFICATION_TEST_INCLUDE_DIRS}
    ${TRANSPORT_TEST_INCLUDE_DIRS}
    ${MQTT_TEST_INCLUDE_DIRS}
    ${OTA_PAL_TEST_INCLUDE_DIRS}
    ${PKCS11_TEST_INCLUDE_DIRS}
    ${CMAKE_CURRENT_LIST_DIR}/config
)

set(IDT_SRCS
    ${QUALIFICATION_TEST_SOURCES}
    ${TRANSPORT_TEST_SOURCES}
    ${MQTT_TEST_SOURCES}
    ${OTA_PAL_TEST_SOURCES}
    ${PKCS11_TEST_SOURCES}
    ${CMAKE_CURRENT_LIST_DIR}/port/platform_function.c
    ${CMAKE_CURRENT_LIST_DIR}/../esp-aws-iot/libraries/corePKCS11/corePKCS11/source/dependency/3rdparty/mbedtls_utils/mbedtls_utils.c
)

set(IDT_REQUIRES
    unity
    coreMQTT
    coreHTTP
    freertos
    corePKCS11
    aws-iot-core-mqtt-file-streams-embedded-c
)

idf_component_register(
    SRCS
        ${IDT_SRCS}
    INCLUDE_DIRS
        ${IDT_INCLUDE_DIRS}
    REQUIRES
        ${IDT_REQUIRES}
)
