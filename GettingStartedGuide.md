# Getting Started Guide

This guide contains instructions on how to setup, build and run the demo without use of the security features of the ESP32-C3 enabled i.e. without the DS peripheral, flash encryption and Secure Boot. It is meant to provide a developer with a friendly first use experience.

Once completed, one can progress to the [Use Security Features](UseSecurityFeatures.md) guide.

## 1 Pre-requisites

### 1.1 Hardware Requirements

* Micro USB cable.
* ESP32-C3 board (e.g [ESP32-C3-DevKitC-02](https://www.mouser.com/ProductDetail/Espressif-Systems/ESP32-C3-DevKitC-02?qs=stqOd1AaK7%2F1Q62ysr4CMA%3D%3D)).
* Personal Computer with Linux, macOS, or Windows.
* WiFi access point with access to the internet.

### 1.2 Software Requirements

* ESP-IDF 4.4 or higher to configure, build, and flash the project. To setup for the ESP32-C3, follow Espressif's [Getting Started Guide for the ESP32-C3](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/get-started/index.html).
* [Python3](https://www.python.org/downloads/)
    and the Package Installer for Python [pip](https://pip.pypa.io/en/stable/installation/) to use the AWS CLI to import certificates and perform OTA Job set up. Pip is included when you install
    from Python 3.10.
* [OpenSSL](https://www.openssl.org/) to create the OTA signing
    key and certificate. If you have git installed on your machine, you can also use the openssl.exe that comes with the git installation.
* [AWS CLI Interface](https://docs.aws.amazon.com/cli/latest/userguide/getting-started-install.html)
    to import your code-signing certificate, private key, and certificate chain into the AWS Certificate Manager,
    and to set up an OTA firmware update job. Refer to
    [Installing or updating the latest version of the AWS CLI](https://docs.aws.amazon.com/cli/latest/userguide/getting-started-install.html)
    for installation instructions. After installation, follow the steps in
    [Configuration basics](https://docs.aws.amazon.com/cli/latest/userguide/cli-configure-quickstart.html)
    to configure the basic settings (security credentials, the default AWS output format and the default AWS Region)
    that AWS CLI uses to interact with AWS. (If you don't have an AWS account and user, follow steps 1 and 2 in the AWS IoT Core Setup Guide below before following the Configuration basics for the AWS CLI.)

## 2 Demo setup

### 2.1 Setup AWS IoT Core:

To setup AWS IoT Core, follow the [AWS IoT Core Setup Guide](AWSSetup.md). The guide shows you how to sign up for an AWS account, create a user, and register your device with AWS IoT Core.
After you have followed the instructions in the AWS IoT Core Setup Guide, you will have created a **device Endpoint**, an AWS IoT **thing**, a **PEM-encoded device certificate**, a **PEM-encoded private key**, and a **PEM-encoded root CA certificate**. (An explanation of these entities is given in the Setup Guide.) The root CA certificate can also be downloaded [here](https://www.amazontrust.com/repository/AmazonRootCA1.pem). Your ESP23-C3 board must now be provisioned with these entities in order for it to connect securely with AWS IoT Core.

### 2.2 Configure the project with the AWS IoT Thing Name and AWS device Endpoint
The demo will connect to the AWS IoT device Endpoint that you configure here.

1. From a terminal/command prompt, run `idf.py menuconfig`. This assumes the ESP-IDF environment is exported-- i.e. that export.bat/export.sh, which can be found under the ESP-IDF directory, has been run, or that you are using the ESP-IDF command prompt/terminal. For Visual Studio (VS) Code users who are using the Espressif IDF extension, do ->View->Command Palette->Search for `ESP-IDF: SDK Configuration editor (menuconfig)` and select the command. The `SDK Configuration editor` window should pop up after a moment.
(Note: If running menuconfig from within a VS Code command prompt, 'j' and 'k' may have to be used in place of the 'up' and 'down' arrow keys. Alternately, one can use a command prompt/terminal outside of the VS Code editor).
2. Select `Featured FreeRTOS IoT Integration` from the menu.
3. Set `Endpoint for MQTT Broker to use` to your **AWS device Endpoint**.
4. Set `Port for MQTT Broker to use` to `8883`.
5. Set `Thing name` to your **Thing Name**.
6. Go back to main menu, Save and Exit.

### 2.3 Provision the ESP32-C3 with the private key, device certificate and CA certificate in Development Mode
The key and certificates which will be used to establish a secure TLS connection will be stored in a special flash partition.

1. Create the `esp_secure_crt` partition binary.
```
python components/esp_secure_cert_mgr/tools/configure_esp_secure_cert.py -p PORT --keep_ds_data_on_host --ca-cert CA_CERT_FILEPATH --device-cert DEVICE_CERT_FILEPATH --private-key PRIVATE_KEY_FILEPATH --target_chip esp32c3 --secure_cert_type cust_flash
```
Replace:
**PORT** with the serial port to which the ESP32-C3 board is connected.
**CA_CERT_FILEPATH** with the file path to the **PEM-encoded root CA certificate**.
**DEVICE_CERT_FILEPATH** with the file path to the **PEM-encoded device certificate**.
**PRIVATE_KEY_FILEPATH** with the file path to the **PEM-encoded private key**.
For convenience sake, you could place your key and certificate files under the 'main/certs' directory.

You will see a message that says "--configure_ds option not set. Configuring without use of DS peripheral.". Ignore this message.
The partition binary will be created here: "esp_ds_data/esp_secure_crt.bin".

2. Write the `esp_secure_crt` partition binary (stored in `esp_ds_data/esp_secure_crt.bin`) to the ESP32-C3's flash by running the following command:
```
esptool.py --no-stub --port PORT write_flash 0xD000 esp_ds_data/esp_secure_cert.bin
```
Replace **PORT** with the serial port to which the ESP32-C3 board is connected.

## 3 Build and flash the demo project

Before you build and flash the demo project, if you are setting up the ESP32-C3 for the first time, the board will have to be provisioned with Wi-Fi credentials to be able to use your Wi-Fi network to connect to the internet. This can be done via BLE or SoftAP. BLE is the default, but can be changed via menuconfig - Featured FreeRTOS IoT Integration -> Show provisioning QR code -> Provisioning Transport method.

Espressif provides BLE and SoftAP provisioning mobile apps which are available on the [Google Play Store](https://play.google.com/store/apps/details?id=com.espressif.provble) for Android or the [Apple App Store](https://apps.apple.com/app/esp-ble-provisioning/id1473590141) for iOS. Download the appropriate app to your phone before proceeding.

Run the following command to build and flash the demo project:
```
idf.py -p PORT flash monitor
```
Replace **PORT** with the serial port to which the ESP32-C3 is connected.

If you are setting up the ESP32-C3 for the first time, the device will go though the Wi-Fi provisioning workflow and you will have to use the app you previously downloaded to scan the QR code and follow the instructions that follow. Once the device is provisioned successfully with the required Wi-Fi credentials, the demo will proceed. If previously Wi-Fi provisioned, the device will not go through the Wi-Fi provisioning workflow again.
Note: If the ESP32-C3 was previously Wi-Fi provisioned, and you are on a different network and wish to re-provision with new network credentials, you will have to erase the nvs flash partition where the Wi-Fi credentials are stored, otherwise the device will presume that it has already been provisioned. In this situation, use the following command to erase the nvs partition.
```
parttool.py -p PORT erase_partition --partition-name=nvs
```

## 4 Monitoring the demo

1. On the serial terminal console, confirm that the TLS connection was successful and that MQTT messages are published.
```
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

2. On the AWS IoT console, select "Test" then select "MQTT test client". Under "Subscribe to a topic", type "#". (# is used to select all topics. You can also enter a specific topic such as /filter/Publisher0.) Click on "Subscribe", then confirm that the MQTT messages from the device are received.
3. To change the LED power state, under "Publish to a topic" publish one of the following JSON payloads to the `/filter/TempSubPubLED` topic:

To turn the LED on:
```json
{
    "led":
    {
        "power": 1
    }
}
```

To turn the LED off:
```json
{
    "led":
    {
        "power": 0
    }
}
```

## 5 Perform firmware Over-the-Air Updates with AWS IoT

This demo uses the OTA client library and the AWS IoT OTA service for code signing and secure download of firmware updates.

### 5.1 Setup pre-requisites for OTA cloud resources
Before you create an OTA job, the following resources are required. This is a one time setup required for performing OTA firmware updates. Make a note of the names of the resources you create, as you will need to provide them during subsequent configuration steps.

* An Amazon S3 bucket to store your updated firmware. S3 is an AWS Service that enables you to store files in the cloud that can be accessed by you or other services. This is used by the OTA Update Manager Service to store the firmware image in an S3 “bucket” before sending it to the device. [Create an Amazon S3 Bucket to Store Your Update](https://docs.aws.amazon.com/freertos/latest/userguide/dg-ota-bucket.html).
* An OTA Update Service role. By default, the OTA Update Manager cloud service does not have permission to access the S3 bucket that will contain the firmware image. An OTA Service Role is required to allow the OTA Update Manager Service to read and write to the S3 bucket. [Create an OTA Update Service role](https://docs.aws.amazon.com/freertos/latest/userguide/create-service-role.html).
* An OTA user policy. An OTA User Policy is required to give your AWS account permissions to interact with the AWS services required for creating an OTA Update. [Create an OTA User Policy](https://docs.aws.amazon.com/freertos/latest/userguide/create-ota-user-policy.html).
* [Create a code-signing certificate](https://docs.aws.amazon.com/freertos/latest/userguide/ota-code-sign-cert-win.html). The demos support a code-signing certificate with an ECDSA P-256 key and SHA-256 hash to perform OTA updates.
* [Grant access to Code Signing for AWS IoT](https://docs.aws.amazon.com/freertos/latest/userguide/code-sign-policy.html).

### 5.2 Provision the project with the code-signing public key certificate
The code-signing public key certificate will be used by the application binary, i.e. the demo, to authenticate a binary that was downloaded for an update. (This downloaded firmware would have been signed by the certificate's corresponding private key.) 

Copy the public key certificate that you would have created in the 'Create a code-signing certificate' step to 'main/certs/aws_codesign.crt'

The demo will read the certificate 'aws_codesign.crt' from your host filesystem and save it in memory.

### 5.3 Build an application binary with a higher version number, to be downloaded and activated on the device 
To perform an OTA firmware update, you must go through these steps:
1. Increment the version of the binary and create the signed binary image.
2. Upload this image to an S3 bucket and create an OTA Update Job on the AWS IoT console.
3. Restore the original version (lower version number) and flash this to the device.

The version of the new image must be later than the current image on the board or else OTA will not proceed.

The OTA Update Job will send a notification to an MQTT topic that the device will be listening to. When it receives an OTA update notfication, the device will then start downloading the new firmware.

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
If successful, there will be a new binary under the 'build' directory - build/GoldenReferenceIntegration.bin. Copy this binary to another location, otherwise it will be overwritten in the next step.

### 5.4 Build and flash the device with a binary with a lower version number
1. Follow the same steps in 5.3 starting with running idf.py menuconfig, but this time, set the `Application version build` number to '0'.
2. Build and flash this new application binary with a lower version number.
```
idf.py -p PORT flash monitor
```

### 5.5 Upload the binary with the higher version number (created in step 5.3) and create an OTA Update Job
1. In the navigation pane of the AWS IoT console, choose 'Manage', and then choose 'Jobs'.
Choose 'Create a job'.
2. Next to 'Create a FreeRTOS Over-the-Air (OTA) update job', choose 'Create FreeRTOS OTA update job'. Provide a name for the job and click on 'Next'.
3. You can deploy an OTA update to a single device or a group of devices. Under 'Devices to update', select the Thing you created earlier. You can find it listed under AWS IoT->Manage->Things. If you are updating a group of devices, select the check box next to the thing group associated with your devices. 
4. Under 'Select the protocol for file transfer', choose 'MQTT'.
5. Under 'Sign and choose your file', choose 'Sign a new file for me'.
6. Under 'Code signing profile', choose 'Create a new profile'.
7. In 'Create a code signing profile':
   1. Type in a name for this profile.
   1. For the Device hardware platform, select: 'ESP32-DevKitC'.
   1. Under Code signing certificate, choose 'Select an existing certificate', then choose the certificate that you created with the AWS CLI earlier and registered with AWS ACM (you can use the ARN to identify it).
   1. Under 'Path name of code signing certificate on device', enter '/'. (This is not applicable for the ESP32-C3 and hence the / is only a filler.)
   1. Click 'Create'. Confirm that the code signing profile was created successfully.
8. Back on the FreeRTOS OTA Job console:
   1. Under 'Code signing profile', select the code signing profile that was just created from the drop down list.
   1. Under 'File', choose 'Upload a new file', then click 'Choose file'. A file browser pops up. Select the signed binary image with the higher version number.
   1. Under 'File upload location in S3', click 'Browse S3', then select the S3 bucket that you created earlier for this job. Click 'Choose'.
   1. Under 'Path name of file on device', type 'NA'.
   1. Under 'IAM role for OTA update job', choose the role that you created earlier for the OTA update from the drop down list.
   1. Click 'Next', then click on 'Create job'. Confirm that the job was created successfully. Note: If this fails to create an OTA job, make sure the role for this OTA job update has the correct permissions (policies) attached.

### 5.6 Monitor OTA

Once the job is created successfully, the demo should start downloading the firmware in chunks. For example:
```
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

Once all the firmware image chunks are downloaded and the signature is validated, the device reboots with the new image. See the OTA section in the [Featured FreeRTOS IoT Integration page for the ESP32-C3](https://www.freertos.org/featured-freertos-iot-integration-targeting-an-espressif-esp32-c3-risc-v-mcu) on FreeRTOS.org for more details.
You can see the new version number of the demo binary in the terminal console output. Look for the string "Application version"

```
I (461802) esp_image: Verifying image signature...
I (461812) secure_boot_v2: Take trusted digest key(s) from eFuse block(s)
I (461822) secure_boot_v2: #0 app key digest == #0 trusted key digest
I (461822) secure_boot_v2: Verifying with RSA-PSS...
I (461872) secure_boot_v2: Signature verified successfully!
I (461872) esp_image: segment 0: paddr=001b0020 vaddr=3c0e0020 size=2d668h (185960) map
I (461902) esp_image: segment 1: paddr=001dd690 vaddr=3fc91800 size=02988h ( 10632) 
I (461902) esp_image: segment 2: paddr=001e0020 vaddr=42000020 size=da904h (895236) map
I (462022) esp_image: segment 3: paddr=002ba92c vaddr=3fc94188 size=00df4h (  3572) 
I (462022) esp_image: segment 4: paddr=002bb728 vaddr=40380000 size=11720h ( 71456) 
I (462032) esp_image: segment 5: paddr=002cce50 vaddr=50000010 size=00010h (    16) 
I (462032) esp_image: segment 6: paddr=002cce68 vaddr=00000000 size=03168h ( 12648) 
I (462042) esp_image: Verifying image signature...
I (462042) secure_boot_v2: Take trusted digest key(s) from eFuse block(s)
I (462052) secure_boot_v2: #0 app key digest == #0 trusted key digest
I (462062) secure_boot_v2: Verifying with RSA-PSS...
I (462112) secure_boot_v2: Signature verified successfully!
I (462652) wifi:state: run -> init (0)
I (462652) wifi:pm stop, total sleep time: 382862279 us / 461539198 us

W (462652) wifi:<ba-del>idx
W (462652) wifi:<ba-del>idx
I (462652) wifi:new:<6,0>, old:<6,0>, ap:<255,255>, sta:<6,0>, prof:1
I (462662) core_mqtt_agent_network_manager: WiFi disconnected.
I (462662) app_wifi: Disconnected. Connecting to the AP again...
E (462672) esp-tls-mbedtls: read error :-0x004C:
I (462672) core_mqtt_agent_network_manager: coreMQTT-Agent disconnected.
I (462682) MQTT: coreMQTT-Agent disconnected.
I (462692) sub_pub_unsub_demo: coreMQTT-Agent disconnected. Preventing coreMQTT-Agent commands from being enqueued.
I (462702) temp_sub_pub_demo: coreMQTT-Agent disconnected. Preventing coreMQTT-Agent commands from being enqueued.
I (462712) ota_over_mqtt_demo: coreMQTT-Agent disconnected. Suspending OTA agent.
I (462722) wifi:flush txq
I (462722) wifi:stop sw txq
I (462722) wifi:lmac stop hw txq
ESP-ROM:esp32c3-api1-20210207
Build:Feb  7 2021
rst:0x3 (RTC_SW_SYS_RST),boot:0xc (SPI_FAST_FLASH_BOOT)
Saved PC:0x403805d8
0x403805d8: esp_restart_noos_dig at C:/Users/wallit/Work/.espressif/frameworks/esp-idf-v4.4/components/esp_system/esp_system.c:46 (discriminator 1)

SPIWP:0xee
mode:DIO, clock div:1
Valid secure boot key blocks: 0
secure boot verification succeeded
load:0x3fcd6268,len:0x2e94
load:0x403ce000,len:0x930
load:0x403d0000,len:0x4db4
entry 0x403ce000
I (75) boot: ESP-IDF v4.4 2nd stage bootloader
I (75) boot: compile time 17:29:01
I (75) boot: chip revision: 3
I (76) boot.esp32c3: SPI Speed      : 80MHz
I (81) boot.esp32c3: SPI Mode       : DIO
I (86) boot.esp32c3: SPI Flash Size : 4MB
I (91) boot: Enabling RNG early entropy source...
I (96) boot: Partition Table:
I (100) boot: ## Label            Usage          Type ST Offset   Length
I (107) boot:  0 esp_secure_cert  unknown          3f 06 0000d000 00006000
I (115) boot:  1 nvs              WiFi data        01 02 00013000 00006000
I (122) boot:  2 otadata          OTA data         01 00 00019000 00002000
I (130) boot:  3 phy_init         RF data          01 01 0001b000 00001000
I (137) boot:  4 ota_0            OTA app          00 10 00020000 00190000
I (145) boot:  5 ota_1            OTA app          00 11 001b0000 00190000
I (152) boot:  6 storage          WiFi data        01 02 00340000 00010000
I (160) boot:  7 nvs_key          NVS keys         01 04 00350000 00001000
I (167) boot: End of partition table
I (172) esp_image: segment 0: paddr=001b0020 vaddr=3c0e0020 size=2d668h (185960) map
I (208) esp_image: segment 1: paddr=001dd690 vaddr=3fc91800 size=02988h ( 10632) load
I (210) esp_image: segment 2: paddr=001e0020 vaddr=42000020 size=da904h (895236) map
I (348) esp_image: segment 3: paddr=002ba92c vaddr=3fc94188 size=00df4h (  3572) load
I (349) esp_image: segment 4: paddr=002bb728 vaddr=40380000 size=11720h ( 71456) load
I (367) esp_image: segment 5: paddr=002cce50 vaddr=50000010 size=00010h (    16) load
I (367) esp_image: segment 6: paddr=002cce68 vaddr=00000000 size=03168h ( 12648) 
I (374) esp_image: Verifying image signature...
I (378) secure_boot_v2: Verifying with RSA-PSS...
I (386) secure_boot_v2: Signature verified successfully!
I (394) boot: Loaded app from partition at offset 0x1b0000
I (395) secure_boot_v2: enabling secure boot v2...
I (401) secure_boot_v2: secure boot v2 is already enabled, continuing..
I (408) boot: Disabling RNG early entropy source...
I (425) cpu_start: Pro cpu up.
I (433) cpu_start: Pro cpu start user code
I (433) cpu_start: cpu freq: 160000000
I (433) cpu_start: Application information:
I (436) cpu_start: Project name:     GoldenReferenceIntegration
I (442) cpu_start: App version:      c506f74-dirty
I (448) cpu_start: Compile time:     Apr 27 2022 14:21:30
I (454) cpu_start: ELF file SHA256:  6b1586752c298eb4...
I (460) cpu_start: ESP-IDF:          v4.4
I (465) heap_init: Initializing. RAM available for dynamic allocation:
I (472) heap_init: At 3FCACE60 len 000131A0 (76 KiB): DRAM
I (478) heap_init: At 3FCC0000 len 0001F060 (124 KiB): STACK/DRAM
I (485) heap_init: At 50000020 len 00001FE0 (7 KiB): RTCRAM
I (491) spi_flash: detected chip: generic
I (496) spi_flash: flash io: dio
I (500) sleep: Configure to isolate all GPIO pins in sleep state
I (507) sleep: Enable automatic switching of GPIO sleep configuration
I (514) coexist: coexist rom version 9387209
I (519) cpu_start: Starting scheduler.
I (524) main: 
...
I (764) temp_pub_sub_demo: Sending subscribe request to agent for topic filter: /filter/Publisher0 with id 
1
I (814) pp: pp rom version: 9387209
I (814) net80211: net80211 rom version: 9387209
I (814) ota_over_mqtt_demo: OTA over MQTT demo, Application version 0.9.1
I (834) ota_over_mqtt_demo:  Received: 0   Queued: 0   Processed: 0   Dropped: 0
I (844) AWS_OTA: otaPal_GetPlatformImageState
I (844) esp_ota_ops: aws_esp_ota_get_boot_flags: 1
I (854) esp_ota_ops: [1] aflags/seq:0xffffffff/0x2, pflags/seq:0x2/0x1
I (854) AWS_OTA: Current State=[RequestingJob], Event=[Start], New state=[RequestingJob]
```

