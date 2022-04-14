# IoT Reference Integration on ESP32-C3 RISC-V MCU

## Introduction
This repo contains an ESP-IDF project that serves as a reference example for partners looking to develop a well-architected application that is designed to connect to AWS IoT Core and make use of related services in the best possible manner.  

The example shows the use of popular and important [AWS IoT LTS libraries](https://github.com/espressif/esp-aws-iot/tree/master/libraries) by means of a simple example designed to run on the ESP32-C3 which periodically publishes the temperature sensor data to AWS IoT Core and allows the WS2812 LED on the ESP32-C3 DevKit to be controlled using a JSON payload that can be published over MQTT.

This example can be used as a boilerplate to create a production-ready application.

## Features
1. Secure and Simplified provisioning: Provisioning is based on Espressif's [Unified Provisioning API](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/api-reference/provisioning/provisioning.html) and can be carried out using Espressif's open source provisioning apps, available on the Google Play Store for Android, and the Apple App Store for iOS and iPadOS.  
[Google Play Store link](https://play.google.com/store/apps/details?id=com.espressif.provble)  
[Apple App Store link](https://apps.apple.com/app/esp-ble-provisioning/id1473590141)  

Both the apps are open source, and the source code for the apps is available on GitHub.  
[Android Provisioning App on GitHub](https://github.com/espressif/esp-idf-provisioning-android)  
[iOS/iPadOS Provisioning App on GitHub](https://github.com/espressif/esp-idf-provisioning-ios)  

2. Libraries used: coreMQTT-Agent, coreJSON and the AWS OTA library.  

3. Sensors and other hardware utilised: Temperature sensor and LED.  

4. Hardware root of trust for secure storage of credentials and certificates.  

## Getting Started

### Setting up your hardware
Follow the hardware setup guide given [here](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/get-started/index.html#hardware) for the ESP32-C3.

### Setting up the development environment
Follow the software setup guide given [here](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/get-started/index.html#software) for the ESP32-C3 to setup your development environment.

> Please ensure that you are able to build and run the `hello_world` example on your board and then continue. 

## Cloning the Repository
Run the following commands to correctly clone the repository:
```
 git clone https://github.com/FreeRTOS/lab-iot-reference-esp32c3.git
 git submodule update --init --recursive
```
## Running the demo

### Provisioning/Configuration
There are some additional steps that need to be run before you can build this example.
The [Getting Started section of the AWS IoT Developer Guide](http://docs.aws.amazon.com/iot/latest/developerguide/iot-gs.html) lays out the steps to get started with AWS IoT.
To build and use this example, follow all the AWS IoT Getting Started steps from the beginning ("Sign in to the AWS Iot Console") up until "Configuring Your Device". For configuring the device, these are the steps:

### Authentication (Based on X.509 certificates)

#### Server Authentication

Server certificates allow devices to verify that they're communicating with AWS IoT and not another server impersonating AWS IoT. By default [Amazon Root CA 1](https://www.amazontrust.com/repository/AmazonRootCA1.pem) (signed by Amazon Trust Services Endpoints CA) is embedded in applications, for more information please refer to https://docs.aws.amazon.com/iot/latest/developerguide/managing-device-certs.html#server-authentication

#### Device Authentication

AWS IoT can use AWS IoT-generated certificates or certificates signed by a CA certificate for device authentication. To use a certificate that is not created by AWS IoT, you must register a CA certificate. All device certificates must be signed by the CA certificate you register. Please refer to guide at https://docs.aws.amazon.com/iot/latest/developerguide/device-certs-your-own.html for step-by-step instructions to register custom X.509 certificates.  

##### Programming Certificates for Device Authentication

The example provided supports two ways of accessing certificates required for device identity:  
1. Using the [Digital Signature Peripheral](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/api-reference/peripherals/ds.html) on the ESP32-C3
2. Using hardcoded certificates directly embedded in the App Binary

> While use of the DS Peripheral is the recommended and most secure way for handling certificates in a production environment, the default developer-friendly workflow is the use of hardcoded certificates.  
> Steps for configuring and using the DS Peripheral can be found in the [Security](#security) section. 

#### Hardcoded Private Key & Certificate

As part of creating a device certificate, you downloaded a Private Key (`xxx-private.pem.key`) and a Certificate file (`xxx-certificate.pem.crt`). These keys need to be loaded by the ESP32-C3 to identify itself.

#### Embedded Key & Cert into App Binary

Copy the `.pem.key` and `.pem.crt` files to the `certs` directory of the example. Rename them by removing the device-specific prefix - the new names are `client.key` and `client.crt`.

```
mv path/to/xxx-private.pem.key main/certs/client.key
mv path/to/xxx-certificate.pem.crt main/certs/client.crt
```

As these files are bound to your AWS IoT account, take care not to accidentally commit them to public source control.

### Steps to run the demo
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

## Security
1. Steps to enable Flash Encryption on ESP32-C3 can be found [here](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/security/flash-encryption.html).
2. Steps to enable Secure Boot on ESP32-C3 can be found [here](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/security/secure-boot-v2.html).
3. The Digital Signature Peripheral on the ESP32-C3 can operate on encrypted private key directly, restricting software access to device identity.  
The DS Peripheral can be provisioned using the `configure_ds.py` script available in the `components/esp_secure_cert_mgr/tools/` directory and following the steps listed in the `README.md` in the same directory.  
Once you have configured the DS Peripheral and verified it by running the `components/esp_secure_cert_mgr/esp_secure_cert_app` application, simply choose the `Use DS peripheral` option in `idf.py menuconfig`.

```
Golden Reference Integration
└── PKI credentials access method
    └── Use DS peripheral
```

See [CONTRIBUTING](CONTRIBUTING.md#security-issue-notifications) for more information.

## Debugging
Follow the debugging guide of the ESP32-C3 given [here](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/api-guides/jtag-debugging/index.html).

## Troubleshooting
* If you are not able to establish USB serial connection, you will need to download the drivers and check the additional information for your operating system in the [Establish Serial Connection](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/get-started/establish-serial-connection.html) guide.

* Raise the ESP debug log level to Debug in order to see messages about the connection to AWS, certificate contents, etc.

* Enable mbedTLS debugging (under Components -> mbedTLS -> mbedTLS Debug) in order to see even more low-level debug output from the mbedTLS layer.

* To create a successful AWS IoT connection, the following factors must all be present:
  - Endpoint hostname is correct for your AWS account.
  - Certificate & private key are both attached to correct Thing in AWS IoT Console.
  - Certificate is activated.
  - Policy is attached to the Certificate in AWS IoT Console.
  - Policy contains sufficient permissions to authorize AWS IoT connection.

* If your TLS connection fails entirely

If connecting fails entirely (handshake doesn't complete), this usually indicates a problem with certification configuration. The error usually looks like this:

```
failed! mbedtls_ssl_handshake returned -0x7780
```

(0x7780 is the mbedTLS error code when the server sends an alert message and closes the connection.)

* Check your client private key and certificate file match a Certificate registered and **activated** in AWS IoT console. You can find the Certificate in IoT Console in one of two ways, via the Thing or via Certificates:
  - To find the Certificate directly, click on "Registry" -> "Security Certificates". Then click on the Certificate itself to view it.
  - To find the Certificate via the Thing, click on "Registry" -> "Things", then click on the particular Thing you are using. Click "Certificates" in the sidebar to view all Certificates attached to that Thing. Then click on the Certificate itself to view it.

Verify the Certificate is activated (when viewing the Certificate, it will say "ACTIVE" or "INACTIVE" near the top under the certificate name).

If the Certificate appears correct and activated, verify that you are connecting to the correct AWS IoT endpoint (see above.)

* If your TLS connection closes immediately

Sometimes connecting is successful (the handshake completes) but as soon as the client sends its `MQTT CONNECT` message the server sends back a TLS alert and closes the connection, without anything else happening.

The error returned from AWS IoT is usually -28 (`MQTT_REQUEST_TIMEOUT_ERROR`). You may also see mbedtls error `-0x7780` (server alert), although if this error comes during `mbedtls_ssl_handshake` then it's usually a different problem (see above).

This error implies the Certificate is recognised, but the Certificate is either missing the correct Thing or the correct Policy attached to it.

* Check in the AWS IoT console that your certificate is activated and has both a **security policy** and a **Thing** attached to it. You can find this in IoT Console by clicking "Registry" -> "Security Certificates", then click the Certificate. Once viewing the Certificate, you can click the "Policies" and "Things" links in the sidebar.

## License

Example source code under ./main/ are licensed under the MIT-0 License. See the LICENSE file. For all other source code licenses including components/, see source header documentation.
