# Getting Started Guide

This guide contains instructions on how to setup, build and run the demo
without use of the security features of the ESP32-C3 i.e. without enabling the
DS peripheral, flash encryption and Secure Boot. The guide is meant to provide the
user with a friendly first-use experience.

Once completed, one can progress to the
[Use Security Features](UseSecurityFeatures.md) guide.

[1 Pre-requisites](#1-pre-requisites)<br>
&emsp;[1.1 Hardware Requirements](#11-hardware-requirements)<br>
&emsp;[1.2 Software Requirements](#12-software-requirements)<br>

[2 Demo setup](#2-demo-setup)<br>
&emsp;[2.1 Setup AWS IoT Core](#21-setup-aws-iot-core)<br>
&emsp;[2.2 Configure the project with the AWS IoT Thing Name and AWS device Endpoint](#22-configure-the-project-with-the-aws-iot-thing-name-and-aws-device-endpoint)<br>
&emsp;[2.3 Provision the ESP32-C3 with the private key, device certificate and CA certificate in Development Mode](#23-provision-the-esp32-c3-with-the-private-key-device-certificate-and-ca-certificate-in-development-mode)<br>

[3 Build and flash the demo project](#3-build-and-flash-the-demo-project)<br>

[4 Monitoring the demo](#4-monitoring-the-demo)<br>

[5 Perform firmware Over-the-Air Updates with AWS IoT](#5-perform-firmware-over-the-air-updates-with-aws-iot)<br>
&emsp;[5.1 Setup pre-requisites for OTA cloud resources](#51-setup-pre-requisites-for-ota-cloud-resources)<br>
&emsp;[5.2 Provision the project with the code-signing public key certificate](#52-provision-the-project-with-the-code-signing-public-key-certificate)<br>
&emsp;[5.3 Build an application binary with a higher version number, to be downloaded and activated on the device](#53-build-an-application-binary-with-a-higher-version-number-to-be-downloaded-and-activated-on-the-device)<br>
&emsp;[5.4 Build and flash the device with a binary with a lower version number](#54-build-and-flash-the-device-with-a-binary-with-a-lower-version-number)<br>
&emsp;[5.5 Upload the binary with the higher version number (created in step 5.3) and create an OTA Update Job](#55-upload-the-binary-with-the-higher-version-number-created-in-step-53-and-create-an-ota-update-job)<br>
&emsp;[5.6 Monitor OTA](#56-monitor-ota)<br>

[6 Run FreeRTOS Integration Test](#6-run-freertos-integration-test)<br>
&emsp;[6.1 Prerequisite](#61-prerequisite)<br>
&emsp;[6.2 Steps for each test case](#62-steps-for-each-test-case)<br>

[7 Run AWS IoT Device Tester](#7-run-aws-iot-device-tester)<br>
&emsp;[7.1 Prerequisite](#71-prerequisite)<br>
&emsp;[7.2 Download AWS IoT Device Tester](#72-download-aws-iot-device-tester)<br>
&emsp;[7.3 Configure AWS IoT Device Tester](#73-configure-aws-iot-device-tester)<br>
&emsp;[7.4 Running the FreeRTOS qualification 2.0 suite](#74-running-the-freertos-qualification-20-suite)<br>

## 1 Pre-requisites

### 1.1 Hardware Requirements

- Micro USB cable.
- ESP32-C3, ESP32-C3, or a, ESP32-S3 board (e.g
[ESP32-C3-DevKitC-02](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/hw-reference/esp32c3/user-guide-devkitc-02.html)).
- Personal Computer with Linux, MacOS, or Windows.
- WiFi access point with access to the internet.

### 1.2 Software Requirements

- [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started):
This is used to configure, build, and flash the project.
To setup for the ESP32-C3, follow Espressif's
[Getting Started Guide for the ESP32-C3](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/get-started/index.html).

  - The versions of `ESP-IDF` supported by this repository are the ones supported by
    [Espressif's GitHub Build Action](https://github.com/espressif/esp-idf-ci-action?tab=readme-ov-file)
  - To see the currently supported `ESP-IDF` versions please refer to the list in
    [build.yml](./.github/workflows/build.yml#L22).

    **NOTE:** Please do not submit a bug report due to build errors or demo failures
    when using an unsupported version of `ESP-IDF`.

- [Python3](https://www.python.org/downloads/)
  and the Package Installer for Python [pip](https://pip.pypa.io/en/stable/installation/)
  to use the AWS CLI to import certificates and perform OTA Job set up. Pip is
  included when you install from Python 3.10.
- [OpenSSL](https://www.openssl.org/) to create the OTA signing
  key and certificate. If you have git installed on your machine,
  you can also use the openssl.exe that comes with the git installation.
- [AWS CLI Interface](https://docs.aws.amazon.com/cli/latest/userguide/getting-started-install.html)
  to import your code-signing certificate, private key, and certificate chain
  into the AWS Certificate Manager, and to set up an OTA firmware update job.
  Refer to the AWS User Guide for
  Installing or updating the latest version of the AWS CLI
  [here](https://docs.aws.amazon.com/cli/latest/userguide/getting-started-install.html)
  for installation instructions. After installation, follow the steps in
  [Configuration basics](https://docs.aws.amazon.com/cli/latest/userguide/cli-configure-quickstart.html)
  to configure the basic settings (security credentials, the default AWS output
  format and the default AWS Region) that AWS CLI uses to interact with AWS.
  (If you don't have an AWS account and user, follow steps 1 and 2 in the
  AWS IoT Core Setup Guide below before following the Configuration basics
  for the AWS CLI.)

## 2 Demo setup

### 2.1 Setup AWS IoT Core

To setup AWS IoT Core, follow the [AWS IoT Core Setup Guide](AWSSetup.md).
The guide shows you how to sign up for an AWS account, create a user, and
register your device with AWS IoT Core.

After you have followed the instructions in the AWS IoT Core Setup Guide, you
will have created a **device Endpoint**, an AWS IoT **thing**, a
**PEM-encoded device certificate**, a **PEM-encoded private key**, and a
**PEM-encoded root CA certificate**. (An explanation of these entities is
given in the [AWS IoT Core Setup Guide](AWSSetup.md).)The AWS Root CA
certificate can also be downloaded
[here](https://www.amazontrust.com/repository/AmazonRootCA1.pem).
Your ESP23-C3 board must now be provisioned with these entities in order
for it to connect securely with AWS IoT Core.

### 2.2 Configure the project with the AWS IoT Thing Name and AWS device Endpoint

The demo will connect to the AWS IoT device Endpoint that you configure here.

1. From a terminal/command prompt navigate to the root directory of this repository
1. Run `idf.py --list-targets`
    - The directly supported chips are the `esp32c3`, `esp32s3`, and `esp32c2`
1. Set the corret chip type by running `idf.py set-target <CHIP_TYPE>`;
1. run `idf.py menuconfig`. This assumes the
ESP-IDF environment is exported-- i.e. that export.bat/export.sh, which can be
found under the ESP-IDF directory, has been run, or that you are using the
ESP-IDF command prompt/terminal. For Visual Studio (VS) Code users who are
using the Espressif IDF extension, do ->View->Command Palette->Search for
`ESP-IDF: SDK Configuration editor (menuconfig)` and select the command. The
`SDK Configuration editor` window should pop up after a moment.
    - **Note**: If running menuconfig from within a VS Code command prompt, 'j'
        and 'k' may have to be used in place of the 'up' and 'down' arrow keys.
        Alternately, one can use a command prompt/terminal outside of the VS
        Code editor.
1. Select `Featured FreeRTOS IoT Integration` from the menu.
1. Set `Endpoint for MQTT Broker to use` to your **AWS device Endpoint**.
1. Set `Port for MQTT Broker to use` to `8883`.
1. Set `Thing name` to your **Thing Name**.
1. Go back to main menu, Save and Exit.

### 2.3 Provision the ESP32-C3 with the private key, device certificate and CA certificate in Development Mode

The key and certificates which will be used to establish a secure TLS
connection will be stored in a special flash partition. Run the
following command to create and flash the certificate partition.

The following values will be needed:

- `PORT`: The serial port to which the ESP32-C3 board is connected. You can
refer to this
[guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/establish-serial-connection.html)
for information about finding what value this should be.

- `CA_CERT_FILEPATH`: The file path to the PEM-encoded root CA certificate,

- `DEVICE_CERT_FILEPATH`: The file path to the PEM-encoded device certificate

- `PRIVATE_KEY_FILEPATH`: The file path to the PEM-encoded private key.

- `KEY_ALG_INFO`: The type of key algorithm being used. In the
format Algorithm Size,

  - **NOTE:** If using the
    [AWS IoT Generated Credentials](https://docs.aws.amazon.com/iot/latest/developerguide/device-certs-create.html)
    this value will be `RSA 2048`

```sh
python managed_components/espressif__esp_secure_cert_mgr/tools/configure_esp_secure_cert.py -p PORT --keep_ds_data_on_host --ca-cert CA_CERT_FILEPATH --device-cert DEVICE_CERT_FILEPATH --private-key PRIVATE_KEY_FILEPATH --target_chip CHIP_TYPE --secure_cert_type cust_flash --priv_key_algo KEY_ALG_INFO
```

> **NOTE:** For convenience sake, you could place your key and certificate files under the `main/certs` directory.

## 3 Build and flash the demo project

Before you build and flash the demo project, if you are setting up the ESP32-C3
for the first time, the board will have to be provisioned with Wi-Fi credentials
to be able to use your Wi-Fi network to connect to the internet. This can be done
via BLE or SoftAP. BLE is the default, but can be changed by running
`idf.py menuconfig` then selecting: Featured FreeRTOS IoT Integration
-> Show provisioning QR code -> Provisioning Transport method.

Espressif provides BLE and SoftAP provisioning mobile apps which are available
on the
[Google Play Store](https://play.google.com/store/apps/details?id=com.espressif.provble)
for Android or the
[Apple App Store](https://apps.apple.com/app/esp-ble-provisioning/id1473590141)
for iOS. Download the appropriate app to your phone before proceeding.

Run the following command to build and flash the demo project:

**NOTE** The list of chip types can be found by running `idf.py list-targets`
The directly supported chips are the ones listed in
[build.yml](./.github/workflows/build.yml#L27)

- Please do not submit a bug report due to build errors or demo failures
  when using an unsupported ESP chip.

```sh
idf.py -p PORT flash monitor;
```

Replace **PORT** with the serial port to which the ESP32-C3 is connected.

If you are setting up the ESP32-C3 for the first time, the device will go though
the Wi-Fi provisioning workflow and you will have to use the app you previously
downloaded to scan the QR code and follow the instructions that follow. Once the
device is provisioned successfully with the required Wi-Fi credentials, the
demo will proceed. If previously Wi-Fi provisioned, the device will not go
through the Wi-Fi provisioning workflow again.

**Note**: If the ESP32-C3 was previously Wi-Fi provisioned, and you are on a
different network and wish to re-provision with new network credentials, you
will have to erase the nvs flash partition where the Wi-Fi credentials are
stored, otherwise the device will presume that it has already been provisioned.
In this situation, use the following command to erase the nvs partition.

```sh
parttool.py -p PORT erase_partition --partition-name=nvs
```

## 4 Monitoring the demo

1. On the serial terminal console, confirm that the TLS connection was
successful and that MQTT messages are published.

```c
I (1843) core_mqtt_agent_network_manager: WiFi connected.
I (1843) app_wifi: Connected with IP Address:10.0.0.9
I (1843) esp_netif_handlers: sta ip: 10.0.0.9, mask: 255.255.255.0, gw: 10.0.0.1
I (1863) ota_over_mqtt_demo:  Received: 0   Queued: 0   Processed: 0   Dropped: 0
I (2843) coreMQTT: Packet received. ReceivedBytes=2.
I (2843) coreMQTT: CONNACK session present bit not set.
I (2843) coreMQTT: Connection accepted.
I (2843) coreMQTT: Received MQTT CONNACK successfully from broker.
I (2853) coreMQTT: MQTT connection established with the broker.
I (2863) coreMQTT: Session present: 0

I (2863) core_mqtt_agent_network_manager: coreMQTT-Agent connected.
I (2873) MQTT: coreMQTT-Agent connected.
I (2873) sub_pub_unsub_demo: coreMQTT-Agent connected.
I (2883) temp_sub_pub_demo: coreMQTT-Agent connected.
I (2893) ota_over_mqtt_demo: coreMQTT-Agent connected. Resuming OTA agent.
I (2893) ota_over_mqtt_demo:  Received: 0   Queued: 0   Processed: 0   Dropped: 0
I (2903) sub_pub_unsub_demo: Task "SubPub0" sending subscribe request to coreMQTT-Agent for topic filter: /filter/SubPub0 with id
1
I (3153) coreMQTT: Packet received. ReceivedBytes=3.
I (3153) temp_pub_sub_demo: Received subscribe ack for topic /filter/Publisher0 containing ID 1
I (3163) temp_pub_sub_demo: Sending publish request to agent with message "{"temperatureSensor":{ "taskName": "Publisher0" , "temperatureValue": 24.099600, "iteration": 0}}" on topic "/filter/Publisher0"
I (3183) temp_pub_sub_demo: Task Publisher0 waiting for publish 0 to complete.
```

2. On the AWS IoT console, select "Test" then select "MQTT test client". Under
"Subscribe to a topic", type "#". (# is used to select all topics. You can
also enter a specific topic such as /filter/Publisher0.) Click on "Subscribe",
then confirm that the MQTT messages from the device are received.
3. To change the LED power state, under "Publish to a topic" publish one of
the following JSON payloads to the `/filter/TempSubPubLED` topic:

To turn the LED on:

```json
{
  "led": {
    "power": 1
  }
}
```

To turn the LED off:

```json
{
  "led": {
    "power": 0
  }
}
```

## 5 Perform firmware Over-the-Air Updates with AWS IoT

This demo uses the OTA client library and the AWS IoT OTA service for code
signing and secure download of firmware updates.

### 5.1 Setup pre-requisites for OTA cloud resources

Before you create an OTA job, the following resources are required. This is a
one time setup required for performing OTA firmware updates. Make a note of the
names of the resources you create, as you will need to provide them during
subsequent configuration steps.

- An Amazon S3 bucket to store your updated firmware. S3 is an AWS Service that
enables you to store files in the cloud that can be accessed by you or other
services. This is used by the OTA Update Manager Service to store the firmware
image in an S3 “bucket” before sending it to the device.
[Create an Amazon S3 Bucket to Store Your Update](https://docs.aws.amazon.com/freertos/latest/userguide/dg-ota-bucket.html).

- An OTA Update Service role. By default, the OTA Update Manager cloud service
does not have permission to access the S3 bucket that will contain the firmware
image. An OTA Service Role is required to allow the OTA Update Manager Service
to read and write to the S3 bucket.
[Create an OTA Update Service role](https://docs.aws.amazon.com/freertos/latest/userguide/create-service-role.html).

- An OTA user policy. An OTA User Policy is required to give your AWS account
permissions to interact with the AWS services required for creating an OTA Update.
[Create an OTA User Policy](https://docs.aws.amazon.com/freertos/latest/userguide/create-ota-user-policy.html).
- [Create a code-signing certificate](https://docs.aws.amazon.com/freertos/latest/userguide/ota-code-sign-cert-win.html).
The demos support a code-signing certificate with an ECDSA P-256 key and
SHA-256 hash to perform OTA updates.
- [Grant access to Code Signing for AWS IoT](https://docs.aws.amazon.com/freertos/latest/userguide/code-sign-policy.html).

### 5.2 Provision the project with the code-signing public key certificate

The code-signing public key certificate will be used by the application binary,
i.e. the demo, to authenticate a binary that was downloaded for an update.
(This downloaded firmware would have been signed by the certificate's
corresponding private key.)

Copy the public key certificate that you would have created in the 'Create a
code-signing certificate' step to 'main/certs/aws_codesign.crt'

The demo will read the certificate 'aws_codesign.crt' from your host filesystem
and save it in memory.

### 5.3 Build an application binary with a higher version number, to be downloaded and activated on the device

To perform an OTA firmware update, you must go through these steps:

1. Increment the version of the binary and create the signed binary image.
2. Upload this image to an S3 bucket and create an OTA Update Job on the AWS IoT console.
3. Restore the original version (lower version number) and flash this to the device.

The version of the new image must be later than the current image on the board
or else OTA will not proceed.

The OTA Update Job will send a notification to an MQTT topic that the device
will be listening to. When it receives an OTA update notification, the device
will then start downloading the new firmware.

Create a binary with a higher version number.

1. Run `idf.py menuconfig`.
2. Select `Featured FreeRTOS IoT Integration` from the menu.
3. Under `Enable OTA demo`, go to `OTA demo configurations`.
4. Set the `Application version build` number to '1'.
5. Go back to main menu, Save and exit.
6. Run the following command to only build the demo project.

```
idf.py build
```

If successful, there will be a new binary under the 'build' directory -
build/FeaturedFreeRTOSIoTIntegration.bin. Copy this binary to another
location, otherwise it will be overwritten in the next step.

### 5.4 Build and flash the device with a binary with a lower version number

1. Follow the same steps in 5.3 starting with running idf.py menuconfig, but
this time, set the `Application version build` number to '0'.
2. Build and flash this new application binary with a lower version number.

```sh
idf.py -p PORT flash monitor
```

### 5.5 Upload the binary with the higher version number (created in step 5.3) and create an OTA Update Job

1. In the navigation pane of the AWS IoT console, choose 'Manage', and then choose 'Jobs'.
   Choose 'Create a job'.
1. Next to 'Create a FreeRTOS Over-the-Air (OTA) update job', choose
`Create FreeRTOS OTA update job'. Provide a name for the job and click on 'Next'.
1. You can deploy an OTA update to a single device or a group of devices. Under
  'Devices to update', select the Thing you created earlier. You can find it
  listed under AWS IoT->Manage->Things. If you are updating a group of devices,
  select the check box next to the thing group associated with your devices.
1. Under 'Select the protocol for file transfer', choose 'MQTT'.
1. Under 'Sign and choose your file', choose 'Sign a new file for me'.
1. Under 'Code signing profile', choose 'Create a new profile'.
1. In 'Create a code signing profile':
   1. Type in a name for this profile.
   1. For the Device hardware platform, select: 'ESP32-DevKitC'.
   1. Under Code signing certificate, choose 'Select an existing certificate',
      then choose the certificate that you created with the AWS CLI earlier and
      registered with AWS ACM (you can use the ARN to identify it).
   1. Under 'Path name of code signing certificate on device', enter '/'. (This
      is not applicable for the ESP32-C3 and hence the / is only a filler.)
   1. Click 'Create'. Confirm that the code signing profile was created successfully.
1. Back on the FreeRTOS OTA Job console:
   1. Under 'Code signing profile', select the code signing profile that was
      just created from the drop down list.
   1. Under 'File', choose 'Upload a new file', then click 'Choose file'. A
      file browser pops up. Select the signed binary image with the higher
      version number.
   1. Under 'File upload location in S3', click 'Browse S3', then select the
      S3 bucket that you created earlier for this job. Click 'Choose'.
   1. Under 'Path name of file on device', type 'NA'.
   1. Under 'IAM role for OTA update job', choose the role that you created
      earlier for the OTA update from the drop down list.
   1. Click 'Next', then click on 'Create job'. Confirm that the job was
      created successfully. Note: If this fails to create an OTA job, make
      sure the role for this OTA job update has the correct permissions
      (policies) attached.

### 5.6 Monitor OTA

Once the job is created successfully, the demo should start downloading the
firmware in chunks. For example:

```c
I (196573) ota_over_mqtt_demo: OTA Event processing completed. Freeing the event buffer to pool.
I (196583) AWS_OTA: Current State=[WaitingForFileBlock], Event=[ReceivedFileBlock], New state=[WaitingForFileBlock]
I (196583) ota_over_mqtt_demo:  Received: 160   Queued: 160   Processed: 158   Dropped: 0
I (196603) AWS_OTA: Received valid file block: Block index=157, Size=4096
I (196613) AWS_OTA: Number of blocks remaining: 130
I (196623) ota_over_mqtt_demo: OTA Event processing completed. Freeing the event buffer to pool.
I (196623) AWS_OTA: Current State=[WaitingForFileBlock], Event=[ReceivedFileBlock], New state=[WaitingForFileBlock]
I (196633) AWS_OTA: Received valid file block: Block index=159, Size=4096
I (196653) AWS_OTA: Number of blocks remaining: 129
I (196653) ota_over_mqtt_demo: OTA Event processing completed. Freeing the event buffer to pool.
I (196653) AWS_OTA: Current State=[WaitingForFileBlock], Event=[ReceivedFileBlock], New state=[WaitingForFileBlock]
I (197603) ota_over_mqtt_demo:  Received: 160   Queued: 160   Processed: 160   Dropped: 0
I (198603) ota_over_mqtt_demo:  Received: 160   Queued: 160   Processed: 160   Dropped: 0
```

Once all the firmware image chunks are downloaded and the signature is
validated, the device reboots with the new image. See the OTA section in the
[Featured FreeRTOS IoT Integration page for the ESP32-C3](https://www.freertos.org/featured-freertos-iot-integration-targeting-an-espressif-esp32-c3-risc-v-mcu)
 on FreeRTOS.org for more details.
You can see the new version number of the demo binary in the terminal console
output. Look for the string "Application version"

```c
I (336900) AWS_OTA: Number of blocks remaining: 1
I (336900) ota_over_mqtt_demo: OTA Event processing completed. Freeing the event buffer to pool.
I (336900) AWS_OTA: Current State=[WaitingForFileBlock], Event=[ReceivedFileBlock], New state=[WaitingForFileBlock]
I (336910) AWS_OTA: Received valid file block: Block index=282, Size=704
I (336920) AWS_OTA: Received final block of the update.
I (337450) AWS_OTA: Signature verification succeeded.
I (337450) AWS_OTA: Received entire update and validated the signature.
I (337450) ota_over_mqtt_demo:  Received: 283   Queued: 283   Processed: 282   Dropped: 0
I (338460) ota_over_mqtt_demo:  Received: 283   Queued: 283   Processed: 282   Dropped: 0
I (339460) ota_over_mqtt_demo:  Received: 283   Queued: 283   Processed: 282   Dropped: 0
I (339880) coreMQTT: Publishing message to $aws/things/thing_esp32c3_nonOta/jobs/AFR_OTA-c3-27340/update.

I (340040) coreMQTT: Packet received. ReceivedBytes=2.
I (340050) coreMQTT: Ack packet deserialized with result: MQTTSuccess.
I (340050) coreMQTT: State record updated. New state=MQTTPublishDone.
I (340050) coreMQTT: Packet received. ReceivedBytes=96.
I (340060) coreMQTT: De-serialized incoming PUBLISH packet: DeserializerResult=MQTTSuccess.
I (340070) coreMQTT: State record updated. New state=MQTTPublishDone.
W (340080) core_mqtt_agent_manager: WARN:  Received an unsolicited publish from topic $aws/things/thing_esp32c3_nonOta/jobs/AFR_OTA-c3-27340/update/accepted
I (340070) ota_over_mqtt_demo: Sent PUBLISH packet to broker $aws/things/thing_esp32c3_nonOta/jobs/AFR_OTA-c3-27340/update to broker.


I (340100) ota_over_mqtt_demo: Received OtaJobEventActivate callback from OTA Agent.
I (340110) esp_image: segment 0: paddr=001b0020 vaddr=3c0e0020 size=2ced8h (184024) map
I (340140) esp_image: segment 1: paddr=001dcf00 vaddr=3fc91800 size=03118h ( 12568)
I (340150) esp_image: segment 2: paddr=001e0020 vaddr=42000020 size=d86e8h (886504) map
I (340260) esp_image: segment 3: paddr=002b8710 vaddr=3fc94918 size=0048ch (  1164)
I (340260) esp_image: segment 4: paddr=002b8ba4 vaddr=40380000 size=116dch ( 71388)
I (340280) esp_image: segment 5: paddr=002ca288 vaddr=50000010 size=00010h (    16)
I (340280) esp_image: segment 0: paddr=001b0020 vaddr=3c0e0020 size=2ced8h (184024) map
I (340310) esp_image: segment 1: paddr=001dcf00 vaddr=3fc91800 size=03118h ( 12568)
I (340310) esp_image: segment 2: paddr=001e0020 vaddr=42000020 size=d86e8h (886504) map
I (340430) esp_image: segment 3: paddr=002b8710 vaddr=3fc94918 size=0048ch (  1164)
I (340430) esp_image: segment 4: paddr=002b8ba4 vaddr=40380000 size=116dch ( 71388)
I (340440) esp_image: segment 5: paddr=002ca288 vaddr=50000010 size=00010h (    16)
I (340490) ota_over_mqtt_demo:  Received: 283   Queued: 283   Processed: 283   Dropped: 0
I (341000) wifi:state: run -> init (0)
I (341000) wifi:pm stop, total sleep time: 271785664 us / 337788344 us

W (341000) wifi:<ba-del>idx
W (341000) wifi:<ba-del>idx
I (341000) wifi:new:<6,0>, old:<6,0>, ap:<255,255>, sta:<6,0>, prof:1
I (341010) core_mqtt_agent_manager: WiFi disconnected.
I (341010) app_wifi: Disconnected. Connecting to the AP again...
E (341020) esp-tls-mbedtls: read error :-0x004C:
I (341020) sub_pub_unsub_demo: coreMQTT-Agent disconnected. Preventing coreMQTT-Agent commands from being enqueued.
I (341030) ota_over_mqtt_demo: coreMQTT-Agent disconnected. Suspending OTA agent.
I (341040) core_mqtt_agent_manager: coreMQTT-Agent disconnected.
I (341050) temp_sub_pub_and_led_control_demo: coreMQTT-Agent disconnected. Preventing coreMQTT-Agent commands from being enqueued.
I (341060) wifi:flush txq
I (341060) wifi:stop sw txq
I (341070) wifi:lmac stop hw txq
ESP-ROM:esp32c3-api1-20210207
Build:Feb  7 2021
rst:0x3 (RTC_SW_SYS_RST),boot:0xc (SPI_FAST_FLASH_BOOT)
Saved PC:0x403805d8
0x403805d8: esp_restart_noos_dig at C:/Users/wallit/Work/.espressif/frameworks/esp-idf-v4.4/components/esp_system/esp_system.c:46 (discriminator 1)

SPIWP:0xee
mode:DIO, clock div:1
load:0x3fcd6100,len:0x16b4
load:0x403ce000,len:0x930
load:0x403d0000,len:0x2dac
entry 0x403ce000
I (35) boot: ESP-IDF v4.4 2nd stage bootloader
I (35) boot: compile time 15:33:46
I (35) boot: chip revision: 3
I (37) boot.esp32c3: SPI Speed      : 80MHz
I (41) boot.esp32c3: SPI Mode       : DIO
I (46) boot.esp32c3: SPI Flash Size : 4MB
I (51) boot: Enabling RNG early entropy source...
I (56) boot: Partition Table:
I (60) boot: ## Label            Usage          Type ST Offset   Length
I (67) boot:  0 esp_secure_cert  unknown          3f 06 0000d000 00006000
I (75) boot:  1 nvs              WiFi data        01 02 00013000 00006000
I (82) boot:  2 otadata          OTA data         01 00 00019000 00002000
I (89) boot:  3 phy_init         RF data          01 01 0001b000 00001000
I (97) boot:  4 ota_0            OTA app          00 10 00020000 00190000
I (104) boot:  5 ota_1            OTA app          00 11 001b0000 00190000
I (112) boot:  6 storage          WiFi data        01 02 00340000 00010000
I (119) boot:  7 nvs_key          NVS keys         01 04 00350000 00001000
I (127) boot: End of partition table
I (178) esp_image: segment 0: paddr=001b0020 vaddr=3c0e0020 size=2ced8h (184024) map
I (206) esp_image: segment 1: paddr=001dcf00 vaddr=3fc91800 size=03118h ( 12568) load
I (209) esp_image: segment 2: paddr=001e0020 vaddr=42000020 size=d86e8h (886504) map
I (345) esp_image: segment 3: paddr=002b8710 vaddr=3fc94918 size=0048ch (  1164) load
I (345) esp_image: segment 4: paddr=002b8ba4 vaddr=40380000 size=116dch ( 71388) load
I (364) esp_image: segment 5: paddr=002ca288 vaddr=50000010 size=00010h (    16) load
I (369) boot: Loaded app from partition at offset 0x1b0000
I (369) boot: Disabling RNG early entropy source...
I (384) cpu_start: Pro cpu up.
I (392) cpu_start: Pro cpu start user code
I (393) cpu_start: cpu freq: 160000000
I (393) cpu_start: Application information:
I (395) cpu_start: Project name:     FeaturedFreeRTOSIoTIntegration
I (402) cpu_start: App version:      v202204.00-dirty
I (408) cpu_start: Compile time:     Apr 29 2022 15:33:03
I (414) cpu_start: ELF file SHA256:  5da757c870ca6788...
I (420) cpu_start: ESP-IDF:          v4.4
I (425) heap_init: Initializing. RAM available for dynamic allocation:
I (432) heap_init: At 3FCACB20 len 000134E0 (77 KiB): DRAM
I (438) heap_init: At 3FCC0000 len 0001F060 (124 KiB): STACK/DRAM
I (445) heap_init: At 50000020 len 00001FE0 (7 KiB): RTCRAM
I (452) spi_flash: detected chip: generic
I (456) spi_flash: flash io: dio
I (460) sleep: Configure to isolate all GPIO pins in sleep state
I (467) sleep: Enable automatic switching of GPIO sleep configuration
I (474) coexist: coexist rom version 9387209
I (479) cpu_start: Starting scheduler.
I (484) main:
...
I (884) app_driver: Initializing Temperature sensor
I (914) ota_over_mqtt_demo: OTA over MQTT demo, Application version 0.0.1
I (924) temp_sub_pub_and_led_control_demo: Sending subscribe request to agent for topic filter: /filter/TempSubPubLED with id 1
I (934) AWS_OTA: otaPal_GetPlatformImageState
I (944) esp_ota_ops: aws_esp_ota_get_boot_flags: 1
I (944) esp_ota_ops: [1] aflags/seq:0x1/0x2, pflags/seq:0x2/0x1
I (954) AWS_OTA: Current State=[RequestingJob], Event=[Start], New state=[RequestingJob]
I (964) ota_over_mqtt_demo:  Received: 0   Queued: 0   Processed: 0   Dropped: 0
I (934) pp: pp rom version: 9387209
I (974) net80211: net80211 rom version: 9387209
I (994) wifi:wifi driver task: 3fcbc2c0, prio:23, stack:6656, core=0
I (994) system_api: Base MAC address is not set
I (994) system_api: read default base MAC address from EFUSE
I (1004) wifi:wifi firmware version: 7679c42
I (1004) wifi:wifi certification version: v7.0
I (1004) wifi:config NVS flash: enabled
I (1004) wifi:config nano formating: disabled
I (1014) wifi:Init data frame dynamic rx buffer num: 32
I (1014) wifi:Init management frame dynamic rx buffer num: 32
I (1024) wifi:Init management short buffer num: 32
I (1024) wifi:Init dynamic tx buffer num: 32
I (1034) wifi:Init static tx FG buffer num: 2
I (1034) wifi:Init static rx buffer size: 1600
I (1044) wifi:Init static rx buffer num: 10
I (1044) wifi:Init dynamic rx buffer num: 32
I (1044) wifi_init: rx ba win: 6
I (1054) wifi_init: tcpip mbox: 32
I (1054) wifi_init: udp mbox: 6
I (1064) wifi_init: tcp mbox: 6
I (1064) wifi_init: tcp tx win: 5744
I (1064) wifi_init: tcp rx win: 5744
I (1074) wifi_init: tcp mss: 1440
I (1074) wifi_init: WiFi IRAM OP enabled
I (1084) wifi_init: WiFi RX IRAM OP enabled
W (1084) BTDM_INIT: esp_bt_mem_release not implemented, return OK
I (1094) wifi_prov_scheme_ble: BT memory released
I (1094) app_wifi: Already provisioned, starting Wi-Fi STA
W (1104) BTDM_INIT: esp_bt_mem_release not implemented, return OK
I (1114) wifi_prov_scheme_ble: BTDM memory released
I (1114) phy_init: phy_version 907,3369105-dirty,Dec  3 2021,14:55:12
I (1164) wifi:mode : sta (84:f7:03:5f:f1:40)
I (1164) wifi:enable tsf
I (1164) wifi:new:<6,0>, old:<1,0>, ap:<255,255>, sta:<6,0>, prof:1
I (1164) wifi:state: init -> auth (b0)
I (1164) wifi:state: auth -> assoc (0)
I (1174) wifi:state: assoc -> run (10)
W (1184) wifi:<ba-add>idx:0 (ifx:0, 8c:6a:8d:fc:31:8e), tid:0, ssn:0, winSize:64
I (1194) wifi:connected with Stranger 5, aid = 12, channel 6, BW20, bssid = 8c:6a:8d:fc:31:8e
I (1194) wifi:security: WPA2-PSK, phy: bgn, rssi: -55
I (1194) wifi:pm start, type: 1

I (1194) wifi:set rx beacon pti, rx_bcn_pti: 14, bcn_timeout: 14, mt_pti: 25000, mt_time: 10000
W (1204) wifi:<ba-add>idx:1 (ifx:0, 8c:6a:8d:fc:31:8e), tid:6, ssn:0, winSize:64
I (1284) wifi:BcnInt:102400, DTIM:1
I (1924) core_mqtt_agent_manager: WiFi connected.
I (1924) app_wifi: Connected with IP Address:10.0.0.140
I (1924) esp_netif_handlers: sta ip: 10.0.0.140, mask: 255.255.255.0, gw: 10.0.0.1
I (1974) ota_over_mqtt_demo:  Received: 0   Queued: 0   Processed: 0   Dropped: 0
I (3104) ota_over_mqtt_demo:  Received: 0   Queued: 0   Processed: 0   Dropped: 0
I (3254) coreMQTT: Packet received. ReceivedBytes=2.
I (3254) coreMQTT: CONNACK session present bit not set.
I (3254) coreMQTT: Connection accepted.
I (3254) coreMQTT: Received MQTT CONNACK successfully from broker.
I (3264) coreMQTT: MQTT connection established with the broker.
I (3274) core_mqtt_agent_manager: Session present: 0

I (3274) sub_pub_unsub_demo: coreMQTT-Agent connected.
I (3284) ota_over_mqtt_demo: coreMQTT-Agent connected. Resuming OTA agent.
I (3294) core_mqtt_agent_manager: coreMQTT-Agent connected.
I (3294) temp_sub_pub_and_led_control_demo: coreMQTT-Agent connected.
I (3304) sub_pub_unsub_demo: Task "SubPub0" sending subscribe request to coreMQTT-Agent for topic filter: /filter/SubPub0 with id 1
I (3444) coreMQTT: Packet received. ReceivedBytes=3.
I (3444) ota_over_mqtt_demo: Subscribed to topic $aws/things/thing_esp32c3_nonOta/jobs/notify-next.
```

## 6 Run FreeRTOS Integration Test

### 6.1 Prerequisite

- Follow the
[OTA update with AWS IoT Guide](#5-perform-firmware-over-the-air-updates-with-aws-iot)
 to create an OTA update and verify the digital signature, checksum and version
number of the new image. If firmware update is verified, you can run the tests on your device.
- Run `idf.py menuconfig`.
- Under `Featured FreeRTOS IoT Integration`, choose `Run qualification test`.
- Under `Component config -> Unity unit testing library`, choose `Include Unity test fixture`.

_Note: The log of module `esp_ota_ops`, `AWS_OTA` and `esp-tls-mbedtls` will be
disabled when running the qualification test. You can change the log level by
`esp_log_level_set` in [main.c](./main/main.c)._

### 6.2 Steps for each test case

1. Device Advisor Test
    - Create a [Device Advisor test suite](https://docs.aws.amazon.com/iot/latest/developerguide/device-advisor.html)
       in the console.
    - Find the Device Advisor test endpoint for your account
    - Under `Featured FreeRTOS IoT Integration -> Qualification Test Configurations -> Qualification Execution Test   Configurations`,
      choose `Device Advisor Test`.
    - Under `FreeRTOS IoT Integration -> Qualification Test Configurations -> Qualification Parameter Configurations`
      - Set `Endpoint for MQTT Broker to use` to Device Advisor test endpoint
      - Set `Thing Name for Device Advisor Test/OTA end-to-end Test` to AWS IoT Thing under test.
    - Build and run.
    - See Device Advisor test result in the console.
2. MQTT Test
   - Under `Featured FreeRTOS IoT Integration -> Qualification Test Configurations -> Qualification Execution Test Configurations`,
    choose `MQTT Test`.
   - Under `FreeRTOS IoT Integration -> Qualification Test Configurations -> Qualification Parameter Configurations`
     - Set `Endpoint for MQTT Broker to use` to your AWS IoT endpoint
     - Set `Client Identifier for MQTT Test`
   - Build and run.
   - See test result on target output.
   - Example output

     ```c
     I (821) qual_main: Run qualification test.
     ...
     -----------------------
     8 Tests 0 Failures 0 Ignored
     OK
     I (84381) qual_main: End qualification test.
     ```

3. Transport Interface Test
    - Follow
      [Run The Transport Interface Test](https://github.com/FreeRTOS/FreeRTOS-Libraries-Integration-Tests/tree/main/src/transport_interface#6-run-the-transport-interface-test)
      to start an echo server.
    - Under `Featured FreeRTOS IoT Integration -> Qualification Test Configurations -> Qualification Execution Test   Configurations`,
      choose `Transport Interface Test`.
    - Under `FreeRTOS IoT Integration -> Qualification Test Configurations -> Qualification Parameter Configurations`
    - Set `Echo Server Domain Name/IP for Transport Interface Test`
    - Set `Port for Echo Server to use`
    - Set ECHO_SERVER_ROOT_CA / TRANSPORT_CLIENT_CERTIFICATE and TRANSPORT_CLIENT_PRIVATE_KEY
      in [test_param_config.h](./components/FreeRTOS-Libraries-Integration-Tests/config/test_param_config.h).
    - Build and run.
    - See test result on target output.
    - Example output

     ```c
     I (855) qual_main: Run qualification test.
     ...
     -----------------------
     14 Tests 0 Failures 0 Ignored
     OK
     I (612755) qual_main: End qualification test.
     ```

4. OTA PAL Test
    - Under `Featured FreeRTOS IoT Integration -> Qualification Test Configurations -> Qualification Execution Test Configurations`,
      choose `OTA PAL Test`.
    - Build and run.
    - See test result on target output.
    - Example output

     ```c
     I (905) qual_main: Run qualification test.
     ...
     -----------------------
     15 Tests 0 Failures 0 Ignored
     OK
     I (113755) qual_main: End qualification test.
     ```

5. Core PKCS11 Test
    - Under `Featured FreeRTOS IoT Integration -> Qualification Test Configurations -> Qualification Execution Test Configurations`,
      choose `CorePKCS#11 Test`.
    - Build and run.
    - See test result on target output.
    - Example output

     ```c
     I (858) qual_main: Run qualification test.
     ...
     -----------------------
     17 Tests 0 Failures 0 Ignored
     OK
     I (7518) qual_main: End qualification test.
     ```

## 7 Run AWS IoT Device Tester

This repository can be tested using
[AWS IoT Device Tester for FreeRTOS (IDT)](https://aws.amazon.com/freertos/device-tester/).
IDT is a downloadable tool that can be used to exercise a device integration with
FreeRTOS to validate functionality and compatibility with Amazon IoT cloud.
Passing the test suite provided by IDT is also required to qualify a device for
the [Amazon Partner Device Catalogue](https://devices.amazonaws.com/).

IDT runs a suite of tests that include testing the device's transport interface
layer implementation, PKCS11 functionality, and OTA capabilities. In IDT test
cases, the IDT binary will make a copy of the source code, update the header files
in the project, then compile the project and flash the resulting image to your
board. Finally, IDT will read serial output from the board and communicate with
the AWS IoT cloud to ensure that test cases are passing.

### 7.1 Prerequisite

- Follow the
[OTA update with AWS IoT Guide](#5-perform-firmware-over-the-air-updates-with-aws-iot)
to create an OTA update and verify the digital signature, checksum and version
number of the new image. If firmware update is verified, you can run the tests
on your device.
- Run `idf.py menuconfig`.
- Under `Featured FreeRTOS IoT Integration`, choose `Run qualification test`.
- Under `Component config -> Unity unit testing library`, choose `Include Unity test fixture`.
- Under `Featured FreeRTOS IoT Integration -> Qualification Test Configurations -> Qualification Execution Test Configurations`,
  **DISABLE** all the tests.
- Run `idf.py fullclean` to clear local CMAKE cache.

_Note: The log of module `esp_ota_ops`, `AWS_OTA` and `esp-tls-mbedtls` will be
disabled when running the qualification test. You can change the log level by
`esp_log_level_set` in [main.c](./main/main.c)._

### 7.2 Download AWS IoT Device Tester

The latest version of IDT can be downloaded from the
[here](https://docs.aws.amazon.com/freertos/latest/userguide/dev-test-versions-afr.html).
This repository has been qualified by IDT v4.6.0 and test suite version 2.3.0 for
[FreeRTOS 202210-LTS](https://github.com/FreeRTOS/FreeRTOS-LTS/tree/202210.01-LTS).

### 7.3 Configure AWS IoT Device Tester

Follow [the instructions to setup your AWS account](https://docs.aws.amazon.com/freertos/latest/userguide/lts-idt-dev-tester-prereqs.html#lts-config-aws-account).

Extract IDT for FreeRTOS to a location on the file system

- The `devicetester-extract-location/bin` directory holds the IDT binary, which
  is the entry point used to run IDT
- The `devicetester-extract-location/results` directory holds logs that are
  generated every time you run IDT.
- The `devicetester-extract-location/configs` directory holds configuration files
  that are required to setup IDT

Before running IDT, the files in `devicetester-extract-location/configs` need
to be updated. We have pre-defined configures available in the
[idt_config](https://github.com/FreeRTOS/iot-reference-esp32c3/tree/main/idt_config).
Copy these templates to `devicetester-extract-location/configs`, and the rest of
this section will walk through the remaining values that need to be filled in.

You need to configure your AWS credentials for IDT.

- In `config.json`, update the `profile` and `awsRegion` fields

You need to specify the device details for IDT.

- In `device.json`, update `serialPort` to the serial port of your board as from
[PORT](./GettingStartedGuide.md#23-provision-the-esp32-c3-with-the-private-key-device-certificate-and-ca-certificate-in-development-mode).
Update `publicKeyAsciiHexFilePath` to the absolute path to `dummyPublicKeyAsciiHex.txt`.
Update `publicDeviceCertificateArn` to the ARN of the certificate uploaded when
[Setup AWS IoT Core](./GettingStartedGuide.md#21-setup-aws-iot-core).

You need to configure IDT the build, flash and test settings.

- In `build.bat` / `build.sh`, update ESP_IDF_PATH, and ESP_IDF_FRAMEWORK_PATH
- In `flash.bat` / `flash.sh`, update ESP_IDF_PATH, ESP_IDF_FRAMEWORK_PATH, and NUM_COMPORT
- In `userdata.json`, update `sourcePath` to the absolute path to the root of this repository.
- In `userdata.json`, update `signerCertificate` with the ARN of the
  [Setup pre-requisites for OTA cloud resources](./GettingStartedGuide.md#51-setup-pre-requisites-for-ota-cloud-resources)
- Run all the steps to create a
  [second code signing certificate](./GettingStartedGuide.md#51-setup-pre-requisites-for-ota-cloud-resources)
  but do NOT provision the key onto your board.
- Copy the ARN for this certificate in `userdata.json` for the field
  `untrustedSignerCertificate`.

### 7.4 Running the FreeRTOS qualification 2.0 suite

With configuration complete, IDT can be run for an individual test group, a
test case, or the entire qualification suite.

List all the available test groups, run:

```sh
.\devicetester_win_x86-64.exe list-groups
```

Run one or more specified test group, run e.g.:

```sh
.\devicetester_win_x86-64.exe run-suite --group-id FullCloudIoT --group-id OTACore
```

Run one or more specified tests, run e.g.:

```sh
.\devicetester_win_x86-64.exe run-suite --group-id OTADataplaneMQTT --test-id OTAE2EGreaterVersion
```

To run the entire qualification suite, run:

```sh
.\devicetester_win_x86-64.exe run-suite --skip-group-id FullPKCS11_PreProvisioned_RSA --skip-group-id FullPKCS11_Import_RSA --skip-group-id FullPKCS11_Core --skip-group-id FullTransportInterfacePlainText
```

For more information, `.\devicetester_win_x86-64.exe help` will show all available commands.

When IDT is run, it generates the `results/uuid` directory that contains the
logs and other information associated with your test run. See
[Understanding results and logs](https://docs.aws.amazon.com/freertos/latest/userguide/lts-results-logs.html)
for more details.