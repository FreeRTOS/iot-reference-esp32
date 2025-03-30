#ifndef FIRMWARE_DATA_H
#define FIRMWARE_DATA_H

#include <stdint.h>

// Function to load firmware data from partitions
void load_firmware_data(void);

// Functions to access loaded data
const uint8_t *get_firmware_hash(void);
const uint8_t *get_firmware_signature(void);
const uint8_t get_firmware_device_type(void);

#endif // FIRMWARE_DATA_H
