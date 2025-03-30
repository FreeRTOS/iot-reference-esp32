/*
 * SPDX-FileCopyrightText: 2017-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "sdkconfig.h"
#include "esp_efuse.h"
#include <assert.h>
#include "efuse_table.h"

// md5_digest_table ad315018dde80877e93c0bd591617777
// This file was generated from the file efuse_table.csv. DO NOT CHANGE THIS FILE MANUALLY.
// If you want to change some fields, you need to change efuse_table.csv file
// then run `efuse_common_table` or `efuse_custom_table` command it will generate this file.
// To show efuse_table run the command 'show_efuse_table'.

#define MAX_BLK_LEN CONFIG_EFUSE_MAX_BLK_LEN

// The last free bit in the block is counted over the entire file.
#define LAST_FREE_BIT_BLK3 136

_Static_assert(LAST_FREE_BIT_BLK3 <= MAX_BLK_LEN, "The eFuse table does not match the coding scheme. Edit the table and restart the efuse_common_table or efuse_custom_table command to regenerate the new files.");

static const esp_efuse_desc_t COOP_COP_DEVICE_TYPE[] = {
    {EFUSE_BLK3, 128, 8}, 	 // ,
};





const esp_efuse_desc_t* ESP_EFUSE_COOP_COP_DEVICE_TYPE[] = {
    &COOP_COP_DEVICE_TYPE[0],    		// 
    NULL
};
