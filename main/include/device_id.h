#ifndef DEVICE_ID_H
#define DEVICE_ID_H

#include <stdint.h>

#define MAC_ADDRESS_SIZE 6

/**
 * @brief Creates a unique device ID by hashing the Base MAC Address and Firmware Hash.
 */
void create_device_id(void);

/**
 * @brief Retrieves the generated device ID.
 * @return A pointer to the 32-byte device ID array.
 */
const uint8_t *get_device_id(void);

#endif // DEVICE_ID_H
