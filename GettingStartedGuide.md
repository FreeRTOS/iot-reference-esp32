# Getting Started

## 1 Prerequisites

### 1.1 Hardware Requirements

* Micro USB cable.
* ESP32-C3 board (e.g [ESP32-C3-DevKitC-02](https://www.mouser.com/ProductDetail/Espressif-Systems/ESP32-C3-DevKitC-02?qs=stqOd1AaK7%2F1Q62ysr4CMA%3D%3D)).
* Personal Computer with Linux, macOS, or Windows.
* WiFi access point with access to the internet.

### 1.2 Software Requirements

* ESP-IDF 4.4 or higher to configure, build, and flash project. To setup for the ESP32-C3, follow Espressif's [Getting Started Guide for the ESP32-C3](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/get-started/index.html).
* [Python3](https://www.python.org/downloads/)
    and the Package Installer for Python [pip](https://pip.pypa.io/en/stable/installation/)
    to use the AWS CLI to import certificates and perform OTA Job set up. Pip is included when you install
    from Python 3.10.
* [OpenSSL](https://www.openssl.org/) to create the OTA signing
    key and certificate. If you have git installed on your machine, you can also use the openssl.exe
    that comes with the git installation.
* [AWS CLI Interface](https://docs.aws.amazon.com/cli/latest/userguide/getting-started-install.html)
    to import your code-signing certificate, private key, and certificate chain into the AWS Certificate Manager,
    and used for OTA firmware update job set up. Refer to
    [Installing or updating the latest version of the AWS CLI](https://docs.aws.amazon.com/cli/latest/userguide/getting-started-install.html)
    for installation instructions. After installation, follow the steps in
    [Configuration basics](https://docs.aws.amazon.com/cli/latest/userguide/cli-configure-quickstart.html)
    to configure the basic settings (security credentials, the default AWS output format and the default AWS Region)
    that AWS CLI uses to interact with AWS.

## 2 Setup

### 2.1 Setting up AWS IoT Core

To setup AWS IoT Core, follow the [AWS IoT Core Setup Guide](AWSSetup.md). This guide goes through signing up for an AWS account, setting up an IAM account, which can be used with the AWS CLI, and registering your device for this project to AWS IoT Core.

### 2.2 Provisioning the ESP32-C3

To provision the ESP32-C3 for this project, you must have:

* An **AWS device endpoint**: This is the device endpoint for your AWS account.
* A **thing name:** This is the name of the device thing registered on your AWS account.
* A **PEM-encoded device certificate:** This is a certificate signed by the Amazon Root CA attached to the thing.
* A **PEM-encoded private key:** This is the private key corresponding to the device certificate.
* A **PEM-encoded root CA certificate:** This is the Amazon Root CA which can be downloaded [here](https://www.amazontrust.com/repository/AmazonRootCA1.pem).

Additionally, if you plan on using OTA, you must have:

* A **PEM-encoded code signing certificate**: This is the code signing certifcate the OTA demo will use to verify the signature of an OTA update.

#### 2.2.1 Provisioning Thing Name and AWS Device Endpoint

1. Open the ESP-IDF menuconfig.
    1. **Terminal/Command Prompt users**
        1. Open the ESP-IDF Terminal/Command Prompt
        2. Set the directory to the root of this project.
        3. Run `idf.py menuconfig`.
    2. **Visual Studio Code users**
        1. Open this project in Visual Studio Code with the Espressif IDF extension.
        2. Click **View** at the top.
        3. Click **Command Palette** in the dropdown menu.
        4. Search for `ESP-IDF: SDK Configuration editor (menuconfig)` and select the command.
        5. The `SDK Configuration editor` window should pop up after a moment.
2. Select `Golden Reference Integration` from the menu.
3. Set `Endpoint for MQTT Broker to use` to your **AWS device endpoint**.
4. Set `Port for MQTT Broker to use` to `8883`.
5. Set `Thing name` to the **thing name**.

#### 2.2.2 Provisioning Keys and Certificates

For provisioning keys and certificates, this project utilizes the [ESP Secure Certificate Manager](https://github.com/espressif/esp_secure_cert_mgr). This requires that a partition on the ESP32-C3 be written to with these credentials. To generate and write this partition, follow these steps:

1. Open the ESP-IDF Terminal/Command Prompt
    1. **Visual Studio Code users**
        1. Click **View** at the top.
        2. Click **Command Palette** in the dropdown menu.
        3. Search for `ESP-IDF: Open ESP-IDF Terminal` and select the command.
        4. The `ESP-IDF Terminal` will open at the bottom.
2. Set the directory to the root of this project.
3. Create the `esp_secure_crt` partition binary.

    1. If provisioning without the **PEM-encoded code signing certificate**, run the following command:
```
python components/esp_secure_cert_mgr/tools/configure_esp_secure_cert.py -p PORT --ca-cert CA_CERT_FILEPATH --device-cert DEVICE_CERT_FILEPATH --private-key PRIVATE_KEY_FILEPATH --target_chip esp32c3 --secure_cert_type cust_flash
```
Replace:
* **PORT** with the serial port of the ESP32-C3.
* **CA_CERT_FILEPATH** with the file path to the **PEM-encoded root CA certificate**.
* **DEVICE_CERT_FILEPATH** with the file path to the **PEM-encoded device certificate**.
* **PRIVATE_KEY_FILEPATH** with the file path to the **PEM-encoded private key**.

    2. If provisioning with the **PEM-encoded code signing certificate**, run the following command:
```
python components/esp_secure_cert_mgr/tools/configure_esp_secure_cert.py -p PORT --ca-cert CA_CERT_FILEPATH --device-cert DEVICE_CERT_FILEPATH --private-key PRIVATE_KEY_FILEPATH --cs-cert CS_CERT_FILEPATH --target_chip esp32c3 --secure_cert_type cust_flash
```
Replace:
* **PORT** with the serial port of the ESP32-C3.
* **CA_CERT_FILEPATH** with the file path to the **PEM-encoded root CA certificate**.
* **DEVICE_CERT_FILEPATH** with the file path to the **PEM-encoded device certificate**.
* **PRIVATE_KEY_FILEPATH** with the file path to the **PEM-encoded private key**.
* **CS_CERT_FILEPATH** with the file path to the **PEM-encoded code signing certificate**.

4. Write the `esp_secure_crt` partition binary (stored in `esp_ds_data/esp_secure_crt.bin`) to the ESP32-C3's flash by running the following command:
```
esptool.py --no-stub --port *PORT* write_flash 0xD000 esp_ds_data/esp_secure_cert.bin
```
Replace:
* **PORT** with the serial port of the ESP32-C3.

**NOTE:** These steps do **NOT** provision the ESP32-C3 in a secure way. For increasing the security of this project, see the [Security Guide](SecurityGuide.md), though it is recommended that you only follow these steps when you are ready for production.

### 3 Configuring Demos

This repository currently supports 3 demos implemented as FreeRTOS tasks, each of which utilize the same MQTT connection managed by the coreMQTT-Agent library for thread-safety. The demos are the following:

* **Over-The-Air update demo:** This demo has the user create an OTA job on AWS IoT for their device and watch as it downloads the updated firmware, and reboot with the updated firmware.
* **SubPubUnsub demo:** This demo creates tasks which Subscribe to a topic on AWS IoT Core, Publish to the same topic, receive their own publish since the device is subscribed to the topic it published to, then Unsubscribe from the topic in a loop.
* **TempPubSub and LED control demo:** This demo utilizes the temperature sensor to send temperature readings to IoT Core, and allows the user to send JSON payloads back to the device to control it's LED.

**NOTE:** By default, all 3 demos are enabled and will run concurrently with each other, and, thus, these configurations are optional.

To configure the demos:

1. Open the ESP-IDF menuconfig.
    1. **Terminal/Command Prompt users**
        1. Open the ESP-IDF Terminal/Command Prompt
        2. Set the directory to the root of this project.
        3. Run `idf.py menuconfig`.
    2. **Visual Studio Code users**
        1. Open this project in Visual Studio Code with the Espressif IDF extension.
        2. Click **View** at the top.
        3. Click **Command Palette** in the dropdown menu.
        4. Search for `ESP-IDF: SDK Configuration editor (menuconfig)` and select the command.
        5. The `SDK Configuration editor` window should pop up after a moment.
2. Select `Golden Reference Integration` from the menu.

From the `Golden Reference Integration` menu, follow the below guides to configure each demo.

#### 3.1 Over-The-Air demo configurations

1. Set `Enable OTA demo` to true.
2. With `Enable OTA demo` set to true, an `OTA demo configurations` menu is revealed.
3. From the `OTA demo configurations` menu, the following options can be set:
    * `Max file path size.`: The maximum size of the file paths used in the demo.
    * `Max stream name size.`: The maximum size of the stream name required for downloading update file from streaming service.
    * `OTA statistic output delay milliseconds.`: The delay used in the OTA demo task to periodically output the OTA statistics like number of packets received, dropped, processed and queued per connection.
    * `MQTT operation timeout milliseconds.`: The maximum time for which OTA demo waits for an MQTT operation to be complete. This involves receiving an acknowledgment for broker for SUBSCRIBE, UNSUBSCRIBE and non QOS0 publishes.
    * `OTA agent task stack priority.`: The priority of the OTA agent task that runs within the AWS OTA library.
    * `OTA agent task stack size.`: The task size of the OTA agent task that runs within the AWS OTA library.
    * `Application version major.`: The major number of the application version.
    * `Application version minor.`: The minor number of the application version.
    * `Application version build.`: The build number of the application version.

#### 3.2 SubPubUnsub demo configurations

/* TODO */

#### 3.3 TempPubSub and LED control demo configurations

/* TODO */

## 4 Building, flashing, and monitoring the project

### 4.1 Terminal/Command Prompt users

1. Open an ESP-IDF Terminal/Command Prompt.
2. Set the directory to the root of this project.
3. Run the following command:
```
idf.py -p PORT flash monitor
```
Replace:
* **PORT** with the serial port of the ESP32-C3.

### 4.2 Visual Studio Code users

1. Open this project in Visual Studio Code with the Espressif IDF extension.
2. Click **View** at the top.
3. Click **Command Palette** in the dropdown menu.
4. Search for `ESP-IDF: Build, Flash and start a monitor on your device` and select the command.

## 5 Interacting with the Demos

### 5.1 Over-The-Air update demo

/* TODO */

### 5.2 SubPubUnsub demo

/* TODO */

### 5.3 TempPubSub and LED control demo

1. Run `idf.py menuconfig` and set the AWS IoT endpoint and Thing Name under `Golden Reference Integration`.

![IDF Menuconfig Screenshot](_static/idf_menuconfig_screenshot.png "IDF Menuconfig Screenshot")

2. This example supports multiple ways to securely store the PKI credentials.
The default method is to use PKI credentials which are embedded in the binary, using the certs from the `certs/` directory. 
3. Run `idf.py build flash monitor -p <UART port>` to build, flash and start the serial console.
4. Subscribe to the `/filter/Publisher0` topic and check if you are getting JSON messages as follows:
```json
{
  "temperatureSensor": {
    "taskName": "Publisher0",
    "temperatureValue": 36.064602,
    "iteration": 1
  }
}
```
5. To change the LED power state, publish the following JSON payload on the same `/filter/Publisher0` topic:
```json
{
    "led":
    {
        "power": 1
    }
}
```