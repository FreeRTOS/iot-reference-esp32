// Copyright 2022 Espressif Systems (Shanghai) CO LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <string.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_partition.h"
#include "esp_crc.h"
#include "esp_secure_cert_config.h"
#include "esp_secure_cert_read.h"
#include "nvs_flash.h"

#define TAG "Pre Prov Ops"

#ifdef CONFIG_ESP_SECURE_CERT_NVS_PARTITION

#define NVS_STR         1
#define NVS_BLOB        2
#define NVS_U8          3
#define NVS_U16         4

static int nvs_get(const char *name_space, const char *key, char *value, size_t *len, size_t type)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open_from_partition(ESP_SECURE_CERT_NVS_PARTITION, ESP_SECURE_CERT_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Could not open NVS handle (0x%x)!", err);
        return err;
    }

    switch (type) {
    case NVS_STR:
        err = nvs_get_str(handle, key, value, len);
        break;
    case NVS_BLOB:
        err = nvs_get_blob(handle, key, value, len);
        break;
    case NVS_U8:
        err = nvs_get_u8(handle, key, (uint8_t *)value);
        break;
    case NVS_U16:
        err = nvs_get_u16(handle, key, (uint16_t *)value);
        break;
    default:
        ESP_LOGE(TAG, "Invalid type of NVS data provided");
        err = ESP_ERR_INVALID_ARG;
    }

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%d) reading NVS data!", err);
        return err;
    }

    nvs_close(handle);
    return err;
}

esp_err_t esp_secure_cert_get_priv_key(char *buffer, uint32_t *len)
{
    return nvs_get(ESP_SECURE_CERT_NAMESPACE, ESP_SECURE_CERT_PRIV_KEY, buffer, (size_t *)len, NVS_STR);
}

esp_err_t esp_secure_cert_get_device_cert(char *buffer, uint32_t *len)
{
    return nvs_get(ESP_SECURE_CERT_NAMESPACE, ESP_SECURE_CERT_DEV_CERT, buffer, (size_t *)len, NVS_STR);
}

esp_err_t esp_secure_cert_get_ca_cert(char *buffer, uint32_t *len)
{
    return nvs_get(ESP_SECURE_CERT_NAMESPACE, ESP_SECURE_CERT_CA_CERT, buffer, (size_t *)len, NVS_STR);
}

#ifdef CONFIG_ESP_SECURE_CERT_DS_PERIPHERAL
esp_err_t esp_secure_cert_get_ciphertext(char *buffer, uint32_t *len)
{
    return nvs_get(ESP_SECURE_CERT_NAMESPACE, ESP_SECURE_CERT_CIPHERTEXT, buffer, (size_t *)len, NVS_BLOB);
}

esp_err_t esp_secure_cert_get_iv(char *buffer, uint32_t *len)
{
    return nvs_get(ESP_SECURE_CERT_NAMESPACE, ESP_SECURE_CERT_IV, buffer, (size_t *)len, NVS_BLOB);
}

esp_err_t esp_secure_cert_get_rsa_length(uint16_t *len)
{
    return nvs_get(ESP_SECURE_CERT_NAMESPACE, ESP_SECURE_CERT_RSA_LEN, (void *)len, 0, NVS_U16);
}

esp_err_t esp_secure_cert_get_efuse_key_id(uint8_t *efuse_key_id)
{
    return nvs_get(ESP_SECURE_CERT_NAMESPACE, ESP_SECURE_CERT_EFUSE_KEY_ID, (void *)efuse_key_id, 0, NVS_U8);
}
#endif /* CONFIG_ESP_SECURE_CERT_DS_PERIPHERAL */
esp_err_t esp_secure_cert_init_nvs_partition()
{
    return nvs_flash_init_partition(ESP_SECURE_CERT_NVS_PARTITION);
}

#elif CONFIG_ESP_SECURE_CERT_CUST_FLASH_PARTITION /* CONFIG_ESP_SECURE_CERT_NVS_PARTITION */

static esp_err_t esp_secure_cert_read_raw_flash(const esp_partition_t *partition, uint32_t src_offset, void *dst, uint32_t size)
{
    /* Encrypted partitions need to be read via a cache mapping */
    const void *buf;
    spi_flash_mmap_handle_t handle;
    esp_err_t err;

    err = esp_partition_mmap(partition, src_offset, size, SPI_FLASH_MMAP_DATA, &buf, &handle);
    if (err != ESP_OK) {
        return err;
    }
    memcpy(dst, buf, size);
    spi_flash_munmap(handle);
    return ESP_OK;
}

const void *esp_secure_cert_mmap(const esp_partition_t *partition, uint32_t src_offset, uint32_t size)
{
    /* Encrypted partitions need to be read via a cache mapping */
    const void *buf;
    spi_flash_mmap_handle_t handle;
    esp_err_t err;

    err = esp_partition_mmap(partition, src_offset, size, SPI_FLASH_MMAP_DATA, &buf, &handle);
    if (err != ESP_OK) {
        return NULL;
    }
    return buf;
}

static esp_err_t esp_secure_cert_read_metadata(esp_secure_cert_metadata *metadata, size_t offset, const esp_partition_t *part, uint32_t *data_len, uint32_t *data_crc)
{


    esp_err_t err;
    err = esp_secure_cert_read_raw_flash(part, ESP_SECURE_CERT_METADATA_OFFSET, metadata, sizeof(esp_secure_cert_metadata));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Could not read metadata.");
        return ESP_FAIL;
    }

    if (metadata->magic_word != ESP_SECURE_CERT_METADATA_MAGIC_WORD) {
        ESP_LOGE(TAG, "Metadata magic word does not match");
        return ESP_FAIL;
    }
    switch (offset) {
    case ESP_SECURE_CERT_METADATA_OFFSET:
        *data_len = sizeof(metadata);
        break;
    case ESP_SECURE_CERT_DEV_CERT_OFFSET:
        *data_len = metadata->dev_cert_len;
        *data_crc = metadata->dev_cert_crc;
        break;
    case ESP_SECURE_CERT_CA_CERT_OFFSET:
        *data_len = metadata->ca_cert_len;
        *data_crc = metadata->ca_cert_crc;
        break;
#ifndef CONFIG_ESP_SECURE_CERT_DS_PERIPHERAL
    case ESP_SECURE_CERT_PRIV_KEY_OFFSET:
        *data_len = metadata->priv_key_len;
        *data_crc = metadata->priv_key_crc;
        break;
#else /* !CONFIG_ESP_SECURE_CERT_DS_PERIPHERAL */
    case ESP_SECURE_CERT_CIPHERTEXT_OFFSET:
        *data_len = metadata->ciphertext_len;
        *data_crc = metadata->ciphertext_crc;
        break;
    case ESP_SECURE_CERT_IV_OFFSET:
        *data_len = metadata->iv_len;
        *data_crc = metadata->iv_crc;
        break;
#endif /* CONFIG_ESP_SECURE_CERT_DS_PERIPHERAL */
    default:
        err = ESP_ERR_INVALID_ARG;
        ESP_LOGE(TAG, "Invalid offset value given");
    }

    return err;
}

static esp_err_t esp_secure_cert_get_addr(size_t offset, const void **buffer, uint32_t *len)
{
    esp_err_t err;
    static esp_secure_cert_metadata metadata;
    uint32_t data_len = 0;
    uint32_t data_crc = 0;

    esp_partition_iterator_t it = esp_partition_find(ESP_SECURE_CERT_PARTITION_TYPE, ESP_PARTITION_SUBTYPE_ANY, ESP_SECURE_CERT_PARTITION_NAME);
    if (it == NULL) {
        ESP_LOGE(TAG, "Partition not found.");
        return ESP_FAIL;
    }

    const esp_partition_t *part = esp_partition_get(it);
    if (part == NULL) {
        ESP_LOGE(TAG, "Could not get partition.");
        return ESP_FAIL;
    }

    err = esp_secure_cert_read_metadata(&metadata, offset, part, &data_len, &data_crc);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error in reading the metadata");
        return err;
    }

    *len = data_len;
    *buffer = esp_secure_cert_mmap(part, offset, *len);
    if (buffer == NULL) {
        return ESP_FAIL;
    }

    uint32_t read_crc = esp_crc32_le(UINT32_MAX, (const uint8_t * )*buffer, data_len);
    if (read_crc != data_crc) {
        ESP_LOGE(TAG, "Data has been tampered");
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t esp_secure_cert_read(size_t offset, unsigned char *buffer, uint32_t *len)
{
    esp_err_t err;
    static esp_secure_cert_metadata metadata;
    uint32_t data_len = 0;
    uint32_t data_crc = 0;

    esp_partition_iterator_t it = esp_partition_find(ESP_SECURE_CERT_PARTITION_TYPE, ESP_PARTITION_SUBTYPE_ANY, ESP_SECURE_CERT_PARTITION_NAME);
    if (it == NULL) {
        ESP_LOGE(TAG, "Partition not found.");
        return ESP_FAIL;
    }

    const esp_partition_t *part = esp_partition_get(it);
    if (part == NULL) {
        ESP_LOGE(TAG, "Could not get partition.");
        return ESP_FAIL;
    }

    err = esp_secure_cert_read_metadata(&metadata, offset, part, &data_len, &data_crc);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error in reading the metadata");
        return err;
    }

    if (buffer == NULL) {
        *len = data_len;
        return ESP_OK;
    }

    if (*len < data_len) {
        ESP_LOGE(TAG, "Insufficient length of buffer. buffer size: %d, required: %d", *len, data_len);
        return ESP_FAIL;
    }

    /* If the requested offset belongs to the medatada, return the already read metadata */
    if (offset == ESP_SECURE_CERT_METADATA_OFFSET) {
        memcpy(buffer, &metadata, sizeof(metadata));
        return ESP_OK;
    }

    err = esp_secure_cert_read_raw_flash(part, offset, buffer, data_len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Could not read data.");
        return ESP_FAIL;
    }

    uint32_t read_crc = esp_crc32_le(UINT32_MAX, (const uint8_t * )buffer, data_len);
    if (read_crc != data_crc) {
        ESP_LOGE(TAG, "Data has been tampered");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t esp_secure_cert_get_device_cert(unsigned char *buffer, uint32_t *len)
{
    return esp_secure_cert_read(ESP_SECURE_CERT_DEV_CERT_OFFSET, buffer, len);
}

esp_err_t esp_secure_cert_get_dev_cert_addr(const void **buffer, uint32_t *len)
{
    return esp_secure_cert_get_addr(ESP_SECURE_CERT_DEV_CERT_OFFSET, buffer, len);
}

esp_err_t esp_secure_cert_get_ca_cert(unsigned char *buffer, uint32_t *len)
{
    return esp_secure_cert_read(ESP_SECURE_CERT_CA_CERT_OFFSET, buffer, len);
}

esp_err_t esp_secure_cert_get_ca_cert_addr(const void **buffer, uint32_t *len)
{
    return esp_secure_cert_get_addr(ESP_SECURE_CERT_CA_CERT_OFFSET, buffer, len);
}


#ifndef CONFIG_ESP_SECURE_CERT_DS_PERIPHERAL
esp_err_t esp_secure_cert_get_priv_key_addr(const void **buffer, uint32_t *len)
{
    return esp_secure_cert_get_addr(ESP_SECURE_CERT_PRIV_KEY_OFFSET, buffer, len);
}

esp_err_t esp_secure_cert_get_priv_key(unsigned char *buffer, uint32_t *len)
{
    return esp_secure_cert_read(ESP_SECURE_CERT_PRIV_KEY_OFFSET, buffer, len);
}

#else /* !CONFIG_ESP_SECURE_CERT_DS_PEIPHERAL */

esp_err_t esp_secure_cert_get_ciphertext_addr(const void **buffer, uint32_t *len)
{
    return esp_secure_cert_get_addr(ESP_SECURE_CERT_CIPHERTEXT_OFFSET, buffer, len);
}

esp_err_t esp_secure_cert_get_iv_addr(const void **buffer, uint32_t *len)
{
    return esp_secure_cert_get_addr(ESP_SECURE_CERT_IV_OFFSET, buffer, len);
}

#endif /* CONFIG_ESP_SECURE_CERT_DS_PERIPHERAL */
#endif /* CONFIG_ESP_SECURE_CERT_CUST_FLASH_PARTITION */

#ifdef CONFIG_ESP_SECURE_CERT_DS_PERIPHERAL
esp_ds_data_ctx_t *esp_secure_cert_get_ds_ctx()
{
    esp_err_t esp_ret;
    esp_ds_data_ctx_t *ds_data_ctx;
    ds_data_ctx = (esp_ds_data_ctx_t *)malloc(sizeof(esp_ds_data_ctx_t));
    if (ds_data_ctx == NULL) {
        ESP_LOGE(TAG, "Error in allocating memory for esp_ds_data_context");
        goto exit;
    }

    ds_data_ctx->esp_ds_data = (esp_ds_data_t *)calloc(1, sizeof(esp_ds_data_t));
    if (ds_data_ctx->esp_ds_data == NULL) {
        ESP_LOGE(TAG, "Could not allocate memory for DS data handle ");
        goto exit;
    }
#ifdef CONFIG_ESP_SECURE_CERT_NVS_PARTITION
    uint32_t len = ESP_DS_C_LEN;
    esp_ret = esp_secure_cert_get_ciphertext((char *)ds_data_ctx->esp_ds_data->c, &len);
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Error in reading ciphertext");
        goto exit;
    }

    len = ESP_DS_IV_LEN;
    esp_ret = esp_secure_cert_get_iv((char *)ds_data_ctx->esp_ds_data->iv, &len);
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Error in reading initialization vector");
        goto exit;
    }

    esp_ret = esp_secure_cert_get_efuse_key_id(&ds_data_ctx->efuse_key_id);
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Error in reading efuse key id");
        goto exit;
    }

    esp_ret = esp_secure_cert_get_rsa_length(&ds_data_ctx->rsa_length_bits);
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Error in reading rsa key length");
        goto exit;
    }
    return ds_data_ctx;

#elif CONFIG_ESP_SECURE_CERT_CUST_FLASH_PARTITION
    const void *buffer;
    uint32_t len;
    esp_ret = esp_secure_cert_get_ciphertext_addr(&buffer, &len);
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Error in reading ciphertext");
        goto exit;
    }
    memcpy((void *)ds_data_ctx->esp_ds_data->c, buffer, len);

    esp_ret = esp_secure_cert_get_iv_addr(&buffer, &len);
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Error in reading initialization vector");
        goto exit;
    }

    memcpy((void *)ds_data_ctx->esp_ds_data->iv, buffer, len);
    unsigned char metadata[ESP_SECURE_CERT_METADATA_SIZE] = {};
    len = sizeof(metadata);
    esp_err_t err = esp_secure_cert_read(ESP_SECURE_CERT_METADATA_OFFSET, metadata, &len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error in reading metadata");
        goto exit;
    }
    ds_data_ctx->rsa_length_bits = ((esp_secure_cert_metadata *)metadata)->rsa_length;
    ds_data_ctx->efuse_key_id = ((esp_secure_cert_metadata *)metadata)->efuse_key_id;
    return ds_data_ctx;
#endif
exit:
    if (ds_data_ctx != NULL) {
        free(ds_data_ctx->esp_ds_data);
    }
    free(ds_data_ctx);
    return NULL;
}

void esp_secure_cert_free_ds_ctx(esp_ds_data_ctx_t *ds_ctx)
{
    if (ds_ctx != NULL) {
        free(ds_ctx->esp_ds_data);
    }
    free(ds_ctx);
}
#endif
