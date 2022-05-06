# This gives QUALIFICATION_TEST_SOURCES, and QUALIFICATION_TEST_INCLUDE_DIRS
include(${CMAKE_CURRENT_LIST_DIR}/FreeRTOS-Libraries-Integration-Tests/qualification_test.cmake)

# This gives TRANSPORT_TEST_SOURCES, and TRANSPORT_TEST_INCLUDE_DIRS
include(${CMAKE_CURRENT_LIST_DIR}/FreeRTOS-Libraries-Integration-Tests/src/transport_interface_test.cmake)

set(IDT_INCLUDE_DIRS
    ${QUALIFICATION_TEST_INCLUDE_DIRS}
    ${TRANSPORT_TEST_INCLUDE_DIRS}
    ${CMAKE_CURRENT_LIST_DIR}/config
)

set(IDT_SRCS
    ${QUALIFICATION_TEST_SOURCES}
    ${TRANSPORT_TEST_SOURCES}
    ${CMAKE_CURRENT_LIST_DIR}/port/platform_function.c
)

set(IDT_REQUIRES
    unity
    coreMQTT
    coreHTTP
    freertos
)

idf_component_register(
    SRCS
        ${IDT_SRCS}
    INCLUDE_DIRS
        ${IDT_INCLUDE_DIRS}
    REQUIRES
        ${IDT_REQUIRES}
)
