set(NETWORK_TRANSPORT_SRCS
    "esp_tls_transport.c"
)

set(NETWORK_TRANSPORT_INCLUDE_DIRS
    "."
)

set(NETWORK_TRANSPORT_REQUIRES
    esp-tls
    esp-aws-iot
)

idf_component_register(
    SRCS
        ${NETWORK_TRANSPORT_SRCS}
    INCLUDE_DIRS
        ${NETWORK_TRANSPORT_INCLUDE_DIRS}
    REQUIRES
        ${NETWORK_TRANSPORT_REQUIRES}
)