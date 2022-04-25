# IoT Reference Integration on ESP32-C3 RISC-V MCU

## Introduction

This repository contains a project that demonstrates how to integrate FreeRTOS modular software libraries with the hardware capabilities of the [ESP32-C3 RISC-V MCU](https://www.espressif.com/en/products/socs/esp32-c3) and the enhanced security capabilities provided by the [Digital Signature peripheral](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/api-reference/peripherals/ds.html) and [Secure Boot V2](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/security/secure-boot-v2.html). The project contains reference implementations that demonstrate IoT application tasks which run concurrently and securely communicate with [AWS IoT](https://aws.amazon.com/iot-core/). The implementation also shows how to perform over-the-air firmware updates that leverage the [AWS IoT OTA service](https://docs.aws.amazon.com/freertos/latest/userguide/freertos-ota-dev.html) and secure bootloader capabilities of Secure Boot V2. The reference implementation runs on the [ESP32-C3-DevKitM-1](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/hw-reference/esp32c3/user-guide-devkitm-1.html) IoT development board. For more details on the feature, see the [Featured IoT Reference Integration page for the ESP32-C3](https://www.freertos.org/ESP32C3) on FreeRTOS.org.

## Demos

This repository currently supports 3 demos implemented as FreeRTOS [tasks](https://www.freertos.org/taskandcr.html), each of which utilize the same MQTT connection. The demos use the [coreMQTT](https://www.freertos.org/mqtt/index.html) library, while the [coreMQTT-Agent](https://www.freertos.org/mqtt-agent/index.html) library is employed to manage thread safety for the MQTT connection. The demos are the following:

* ota: This demo uses the [AWS IoT OTA service](https://docs.aws.amazon.com/freertos/latest/userguide/freertos-ota-dev.html) for FreeRTOS to configure and create OTA updates. The OTA client software on the ESP32-C3 uses the [AWS IoT OTA library](https://www.freertos.org/ota/index.html) and runs in the background within a FreeRTOS agent (or daemon) task. A new firmware image is first digitally signed and uploaded to the OTA service, and the device is then configured to securely store the corresponding public key certificate in encrypted flash. The demo subscribes to and listens on an OTA job topic in order to be notified of an OTA update. Upon receiving notification of a pending OTA update, the device downloads the firmware patch and performs code signature verification of the downloaded image by using the public key certificate stored in encrypted flash. On successful verification, the device reboots and the updated image is activated. The OTA client then performs a self-test on the updated image to check for its integrity.
* sub_pub_unsub_demo: This demo performs a simple and continuous cycle of publish, subscribe and unsubscribe of a message to an AWS IoT Core MQTT topic. The demo creates a task which subscribes to a topic on AWS IoT Core, publishes a constant string to the same topic, receives its own publish (since the device is subscribed to the topic it published to), and then unsubscribes from the topic. All this is done in a loop.
* temp_sub_pub_demo: This demo continously reads the temperature from the onboard temperature sensor and sends the readings to IoT Core by publishing the data to an AWS IoT Core MQTT topic.

All three demos can be selected to run together concurrently as separate tasks.

## Cloning the Repository

To clone using HTTPS:

```
 git clone https://github.com/FreeRTOS/iot-reference-esp32c3.git --recurse-submodules
 git submodule update --init --recursive
```

Using SSH:

```
 git clone git@github.com:FreeRTOS/iot-reference-esp32c3.git --recurse-submodules
```

If you have downloaded the repo without using the --recurse-submodules argument, you should run:

```
git submodule update --init --recursive
```

## Getting started with the demos

To get started and run the demos, follow the [Getting Started Guide](GettingStartedGuide.md).

## Security

See [CONTRIBUTING](CONTRIBUTING.md#security-issue-notifications) for more information.


## License

Example source code under ./main/ is licensed under the MIT-0 License. See the [LICENSE](LICENSE) file. For all other source code licenses including components/, see source header documentation.