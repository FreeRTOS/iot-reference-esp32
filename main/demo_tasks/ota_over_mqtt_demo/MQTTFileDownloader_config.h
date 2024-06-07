/*
 * AWS IoT Core MQTT File Streams Embedded C v1.1.0
 * Copyright (C) 2023 Amazon.com, Inc. and its affiliates. All Rights Reserved.
 * SPDX-License-Identifier: MIT
 *
 * Licensed under the MIT License. See the LICENSE accompanying this file
 * for the specific language governing permissions and limitations under
 * the License.
 */

/**
 * @file MQTTFileDownloader_config.h
 * @brief Configs for MQTT stream.
 */

#ifndef MQTT_FILE_DOWNLOADER_CONFIG_H
#define MQTT_FILE_DOWNLOADER_CONFIG_H

/**
 * Configure the Maximum size of the data payload. The smallest value is 256 bytes,
 * maximum is 128KB.
 */
#ifndef mqttFileDownloader_CONFIG_BLOCK_SIZE
#define mqttFileDownloader_CONFIG_BLOCK_SIZE    512U
#endif

#endif /* #ifndef MQTT_FILE_DOWNLOADER_DEFAULT_H */