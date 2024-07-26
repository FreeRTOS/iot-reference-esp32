# Changelog for ESP32-C3 MCU Featured FreeRTOS IoT Integration

## v202407.00 ( July 2024 )
- [#88](https://github.com/FreeRTOS/iot-reference-esp32c3/pull/88) Update Long Term Support (LTS) libraries to 202406.01-LTS:
  * [coreMQTT v2.3.1](https://github.com/FreeRTOS/coreMQTT/blob/v2.3.1)
  * [coreHTTP v3.1.1](https://github.com/FreeRTOS/coreHTTP/tree/v3.1.1)
  * [corePKCS11 v3.6.1](https://github.com/FreeRTOS/corePKCS11/tree/v3.6.1)
  * [coreJSON v3.3.0](https://github.com/FreeRTOS/coreJSON/tree/v3.3.0)
  * [backoffAlgorithm v1.4.1](https://github.com/FreeRTOS/backoffAlgorithm/tree/v1.4.1)
  * [AWS IoT Device Shadow v1.4.1](https://github.com/aws/Device-Shadow-for-AWS-IoT-embedded-sdk/tree/v1.4.1)
  * [AWS IoT Device Defender v1.4.0](https://github.com/aws/Device-Defender-for-AWS-IoT-embedded-sdk/tree/v1.4.0)
  * [AWS IoT Jobs v1.5.1](https://github.com/aws/Jobs-for-AWS-IoT-embedded-sdk/tree/v1.5.1)
  * [AWS MQTT file streams v1.1.0](https://github.com/aws/aws-iot-core-mqtt-file-streams-embedded-c/tree/v1.1.0)

- [#79](https://github.com/FreeRTOS/iot-reference-esp32c3/pull/79) Fix out of order PUBACK and PUBLISH handling
- [#71](https://github.com/FreeRTOS/iot-reference-esp32c3/pull/71) Update Security Feature guide to cover ESP-IDF latest version changes
- [#77](https://github.com/FreeRTOS/iot-reference-esp32c3/pull/77) Notify other tasks that OTA is stopped when fail to activate new image
- [#76](https://github.com/FreeRTOS/iot-reference-esp32c3/pull/76) Post OTA_STOPPED_EVENT once new image verification finished
- [#68](https://github.com/FreeRTOS/iot-reference-esp32c3/pull/68) Shrink idle and timer task stack and OTA buffers to fit into mimimal size when using ESP IDF v5.1
- [#66](https://github.com/FreeRTOS/iot-reference-esp32c3/pull/66) Add C linage for C++ support
- [#64](https://github.com/FreeRTOS/iot-reference-esp32c3/pull/64) Fix GPIO level in temperature sensor pub sub and LED control demo
- [#57](https://github.com/FreeRTOS/iot-reference-esp32c3/pull/57) Add matrix build for supported targets
- [#43](https://github.com/FreeRTOS/iot-reference-esp32c3/pull/43) Add process loop call in MQTT Agent manager to fix TLS connection dropped.
- [#20](https://github.com/FreeRTOS/iot-reference-esp32c3/pull/20) Updated esp_secure_cert_mgr and IDF v5.0 support

## v202212.00 ( December 2022 )
- [#12](https://github.com/FreeRTOS/iot-reference-esp32c3/pull/12) Update Long Term Support (LTS) libraries to 202210.01-LTS:
  * [coreMQTT v2.1.1](https://github.com/FreeRTOS/coreMQTT/blob/v2.1.1/CHANGELOG.md)
  * [coreHTTP v3.0.0](https://github.com/FreeRTOS/coreHTTP/tree/v3.0.0)
  * [corePKCS11 v3.5.0](https://github.com/FreeRTOS/corePKCS11/tree/v3.5.0)
  * [coreJSON v3.2.0](https://github.com/FreeRTOS/coreJSON/tree/v3.2.0)
  * [backoffAlgorithm v1.3.0](https://github.com/FreeRTOS/backoffAlgorithm/tree/v1.3.0)
  * [AWS IoT Device Shadow v1.3.0](https://github.com/aws/Device-Shadow-for-AWS-IoT-embedded-sdk/tree/v1.3.0)
  * [AWS IoT Device Defender v1.3.0](https://github.com/aws/Device-Defender-for-AWS-IoT-embedded-sdk/tree/v1.3.0)
  * [AWS IoT Jobs v1.3.0](https://github.com/aws/Jobs-for-AWS-IoT-embedded-sdk/tree/v1.3.0)
  * [AWS IoT Over-the-air Update v3.4.0](https://github.com/aws/ota-for-aws-iot-embedded-sdk/tree/v3.4.0)

- [#5](https://github.com/FreeRTOS/iot-reference-esp32c3/pull/5) and [#10](https://github.com/FreeRTOS/iot-reference-esp32c3/pull/10) Documentation updates

## v202204.00 ( April 2022 )

This is the first release for the repository.

The repository contains IoT Reference integration projects using the ESP32-C3. This release includes the following examples:
* MQTT Publish/Subscribe/Unsubscribe with OTA capability
