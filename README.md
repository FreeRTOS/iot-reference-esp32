# IoT Reference Integration on ESP32-C3 RISC-V MCU

## Introduction

This repository contains a project that demonstrates how to integrate FreeRTOS modular software libraries with the hardware capabilities of the [ESP32-C3 RISC-V MCU](https://www.espressif.com/en/products/socs/esp32-c3) and the enhanced security capabilities provided by the [Digital Signature peripheral](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/api-reference/peripherals/ds.html) and [Secure Boot V2](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/security/secure-boot-v2.html). The project contains reference implementations that show different IoT application tasks that run concurrently and securely communicate with [AWS IoT](https://aws.amazon.com/iot-core/). The implementation also shows how to perform over-the-air firmware updates that leverage the [AWS IoT OTA service](https://docs.aws.amazon.com/freertos/latest/userguide/freertos-ota-dev.html) and secure bootloader capabilities of Secure Boot V2. The reference implementation runs on the [ESP32-C3-DevKitM-1](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/hw-reference/esp32c3/user-guide-devkitm-1.html) IoT development board. For more details on the feature, see the [Featured IoT Reference Integration page for the ESP32-C3](https://www.freertos.org/ESP32C3) on FreeRTOS.org.

## Demos

This repository currently supports 3 demos implemented as FreeRTOS tasks, each of which utilize the same MQTT connection managed by the coreMQTT-Agent library for thread-safety. The demos are the following:

* ota: This demo has the user create an OTA job on AWS IoT for their device and watch as it downloads the updated firmware, and reboot with the updated firmware.
* sub_pub_unsub_demo: This demo creates tasks which Subscribe to a topic on AWS IoT Core, Publish to the same topic, receive their own publish since the device is subscribed to the topic it published to, then Unsubscribe from the topic in a loop.
* temp_sub_pub_demo: This demo utilizes the temperature sensor to send temperature readings to IoT Core, and allows the user to send JSON payloads back to the device to control it's LED.

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

## Getting started and running the demos

To get started and run the demos, follow the [Getting Started Guide](GettingStartedGuide.md).

## Security

See [CONTRIBUTING](CONTRIBUTING.md#security-issue-notifications) for more information.


## License

Example source code under ./main/ is licensed under the MIT-0 License. See the LICENSE file. For all other source code licenses including components/, see source header documentation.