/*
 * Copyright 2018 Espressif Systems (Shanghai) PTE LTD
 *
 * FreeRTOS OTA PAL for ESP32-DevKitC ESP-WROVER-KIT V1.0.4
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.*/

/* OTA PAL implementation for Espressif esp32_devkitc_esp_wrover_kit platform */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/param.h>

#include "ota_config.h"

#include "ota_config.h"
#include "ota_pal.h"

#include "core_pkcs11.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "hal/wdt_hal.h"
#include "iot_crypto.h"

#if !CONFIG_IDF_TARGET_ESP32C6 && !CONFIG_IDF_TARGET_ESP32H2
#include "soc/rtc_cntl_reg.h"
#else
#include "soc/lp_analog_peri_reg.h"
#include "soc/lp_timer_reg.h"
#include "soc/lp_wdt_reg.h"
#include "soc/pmu_reg.h"
#endif

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
#include "spi_flash_mmap.h"
#else
#include "esp_spi_flash.h"
#endif
#include "aws_esp_ota_ops.h"
#include "esp_image_format.h"
#include "esp_ota_ops.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mbedtls/asn1.h"
#include "mbedtls/base64.h"
#include "mbedtls/bignum.h"
#include "mbedtls/md.h" // Add this at the top with other includes
#include "mbedtls/pk.h"

static mbedtls_pk_context codeSigningKey; // Declare static key context globally

#define OTA_HALF_SECOND_DELAY pdMS_TO_TICKS(500UL)
#define ECDSA_INTEGER_LEN 32

static const char *TAG = "MY_OTA_PAL";

/* Check configuration for memory constraints provided SPIRAM is not enabled */
#if !CONFIG_SPIRAM_SUPPORT
#if (configENABLED_DATA_PROTOCOLS & OTA_DATA_OVER_HTTP) && (configENABLED_NETWORKS & AWSIOT_NETWORK_TYPE_BLE)
#error "Cannot enable OTA data over HTTP together with BLE because of not enough heap."
#endif
#endif /* !CONFIG_SPIRAM_SUPPORT */

/*
 * Includes 4 bytes of version field, followed by 64 bytes of signature
 * (Rest 12 bytes for padding to make it 16 byte aligned for flash encryption)
 */
#define ECDSA_SIG_SIZE 80

typedef struct {
    const esp_partition_t *update_partition;
    const AfrOtaJobDocumentFields_t *cur_ota;
    esp_ota_handle_t update_handle;
    uint32_t data_write_len;
    bool valid_image;
} esp_ota_context_t;

typedef struct {
    uint8_t sec_ver[4];
    uint8_t raw_ecdsa_sig[64];
    uint8_t pad[12];
} esp_sec_boot_sig_t;

static esp_ota_context_t ota_ctx;

static char *codeSigningCertificatePEM = NULL;

/* Specify the OTA signature algorithm we support on this platform. */
const char OTA_JsonFileSignatureKey[OTA_FILE_SIG_KEY_STR_MAX_LENGTH] = "sig-sha256-ecdsa";

static CK_RV prvGetCertificateHandle(CK_FUNCTION_LIST_PTR pxFunctionList, CK_SESSION_HANDLE xSession,
                                     const char *pcLabelName, CK_OBJECT_HANDLE_PTR pxCertHandle);
static CK_RV prvGetCertificate(const char *pcLabelName, uint8_t **ppucData, uint32_t *pulDataSize);

static OtaPalStatus_t asn1_to_raw_ecdsa(const uint8_t *signature, uint16_t sig_len, uint8_t *out_signature) {
    int ret = 0;
    const unsigned char *end = signature + sig_len;
    size_t len;
    mbedtls_mpi r = {0};
    mbedtls_mpi s = {0};

    if (out_signature == NULL) {
        LogError(("ASN1 invalid argument !"));
        goto cleanup;
    }

    mbedtls_mpi_init(&r);
    mbedtls_mpi_init(&s);

    if ((ret = mbedtls_asn1_get_tag(&signature, end, &len, MBEDTLS_ASN1_CONSTRUCTED | MBEDTLS_ASN1_SEQUENCE)) != 0) {
        LogError(("Bad Input Signature"));
        goto cleanup;
    }

    if (signature + len != end) {
        LogError(("Incorrect ASN1 Signature Length"));
        goto cleanup;
    }

    if (((ret = mbedtls_asn1_get_mpi(&signature, end, &r)) != 0) ||
        ((ret = mbedtls_asn1_get_mpi(&signature, end, &s)) != 0)) {
        LogError(("ASN1 parsing failed"));
        goto cleanup;
    }

    ret = mbedtls_mpi_write_binary(&r, out_signature, ECDSA_INTEGER_LEN);
    ret = mbedtls_mpi_write_binary(&s, out_signature + ECDSA_INTEGER_LEN, ECDSA_INTEGER_LEN);

cleanup:
    mbedtls_mpi_free(&r);
    mbedtls_mpi_free(&s);

    if (ret == 0) {
        return OtaPalSuccess;
    } else {
        return OtaPalBadSignerCert;
    }
}

static void _esp_ota_ctx_clear(esp_ota_context_t *ota_ctx) {
    if (ota_ctx != NULL) {
        memset(ota_ctx, 0, sizeof(esp_ota_context_t));
    }
}

static bool _esp_ota_ctx_validate(AfrOtaJobDocumentFields_t *pFileContext) {
    return (pFileContext != NULL && ota_ctx.cur_ota == pFileContext);
}

static void _esp_ota_ctx_close(AfrOtaJobDocumentFields_t *pFileContext) {
    if (pFileContext != NULL) {
        pFileContext->fileId = 0;
    }

    /*memset(&ota_ctx, 0, sizeof(esp_ota_context_t)); */
    ota_ctx.cur_ota = 0;
}

/* Abort receiving the specified OTA update by closing the file. */
OtaPalStatus_t otaPal_Abort(AfrOtaJobDocumentFields_t *const pFileContext) {
    OtaPalStatus_t ota_ret = OtaPalAbortFailed;

    if (_esp_ota_ctx_validate(pFileContext)) {
        _esp_ota_ctx_close(pFileContext);
        ota_ret = OtaPalSuccess;
    } else if (pFileContext && (pFileContext->fileId == 0)) {
        ota_ret = OtaPalSuccess;
    }

    return ota_ret;
}

/* Attempt to create a new receive file for the file chunks as they come in. */
OtaPalStatus_t otaPal_CreateFileForRx(AfrOtaJobDocumentFields_t *const pFileContext) {
    if ((NULL == pFileContext) || (NULL == pFileContext->filepath)) {
        return OtaPalRxFileCreateFailed;
    }

    if (otaPal_SetPlatformImageState(pFileContext, OtaImageStateAccepted) == OtaPalSuccess) {
        /* This demo just accepts the image. But if the application wants to
         * verify any more details about it can be done here. Once verified,
         * the success message can be sent to IoT core. */
        return OtaPalNewImageBooted;
    }

    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);

    if (update_partition == NULL) {
        LogError(("Failed to find update partition"));
        return OtaPalRxFileCreateFailed;
    }

    LogInfo(("Writing to partition subtype %d at offset 0x%" PRIx32 "", update_partition->subtype,
             update_partition->address));

    esp_ota_handle_t update_handle;
    esp_err_t err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle);

    if (err != ESP_OK) {
        LogError(("esp_ota_begin failed (%d)", err));
        return OtaPalRxFileCreateFailed;
    }

    ota_ctx.cur_ota = pFileContext;
    ota_ctx.update_partition = update_partition;
    ota_ctx.update_handle = update_handle;

    ota_ctx.data_write_len = 0;
    ota_ctx.valid_image = false;

    LogInfo(("esp_ota_begin succeeded"));

    return OtaPalSuccess;
}

static CK_RV prvGetCertificateHandle(CK_FUNCTION_LIST_PTR pxFunctionList, CK_SESSION_HANDLE xSession,
                                     const char *pcLabelName, CK_OBJECT_HANDLE_PTR pxCertHandle) {
    CK_ATTRIBUTE xTemplate;
    CK_RV xResult = CKR_OK;
    CK_ULONG ulCount = 0;
    CK_BBOOL xFindInit = CK_FALSE;

    /* Get the certificate handle. */
    if (0 == xResult) {
        xTemplate.type = CKA_LABEL;
        xTemplate.ulValueLen = strlen(pcLabelName) + 1;
        xTemplate.pValue = (char *)pcLabelName;
        xResult = pxFunctionList->C_FindObjectsInit(xSession, &xTemplate, 1);
    }

    if (0 == xResult) {
        xFindInit = CK_TRUE;
        xResult = pxFunctionList->C_FindObjects(xSession, (CK_OBJECT_HANDLE_PTR)pxCertHandle, 1, &ulCount);
    }

    if (CK_TRUE == xFindInit) {
        xResult = pxFunctionList->C_FindObjectsFinal(xSession);
    }

    return xResult;
}

/* Note that this function mallocs a buffer for the certificate to reside in,
 * and it is the responsibility of the caller to free the buffer. */
static CK_RV prvGetCertificate(const char *pcLabelName, uint8_t **ppucData, uint32_t *pulDataSize) {
    /* Find the certificate */
    CK_OBJECT_HANDLE xHandle = 0;
    CK_RV xResult;
    CK_FUNCTION_LIST_PTR xFunctionList;
    CK_SLOT_ID xSlotId;
    CK_ULONG xCount = 1;
    CK_SESSION_HANDLE xSession;
    CK_ATTRIBUTE xTemplate = {0};
    uint8_t *pucCert = NULL;
    CK_BBOOL xSessionOpen = CK_FALSE;

    xResult = C_GetFunctionList(&xFunctionList);

    if (CKR_OK == xResult) {
        xResult = xFunctionList->C_Initialize(NULL);
    }

    if ((CKR_OK == xResult) || (CKR_CRYPTOKI_ALREADY_INITIALIZED == xResult)) {
        xResult = xFunctionList->C_GetSlotList(CK_TRUE, &xSlotId, &xCount);
    }

    if (CKR_OK == xResult) {
        xResult = xFunctionList->C_OpenSession(xSlotId, CKF_SERIAL_SESSION, NULL, NULL, &xSession);
    }

    if (CKR_OK == xResult) {
        xSessionOpen = CK_TRUE;
        xResult = prvGetCertificateHandle(xFunctionList, xSession, pcLabelName, &xHandle);
    }

    if ((xHandle != 0) && (xResult == CKR_OK)) /* 0 is an invalid handle */
    {
        /* Get the length of the certificate */
        xTemplate.type = CKA_VALUE;
        xTemplate.pValue = NULL;
        xResult = xFunctionList->C_GetAttributeValue(xSession, xHandle, &xTemplate, xCount);

        if (xResult == CKR_OK) {
            pucCert = pvPortMalloc(xTemplate.ulValueLen);
        }

        if ((xResult == CKR_OK) && (pucCert == NULL)) {
            xResult = CKR_HOST_MEMORY;
        }

        if (xResult == CKR_OK) {
            xTemplate.pValue = pucCert;
            xResult = xFunctionList->C_GetAttributeValue(xSession, xHandle, &xTemplate, xCount);

            if (xResult == CKR_OK) {
                *ppucData = pucCert;
                *pulDataSize = xTemplate.ulValueLen;
            } else {
                vPortFree(pucCert);
            }
        }
    } else /* Certificate was not found. */
    {
        *ppucData = NULL;
        *pulDataSize = 0;
    }

    if (xSessionOpen == CK_TRUE) {
        (void)xFunctionList->C_CloseSession(xSession);
    }

    return xResult;
}

uint8_t *otaPal_ReadAndAssumeCertificate(const uint8_t *const pucCertName, uint32_t *const ulSignerCertSize) {
    uint8_t *pucCertData;
    uint32_t ulCertSize;
    uint8_t *pucSignerCert = NULL;
    CK_RV xResult;

    xResult = prvGetCertificate((const char *)pucCertName, &pucSignerCert, ulSignerCertSize);

    if ((xResult == CKR_OK) && (pucSignerCert != NULL)) {
        LogInfo(("Using cert with label: %s OK", (const char *)pucCertName));
    } else {
        LogInfo(("No such certificate file: %s. Using certificate in ota_config.h.", (const char *)pucCertName));

        if (codeSigningCertificatePEM == NULL) {
            LogError(("Certificate not set"));
            return NULL;
        }
        /* Allocate memory for the signer certificate plus a terminating zero so we can copy it and return to the
         * caller. */
        ulCertSize = strlen(codeSigningCertificatePEM) + 1;
        pucSignerCert = pvPortMalloc(ulCertSize);           /*lint !e9029 !e9079 !e838 malloc proto requires void*. */
        pucCertData = (uint8_t *)codeSigningCertificatePEM; /*lint !e9005 we don't modify the cert but it could be set
                                                               by PKCS11 so it's not const. */

        if (pucSignerCert != NULL) {
            memcpy(pucSignerCert, pucCertData, ulCertSize);
            *ulSignerCertSize = ulCertSize;
        } else {
            LogError(("No memory for certificate in otaPal_ReadAndAssumeCertificate!"));
        }
    }

    return pucSignerCert;
}

/* Verify the signature of the specified file. */

OtaPalStatus_t otaPal_CheckFileSignature(AfrOtaJobDocumentFields_t *const pFileContext) {
    OtaPalStatus_t result = OtaPalSignatureCheckFailed;
    mbedtls_md_context_t md_ctx;
    unsigned char hash[32]; // SHA-256 hash size
    static spi_flash_mmap_handle_t ota_data_map;
    uint32_t mmu_free_pages_count, len, flash_offset = 0;
    int ret;

    // Initialize hash context
    mbedtls_md_init(&md_ctx);
    ret = mbedtls_md_setup(&md_ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 0);
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to setup hash context: %d", ret);
        goto cleanup;
    }
    ret = mbedtls_md_starts(&md_ctx);
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to start hash: %d", ret);
        goto cleanup;
    }

    // Map and hash the firmware incrementally
    mmu_free_pages_count = spi_flash_mmap_get_free_pages(SPI_FLASH_MMAP_DATA);
    len = ota_ctx.data_write_len;

    while (len > 0) {
        uint32_t mmu_page_offset = ((flash_offset & 0x0000FFFF) != 0) ? 1 : 0;
        uint32_t partial_image_len = MIN(len, ((mmu_free_pages_count - mmu_page_offset) * SPI_FLASH_MMU_PAGE_SIZE));
        const void *buf = NULL;

        esp_err_t map_ret = esp_partition_mmap(ota_ctx.update_partition, flash_offset, partial_image_len,
                                               SPI_FLASH_MMAP_DATA, &buf, &ota_data_map);
        if (map_ret != ESP_OK) {
            ESP_LOGE(TAG, "Partition mmap failed %d", map_ret);
            goto cleanup;
        }

        // Update hash with current chunk
        ret = mbedtls_md_update(&md_ctx, buf, partial_image_len);
        if (ret != 0) {
            ESP_LOGE(TAG, "Failed to update hash: %d", ret);
            spi_flash_munmap(ota_data_map);
            goto cleanup;
        }

        spi_flash_munmap(ota_data_map);
        flash_offset += partial_image_len;
        len -= partial_image_len;
    }

    // Finalize the hash
    ret = mbedtls_md_finish(&md_ctx, hash);
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to finish hash: %d", ret);
        goto cleanup;
    }

    ESP_LOG_BUFFER_HEX("OTA_SIG", pFileContext->signature,
                       pFileContext->signatureLen > 8 ? 8 : pFileContext->signatureLen);

    // Verify the signature using the public key
    ret = mbedtls_pk_verify(&codeSigningKey, MBEDTLS_MD_SHA256, hash, sizeof(hash),
                            (const unsigned char *)pFileContext->signature, pFileContext->signatureLen);
    if (ret == 0) {
        ESP_LOGI(TAG, "Signature verification succeeded");
        result = OtaPalSuccess;
    } else {
        ESP_LOGE(TAG, "Signature verification failed: %d", ret);
    }

cleanup:
    mbedtls_md_free(&md_ctx);
    return result;
}

/* Close the specified file. This shall authenticate the file if it is marked as secure. */
OtaPalStatus_t otaPal_CloseFile(AfrOtaJobDocumentFields_t *const pFileContext) {
    OtaPalStatus_t mainErr = OtaPalSuccess;

    if (!_esp_ota_ctx_validate(pFileContext)) {
        return OtaPalFileClose;
    }

    if (pFileContext->signature == NULL) {
        LogError(("Image Signature not found"));
        _esp_ota_ctx_clear(&ota_ctx);
        mainErr = OtaPalSignatureCheckFailed;
    } else if (ota_ctx.data_write_len == 0) {
        LogError(("No data written to partition"));
        mainErr = OtaPalSignatureCheckFailed;
    } else {
        /* Verify the file signature, close the file and return the signature verification result. */
        mainErr = otaPal_CheckFileSignature(pFileContext);

        if (mainErr != OtaPalSuccess) {
            esp_partition_erase_range(ota_ctx.update_partition, 0, ota_ctx.update_partition->size);
        } else {
            /* Write ASN1 decoded signature at the end of firmware image for bootloader to validate during bootup */
            esp_sec_boot_sig_t *sec_boot_sig = (esp_sec_boot_sig_t *)malloc(sizeof(esp_sec_boot_sig_t));

            if (sec_boot_sig != NULL) {
                memset(sec_boot_sig->sec_ver, 0x00, sizeof(sec_boot_sig->sec_ver));
                memset(sec_boot_sig->pad, 0xFF, sizeof(sec_boot_sig->pad));
                mainErr = asn1_to_raw_ecdsa((const uint8_t *)pFileContext->signature, pFileContext->signatureLen,
                                            sec_boot_sig->raw_ecdsa_sig);
                if (mainErr == OtaPalSuccess) {
                    esp_err_t ret = esp_ota_write_with_offset(ota_ctx.update_handle, sec_boot_sig, ECDSA_SIG_SIZE,
                                                              ota_ctx.data_write_len);

                    if (ret != ESP_OK) {
                        return OtaPalFileClose;
                    }

                    ota_ctx.data_write_len += ECDSA_SIG_SIZE;
                }

                free(sec_boot_sig);
                ota_ctx.valid_image = true;
            } else {
                mainErr = OtaPalFileClose;
            }
        }
    }

    return mainErr;
}

OtaPalStatus_t IRAM_ATTR otaPal_ResetDevice(AfrOtaJobDocumentFields_t *const pFileContext) {
    (void)pFileContext;

    /* Short delay for debug log output before reset. */
    vTaskDelay(OTA_HALF_SECOND_DELAY);
    esp_restart();
    return OtaPalSuccess;
}

OtaPalStatus_t otaPal_ActivateNewImage(AfrOtaJobDocumentFields_t *const pFileContext) {
    (void)pFileContext;

    if (ota_ctx.cur_ota != NULL) {
        if (esp_ota_end(ota_ctx.update_handle) != ESP_OK) {
            LogError(("esp_ota_end failed!"));
            esp_partition_erase_range(ota_ctx.update_partition, 0, ota_ctx.update_partition->size);
            otaPal_ResetDevice(pFileContext);
        }

        esp_err_t err = esp_ota_set_boot_partition(ota_ctx.update_partition);

        if (err != ESP_OK) {
            LogError(("esp_ota_set_boot_partition failed (%d)!", err));
            esp_partition_erase_range(ota_ctx.update_partition, 0, ota_ctx.update_partition->size);
            _esp_ota_ctx_clear(&ota_ctx);
        }

        otaPal_ResetDevice(pFileContext);
    }

    _esp_ota_ctx_clear(&ota_ctx);
    otaPal_ResetDevice(pFileContext);
    return OtaPalSuccess;
}

/* Write a block of data to the specified file. */
int16_t otaPal_WriteBlock(AfrOtaJobDocumentFields_t *const pFileContext, uint32_t iOffset, uint8_t *const pacData,
                          uint32_t iBlockSize) {
    if (_esp_ota_ctx_validate(pFileContext)) {
        esp_err_t ret = esp_ota_write_with_offset(ota_ctx.update_handle, pacData, iBlockSize, iOffset);

        if (ret != ESP_OK) {
            LogError(("Couldn't flash at the offset %" PRIu32 "", iOffset));
            return -1;
        }

        ota_ctx.data_write_len += iBlockSize;
    } else {
        LogInfo(("Invalid OTA Context"));
        return -1;
    }

    return iBlockSize;
}

OtaPalImageState_t otaPal_GetPlatformImageState(AfrOtaJobDocumentFields_t *const pFileContext) {
    OtaPalImageState_t eImageState = OtaPalImageStateUnknown;
    uint32_t ota_flags;

    (void)pFileContext;

    LogInfo(("%s", __func__));

    if ((ota_ctx.cur_ota != NULL) && (ota_ctx.data_write_len != 0)) {
        /* Firmware update is complete or on-going, retrieve its status */
        ota_flags = ota_ctx.valid_image == true ? ESP_OTA_IMG_NEW : ESP_OTA_IMG_INVALID;
    } else {
        esp_err_t ret = aws_esp_ota_get_boot_flags(&ota_flags, true);

        if (ret != ESP_OK) {
            LogError(("Failed to get ota flags %d", ret));
            return eImageState;
        }
    }

    switch (ota_flags) {
    case ESP_OTA_IMG_PENDING_VERIFY:
        /* Pending Commit means we're in the Self Test phase. */
        eImageState = OtaPalImageStatePendingCommit;
        break;

    case ESP_OTA_IMG_VALID:
    case ESP_OTA_IMG_NEW:
        eImageState = OtaPalImageStateValid;
        break;

    default:
        eImageState = OtaPalImageStateInvalid;
        break;
    }

    return eImageState;
}

static void disable_rtc_wdt() {
    LogInfo(("Disabling RTC hardware watchdog timer"));

    wdt_hal_context_t rtc_wdt_ctx =
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 0)
        RWDT_HAL_CONTEXT_DEFAULT();
#else
        {.inst = WDT_RWDT, .rwdt_dev = &RTCCNTL};
#endif

    wdt_hal_write_protect_disable(&rtc_wdt_ctx);
    wdt_hal_disable(&rtc_wdt_ctx);
    wdt_hal_write_protect_enable(&rtc_wdt_ctx);
}

OtaPalStatus_t otaPal_SetPlatformImageState(AfrOtaJobDocumentFields_t *const pFileContext, OtaImageState_t eState) {
    OtaPalStatus_t mainErr = OtaPalSuccess;
    int state;

    (void)pFileContext;

    LogInfo(("%s, %d", __func__, eState));

    switch (eState) {
    case OtaImageStateAccepted:
        LogInfo(("Set image as valid one!"));
        state = ESP_OTA_IMG_VALID;
        break;

    case OtaImageStateRejected:
        LogWarn(("Set image as invalid!"));
        state = ESP_OTA_IMG_INVALID;
        break;

    case OtaImageStateAborted:
        LogWarn(("Set image as aborted!"));
        state = ESP_OTA_IMG_ABORTED;
        break;

    case OtaImageStateTesting:
        LogWarn(("Set image as testing!"));
        return OtaPalSuccess;

    default:
        LogWarn(("Set image invalid state!"));
        return OtaPalBadImageState;
    }

    uint32_t ota_flags;
    /* Get current active (running) firmware image flags */
    esp_err_t ret = aws_esp_ota_get_boot_flags(&ota_flags, true);

    if (ret != ESP_OK) {
        LogError(("Failed to get ota flags %d", ret));
        return OtaPalCommitFailed;
    }

    /* If this is first request to set platform state, post bootup and there is not OTA being
     * triggered yet, then operate on active image flags, else use passive image flags */
    if ((ota_ctx.cur_ota == NULL) && (ota_ctx.data_write_len == 0)) {
        if (ota_flags == ESP_OTA_IMG_PENDING_VERIFY) {
            LogInfo(("Image is pending verification."));
            ret = aws_esp_ota_set_boot_flags(state, true);

            if (ret != ESP_OK) {
                LogError(("Failed to set ota flags %d", ret));
                return OtaPalCommitFailed;
            } else {
                /* RTC watchdog timer can now be stopped */
                disable_rtc_wdt();
            }
        } else {
            LogWarn(("Image not in self test mode %" PRIu32 "", ota_flags));
            mainErr = OtaPalCommitFailed; /*ota_flags == ESP_OTA_IMG_VALID ? OtaPalSuccess : OtaPalCommitFailed; */
        }

        /* For debug purpose only, get current flags */
        aws_esp_ota_get_boot_flags(&ota_flags, true);
    } else {
        if ((eState == OtaImageStateAccepted) && (ota_ctx.valid_image == false)) {
            /* Incorrect update image or not yet validated */
            return OtaPalCommitFailed;
        }

        if (ota_flags != ESP_OTA_IMG_VALID) {
            LogError(("Currently executing firmware not marked as valid, abort"));
            return OtaPalCommitFailed;
        }

        ret = aws_esp_ota_set_boot_flags(state, false);

        if (ret != ESP_OK) {
            LogError(("Failed to set ota flags %d", ret));
            return OtaPalCommitFailed;
        }
    }

    return mainErr;
}

static const esp_partition_t *get_running_firmware(void) {
    const esp_partition_t *configured = esp_ota_get_boot_partition();
    const esp_partition_t *running = esp_ota_get_running_partition();

    ESP_LOGI(TAG, "Running partition type %d subtype %d (offset 0x%08" PRIx32 ")", running->type, running->subtype,
             running->address);
    ESP_LOGI(TAG, "Configured partition type %d subtype %d (offset 0x%08" PRIx32 ")", configured->type,
             configured->subtype, configured->address);
    return running;
}

esp_err_t otaPal_EraseLastBootPartition(void) {
    const esp_partition_t *cur_app = get_running_firmware();

    ESP_LOGI(TAG, "Current running firmware is: %s", cur_app->label);
    return (esp_ota_erase_last_boot_app_partition());
}

bool otaPal_SetCodeSigningCertificate(const char *pcCodeSigningCertificatePEM) {
    int ret;

    // Free any existing key context
    mbedtls_pk_free(&codeSigningKey);
    mbedtls_pk_init(&codeSigningKey);

    // Parse the PEM-formatted public key
    ret = mbedtls_pk_parse_public_key(&codeSigningKey, (const unsigned char *)pcCodeSigningCertificatePEM,
                                      strlen(pcCodeSigningCertificatePEM) + 1);
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to parse public key: %d", ret);
        return false;
    }

    // Validate that the key is an ECDSA key
    if (mbedtls_pk_get_type(&codeSigningKey) != MBEDTLS_PK_ECKEY) {
        ESP_LOGE(TAG, "Expected an ECDSA public key");
        mbedtls_pk_free(&codeSigningKey);
        return false;
    }

    ESP_LOGI(TAG, "ECDSA public key set successfully");
    return true;
}