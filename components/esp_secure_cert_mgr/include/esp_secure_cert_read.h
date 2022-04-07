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
#pragma once
#include "esp_err.h"

#include "esp_secure_cert_config.h"
#ifdef CONFIG_ESP_SECURE_CERT_DS_PERIPHERAL
#include "rsa_sign_alt.h"
#endif

#ifdef CONFIG_ESP_SECURE_CERT_NVS_PARTITION
/* @info
 * Init the esp_secure_cert nvs partition
 *
 * @return
 *      - ESP_OK    On success
 *      - ESP_FAIL/other relevant esp error code
 *                  On failure
 */
esp_err_t esp_secure_cert_init_nvs_partition();

/* @info
 *  Get the private key from the esp_secure_cert partition
 *
 * @params
 *      buffer(in)  The buffer in which the private key shall be stored.
 *                  If the buffer value is NULL, then the function shall
 *                  return success and the len argument shall hold the value
 *                  of the length of the private key.
 *      len(out)    The length of the private key.
 *
 * @return
 *      - ESP_OK    On success
 *      - ESP_FAIL/other relevant esp error code
 *                  On failure
 */
esp_err_t esp_secure_cert_get_priv_key(char *buffer, uint32_t *len);

/* @info
 *  Get the device cert from the esp_secure_cert partition
 *
 * @params
 *      buffer(in)  The buffer in which the device cert shall be stored.
 *                  If the buffer value is NULL, then the function shall
 *                  return success and the len argument shall hold the value
 *                  of the length of the device cert.
 *      len(out)    The length of the device cert.
 * @return
 *      - ESP_OK    On success
 *      - ESP_FAIL/other relevant esp error code
 *                  On failure
 */
esp_err_t esp_secure_cert_get_device_cert(char *buffer, uint32_t *len);

/* @info
 *  Get the ca cert from the esp_secure_cert partition
 *
 * @params
 *      buffer(in)  The buffer in which the ca cert shall be stored.
 *                  If the buffer value is NULL, then the function shall
 *                  return success and the len argument shall hold the value
 *                  of the length of the ca cert.
 *      len(out)    The length of the ca cert.
 * @return
 *      - ESP_OK    On success
 *      - ESP_FAIL/other relevant esp error code
 *                  On failure
 */
esp_err_t esp_secure_cert_get_ca_cert(char *buffer, uint32_t *len);

#ifdef CONFIG_ESP_SECURE_CERT_DS_PERIPHERAL
/* @info
 *  Get the ciphertext ( encrypted RSA private key) from the esp_secure_cert partition
 *
 * @params
 *      buffer(in)  The buffer in which the ciphertext shall be stored.
 *                  If the buffer value is NULL, then the function shall
 *                  return success and the len argument shall hold the value
 *                  of the length of the ciphertext.
 *      len(out)    The length of the ciphertext.
 * @return
 *      - ESP_OK    On success
 *      - ESP_FAIL/other relevant esp error code
 *                  On failure
 */
esp_err_t esp_secure_cert_get_ciphertext(char *buffer, uint32_t *len);

/* @info
 *  Get the initialization vector (iv) from the esp_secure_cert partition
 *
 * @params
 *      buffer(in)  The buffer in which the iv shall be stored.
 *                  If the buffer value is NULL, then the function shall
 *                  return success and the len argument shall hold the value
 *                  of the length of the iv.
 *      len(out)    The length of the iv.
 * @return
 *      - ESP_OK    On success
 *      - ESP_FAIL/other relevant esp error code
 *                  On failure
 */
esp_err_t esp_secure_cert_get_iv(char *buffer, uint32_t *len);

/* @info
 *  Get the rsa length from the esp_secure_cert partition
 *
 * @params
 *      len(out)    The rsa length.
 * @return
 *      - ESP_OK    On success
 *      - ESP_FAIL/other relevant esp error code
 *                  On failure
 */
esp_err_t esp_secure_cert_get_rsa_length(uint16_t *len);

/* @info
 *  Get the efuse key id from the esp_secure_cert partition
 *
 * @params
 *      efuse_key_id(out)    The efuse key id.
 * @return
 *      - ESP_OK    On success
 *      - ESP_FAIL/other relevant esp error code
 *                  On failure
 */
esp_err_t esp_secure_cert_get_efuse_key_id(uint8_t *efuse_key_id);
#endif /* CONFIG_ESP_SECURE_CERT_DS_PERIPHERAL */
#elif CONFIG_ESP_SECURE_CERT_CUST_FLASH_PARTITION

/* @info
 *       This function returns the flash address of device certificate
 *       The address is mapped to memory.
 *
 * @params
 *      - buffer    This value shall be filled with the device certificate address
 *                      on successfull completion
 *      - len       This value shall be filled with the length of the device certificate
 * @return
 *      - ESP_OK    On success
 *      - ESP_FAIL/other relevant esp error code
 *                  On failure
 */
esp_err_t esp_secure_cert_get_dev_cert_addr(const void **buffer, uint32_t *len);

/* @info
 *       This function returns the flash address of ca certificate
 *       The address is mapped to memory.
 *
 * @params
 *      - buffer    This value shall be filled with the ca certificate address
 *                      on successfull completion
 *      - len       This value shall be filled with the length of the ca certificate
 * @return
 *      - ESP_OK    On success
 *      - ESP_FAIL/other relevant esp error code
 *                  On failure
 */
esp_err_t esp_secure_cert_get_ca_cert_addr(const void **buffer, uint32_t *len);

#ifndef CONFIG_ESP_SECURE_CERT_DS_PERIPHERAL
/* @info
 *       This function returns the flash address of private key
 *       The address is mapped to memory.
 *
 * @params
 *      - buffer    This value shall be filled with the private key address
 *                      on successfull completion
 *      - len       This value shall be filled with the length of the private key * @return
 *      - ESP_OK    On success
 *      - ESP_FAIL/other relevant esp error code
 *                  On failure
 */
esp_err_t esp_secure_cert_get_priv_key_addr(const void **buffer, uint32_t *len);
#else /* !CONFIG_MFG_WRITE_TO_FLASH */

/* @info
 *       This function returns the flash address of initialization vector
 *       The address is mapped to memory.
 *
 * @params
 *      - buffer    This value shall be filled with the initialization vector
 *                      on successfull completion
 *      - len       This value shall be filled with the length of the initialization vector
 * @return
 *      - ESP_OK    On success
 *      - ESP_FAIL/other relevant esp error code
 *                  On failure
 */
esp_err_t esp_secure_cert_get_iv_addr(const void **buffer, uint32_t *len);

/* @info
 *       This function returns the flash address of ciphertext (encrypted private key)
 *       The address is mapped to memory.
 *
 * @params
 *      - buffer    This value shall be filled with the private key ciphertext
 *                      on successfull completion
 *      - len       This value shall be filled with the length of the ciphertext
 * @return
 *      - ESP_OK    On success
 *      - ESP_FAIL/other relevant esp error code
 *                  On failure
 */
esp_err_t esp_secure_cert_get_ciphertext_addr(const void **buffer, uint32_t *len);

#endif /* CONFIG_ESP_SECURE_CERT_DS_PERIPHERAL */
#endif /* CONFIG_ESP_SECURE_CERT_CUST_FLASH_PARTITION */

#ifdef CONFIG_ESP_SECURE_CERT_DS_PERIPHERAL
/* @info
 *       This function returns the flash esp_ds_context which can then be
 *       directly provided to an esp-tls connection through its config structure.
 *       The memory for the context is dynamically allocated.
 *
 * @params
 *      - ds_ctx    The pointer to the DS context
 * @return
 *      - ESP_OK    On success
 *      - ESP_FAIL/other relevant esp error code
 *                  On failure
 */
esp_ds_data_ctx_t *esp_secure_cert_get_ds_ctx();

/*
 *@info
 *      Free the ds context
 */
void esp_secure_cert_free_ds_ctx(esp_ds_data_ctx_t *ds_ctx);
#endif /* CONFIG_ESP_SECURE_CERT_DS_PERIPHERAL */
