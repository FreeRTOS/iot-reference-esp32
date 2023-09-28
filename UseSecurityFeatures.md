# Getting Started With Security Features

## 1 Pre-requisites

In the [Getting Started Guide](GettingStartedGuide.md), one would have setup the ESP32-C3 device, installed the required software, setup AWS IoT, configured the demo project with the AWS IoT endpoint, thing name, private key and certificates, and built and run the demo.

## 2 Enable the DS peripheral

1. Run `idf.py menuconfig`
2. Select `Featured FreeRTOS IoT Integration`.
3. Select `Use DS Peripheral`.
4. Go back to the main menu.

## 3 Enable flash encryption

5. Select `Security features`.
6. Set `Enable flash encryption on boot (READ DOCS FIRST)` to true.
7. Select `Enable usage mode`.
8. Set `Development (NOT SECURE)` to true.
9. Go back to `Security features`.
10. Go back to main menu, Save and Exit.

**NOTE**: This enables Flash Encryption in **Development Mode**. For production devices,
refer to Espressif's documentation on
[**Release Mode** for Flash Encryption](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/security/flash-encryption.html#release-mode)

## 4 Provision the ESP32-C3 with the private key, device certificate and CA certificate in Development Mode
The key and certificates which will be used to establish a secure TLS
connection will be encrypted and stored in a special flash partition.

1. Create the `esp_secure_crt` partition binary. If this is the first time
running this command, an eFuse block in the ESP32-C3 will be burnt with a
 generated key and this **CANNOT** be reversed:
```
python components/esp_secure_cert_mgr/tools/configure_esp_secure_cert.py -p PORT --configure_ds --keep_ds_data_on_host --ca-cert CA_CERT_FILEPATH --device-cert DEVICE_CERT_FILEPATH --private-key PRIVATE_KEY_FILEPATH --target_chip esp32c3 --secure_cert_type cust_flash
```
Replace:
**PORT** with the serial port to which the ESP32-C3 board is connected.
**CA_CERT_FILEPATH** with the file path to the **PEM-encoded root CA certificate**.
**DEVICE_CERT_FILEPATH** with the file path to the **PEM-encoded device certificate**.
**PRIVATE_KEY_FILEPATH** with the file path to the **PEM-encoded private key**.

Type in BURN when prompted to.

2. Write the `esp_secure_crt` partition binary (stored in `esp_ds_data/esp_secure_crt.bin`) to the ESP32-C3's flash by running the following command:
```
esptool.py --no-stub --port PORT write_flash 0xD000 esp_ds_data/esp_secure_cert.bin
```
Replace **PORT** with the serial port to which the ESP32-C3 board is connected.

## 5 Configure Secure Boot

1. For Secure Boot, an RSA 3072 private key must be generated which will be
used to sign the secondary bootloader and the application binary. Please refer
to the Secure Boot section in the
[Featured FreeRTOS IoT Integration page for the ESP32-C3](https://www.freertos.org/featured-freertos-iot-integration-targeting-an-espressif-esp32-c3-risc-v-mcu/)
on FreeRTOS.org for further details. The private key can be generated with the
following command:
```
openssl genrsa -out secure_boot_signing_key.pem 3072
```
This will output `secure_boot_signing_key.pem`, which can be renamed as you see
fit. Keep this key in a safe place as it will be necessary for signing binaries
in the future.
Note: If you have installed openssl and the openssl command fails with a command
not found error, please ensure you have the openssl path exported when using your
terminal/command prompt.

2. Run `idf.py menuconfig`
3. Select `Security features`.
4. Set `Enable hardware Secure Boot in bootloader (READ DOCS FIRST)` to true.
5. Set `Sign binaries during build` to true.
6. Set `Secure boot private signing key` to the path to the RSA 3072 private key you generated in step 1.
7. Go back to main menu, Save and Exit.

**NOTE**: This covers setting up Secure Boot with a single private key, but
up to 3 private keys can be used. Refer to Espressif's documentation on
[Secure Boot V2](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/security/secure-boot-v2.html).

### 5.1 Build and flash the Secure Boot enabled bootloader
1. Build the bootloader by running the following command:
```
idf.py bootloader
```
This command should output something similar to the following:
```
==============================================================================
Bootloader built. Secure boot enabled, so bootloader not flashed automatically.
To sign the bootloader with additional private keys.
        C:/Users/user/.espressif/python_env/idf4.4_py3.8_env/Scripts/python.exe C:/Users/user/Desktop/esp-idf-6/components/esptool_py/esptool/espsecure.py sign_data -k secure_boot_signing_key2.pem -v 2 --append_signatures -o signed_bootloader.bin build/bootloader/bootloader.bin
Secure boot enabled, so bootloader not flashed automatically.
        C:/Users/user/.espressif/python_env/idf4.4_py3.8_env/Scripts/python.exe  C:/Users/user/Desktop/esp-idf-6/components/esptool_py/esptool/esptool.py --chip esp32c3 --port=(PORT) --baud=(BAUD) --before=default_reset --after=no_reset --no-stub write_flash --flash_mode dio --flash_freq 80m --flash_size 4MB 0x0 C:/FreeRTOS-Repositories/lab-iot-reference-esp32c3/build/bootloader/bootloader.bin
==============================================================================
```
2. Flash the bootloader by copying and pasting the command under "Secure boot
enabled, so bootloader not flashed automatically," (the second block of text)
replacing:
**PORT** with the serial port to which the ESP32-C3 is connected. (Do not include
the opening and closing braces around PORT in the command)
**BAUD** with 460800.

## 6 Build and flash the demo project

With Secure Boot enabled, application binaries must be signed before being
flashed. With the configurations set in this document, this is automatically
done any time a new application binary is built. Binaries are automatically
signed using the RSA key we generated and configured in section 2.2
(Configure Secure Boot).

If flash encryption is enabled, the bootloader will generate the private key
used to encrypt flash and store it in the ESP32-C3's eFuse. It will then encrypt
the bootloader, the partition table, all `app` partitions, and all partitions
marked `encrypted` in the partition table.

Run the following command to build and flash the demo project:
```
idf.py -p PORT flash monitor
```
Replace **PORT** with the serial port to which the ESP32-C3 is connected.

**NOTE**: If Flash Encryption was enabled, instead of `flash`, you must use
`encrypted-flash` to flash the board AFTER this step i.e. with subsequent flashes.
If flashing to an encrypted part of flash with `esptool.py`, you must also add the
`--encrypt` option.

## 7 Monitoring the demo

1. On the serial terminal console, confirm that the TLS connection was
successful and that MQTT messages are published.
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

2. On the AWS IoT console, select "Test" then select "MQTT test client". Under
"Subscribe to a topic", type "#" (# is to select all topics. You can also enter a
specific topic such as /filter/Publisher0), click on "Subscribe", and confirm that
the MQTT messages from the device are received.
3. To change the LED power state, under "Publish to a topic" publish one of the
following JSON payloads to the `/filter/TempSubPubLED` topic:

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

## 8 Perform firmware Over-the-Air Updates with AWS IoT

In the previous [Getting Started Guide](GettingStartedGuide.md),
you would have setup the required OTA cloud resources.

### 8.1 Build an application binary with a higher version number, to be downloaded and activated on the device

Create a binary with a higher version number.
1. Run `idf.py menuconfig`
2. Select `Featured FreeRTOS IoT Integration` from the menu.
3. Under `Enable OTA demo` go to `OTA demo configurations`
4. Set the `Application version build` number to '1'.
5. Go back to main menu, Save and exit.
6. Run the following command to only build the demo project.
```
idf.py build
```
If successful, there will be a new binary under the 'build' directory - build/
GoldenReferenceIntegration.bin. Copy this binary to another location, else it will
be overwritten in the next step.

### 8.2 Build and flash the device with a binary with a lower version number
1. Follow the same steps in 8.1, but this time, set the `Application version build` number to '0'.
2. Build and flash this new application binary with a lower version number.
```
idf.py -p PORT encrypted-flash monitor
```
**NOTE**: Since Flash Encryption was enabled in the previous steps, instead of
`flash`, we use `encrypted-flash` to flash the board for this step.

### 8.3 Upload the binary with the higher version number (created in step 8.1) and create an OTA Update Job
1. In the navigation pane of the AWS IoT console, choose 'Manage', and then choose 'Jobs'.
Choose 'Create a job'.
2. Next to 'Create a FreeRTOS Over-the-Air (OTA) update job', choose
'Create FreeRTOS OTA update job'. Provide a name for the job and click on 'Next'.
3. You can deploy an OTA update to a single device or a group of devices.
Under 'Devices to update', select the Thing you would have created earlier.
You can find it listed under AWS IoT->Manage->Things. If you are updating a
group of devices, select the check box next to the thing group associated
with your devices.
4. Under 'Select the protocol for file transfer', choose 'MQTT'.
5. Under 'Sign and choose your file', choose 'Sign a new file for me'.
6. Under 'Code signing profile', choose the code signing profile you would have
created earlier.
7. Under 'File', choose 'Upload a new file' then click 'Choose file'. A file
browser pops up. Select the signed binary image with the higher version number.
8. Under 'File upload location in S3', click 'Browse S3', then select the S3 bucket
that you had earlier created for this job. Click 'Choose'
9. Under 'Path name of file on device', type 'NA'
10. Under 'IAM role for OTA update job', choose the role that you created
earlier for the OTA update from the drop down list.
11. Click 'Next', then click on 'Create job'. Confirm if the job was created successfully.

### 8.4 Monitor OTA

Once the job is created successfully, the demo should start downloading the firmware in chunks. For eg.
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

Once all the firmware image chunks are downloaded and the signature is validated, the device reboots with the new image, during which the Secure Boot sequence is executed. See the OTA section in the [Featured FreeRTOS IoT Integration page for the ESP32-C3](https://www.freertos.org/featured-freertos-iot-integration-targeting-an-espressif-esp32-c3-risc-v-mcu/) on FreeRTOS.org for more details.
You can see the new version number of the demo binary. Look for the string "Application version"

```
I (793824) AWS_OTA: Number of blocks remaining: 1
I (793824) ota_over_mqtt_demo: OTA Event processing completed. Freeing the event buffer to pool.
I (793824) AWS_OTA: Current State=[WaitingForFileBlock], Event=[ReceivedFileBlock], New state=[WaitingForFileBlock]
I (793914) ota_over_mqtt_demo:  Received: 288   Queued: 288   Processed: 288   Dropped: 0
I (794914) ota_over_mqtt_demo:  Received: 288   Queued: 288   Processed: 288   Dropped: 0
I (795914) ota_over_mqtt_demo:  Received: 288   Queued: 288   Processed: 288   Dropped: 0
I (796734) coreMQTT: Publishing message to $aws/things/thing_esp32c3_nonOta/streams/AFR_OTA-dbfcec8e-1161-42e5-be91-a570c42ae26c/get/cbor.

I (796734) ota_over_mqtt_demo: Sent PUBLISH packet to broker $aws/things/thing_esp32c3_nonOta/streams/AFR_OTA-dbfcec8e-1161-42e5-be91-a570c42ae26c/get/cbor to broker.


I (796744) AWS_OTA: Published to MQTT topic to request the next block: topic=$aws/things/thing_esp32c3_nonOta/streams/AFR_OTA-dbfcec8e-1161-42e5-be91-a570c42ae26c/get/cbor
I (796764) AWS_OTA: Current State=[WaitingForFileBlock], Event=[RequestFileBlock], New state=[WaitingForFileBlock]
I (796884) coreMQTT: Packet received. ReceivedBytes=4219.
I (796884) coreMQTT: De-serialized incoming PUBLISH packet: DeserializerResult=MQTTSuccess.
I (796884) coreMQTT: State record updated. New state=MQTTPublishDone.
I (796894) AWS_OTA: Received valid file block: Block index=288, Size=4096
I (796914) AWS_OTA: Received final block of the update.
I (797464) AWS_OTA: Signature verification succeeded.
I (797464) AWS_OTA: Received entire update and validated the signature.
I (797474) ota_over_mqtt_demo:  Received: 289   Queued: 289   Processed: 288   Dropped: 0
I (798474) ota_over_mqtt_demo:  Received: 289   Queued: 289   Processed: 288   Dropped: 0
I (799474) ota_over_mqtt_demo:  Received: 289   Queued: 289   Processed: 288   Dropped: 0
I (799894) coreMQTT: Publishing message to $aws/things/thing_esp32c3_nonOta/jobs/AFR_OTA-c3-29440/update.

I (800044) coreMQTT: Packet received. ReceivedBytes=2.
I (800044) coreMQTT: Ack packet deserialized with result: MQTTSuccess.
I (800044) coreMQTT: State record updated. New state=MQTTPublishDone.
I (800054) ota_over_mqtt_demo: Sent PUBLISH packet to broker $aws/things/thing_esp32c3_nonOta/jobs/AFR_OTA-c3-29440/update to broker.


I (800064) ota_over_mqtt_demo: Received OtaJobEventActivate callback from OTA Agent.
I (800074) esp_image: segment 0: paddr=001b0020 vaddr=3c0e0020 size=2e280h (189056) map
I (800064) coreMQTT: Packet received. ReceivedBytes=96.
I (800094) coreMQTT: De-serialized incoming PUBLISH packet: DeserializerResult=MQTTSuccess.
I (800104) coreMQTT: State record updated. New state=MQTTPublishDone.
W (800114) core_mqtt_agent_manager: WARN:  Received an unsolicited publish from topic $aws/things/thing_esp32c3_nonOta/jobs/AFR_OTA-c3-29440/update/accepted
I (800124) esp_image: segment 1: paddr=001de2a8 vaddr=3fc91800 size=01d70h (  7536)
I (800134) esp_image: segment 2: paddr=001e0020 vaddr=42000020 size=db530h (898352) map
I (800264) esp_image: segment 3: paddr=002bb558 vaddr=3fc93570 size=01a14h (  6676)
I (800264) esp_image: segment 4: paddr=002bcf74 vaddr=40380000 size=11720h ( 71456)
I (800284) esp_image: segment 5: paddr=002ce69c vaddr=50000010 size=00010h (    16)
I (800284) esp_image: segment 6: paddr=002ce6b4 vaddr=00000000 size=0191ch (  6428)
I (800284) esp_image: Verifying image signature...
I (800294) secure_boot_v2: Take trusted digest key(s) from eFuse block(s)
I (800304) secure_boot_v2: #0 app key digest == #0 trusted key digest
I (800304) secure_boot_v2: Verifying with RSA-PSS...
I (800354) secure_boot_v2: Signature verified successfully!
I (800354) esp_image: segment 0: paddr=001b0020 vaddr=3c0e0020 size=2e280h (189056) map
I (800384) esp_image: segment 1: paddr=001de2a8 vaddr=3fc91800 size=01d70h (  7536)
I (800384) esp_image: segment 2: paddr=001e0020 vaddr=42000020 size=db530h (898352) map
I (800524) esp_image: segment 3: paddr=002bb558 vaddr=3fc93570 size=01a14h (  6676)
I (800524) esp_image: segment 4: paddr=002bcf74 vaddr=40380000 size=11720h ( 71456)
I (800534) esp_image: segment 5: paddr=002ce69c vaddr=50000010 size=00010h (    16)
I (800534) esp_image: segment 6: paddr=002ce6b4 vaddr=00000000 size=0191ch (  6428)
I (800544) esp_image: Verifying image signature...
I (800544) secure_boot_v2: Take trusted digest key(s) from eFuse block(s)
I (800554) secure_boot_v2: #0 app key digest == #0 trusted key digest
I (800564) secure_boot_v2: Verifying with RSA-PSS...
I (800614) secure_boot_v2: Signature verified successfully!
I (800664) ota_over_mqtt_demo:  Received: 289   Queued: 289   Processed: 289   Dropped: 0
I (801164) wifi:state: run -> init (0)
I (801164) wifi:pm stop, total sleep time: 677102328 us / 799967059 us

W (801164) wifi:<ba-del>idx
W (801164) wifi:<ba-del>idx
I (801164) wifi:new:<6,0>, old:<6,0>, ap:<255,255>, sta:<6,0>, prof:1
I (801174) core_mqtt_agent_manager: WiFi disconnected.
I (801174) app_wifi: Disconnected. Connecting to the AP again...
E (801184) esp-tls-mbedtls: read error :-0x004C:
I (801184) sub_pub_unsub_demo: coreMQTT-Agent disconnected. Preventing coreMQTT-Agent commands from being enqueued.
I (801194) ota_over_mqtt_demo: coreMQTT-Agent disconnected. Suspending OTA agent.
I (801204) core_mqtt_agent_manager: coreMQTT-Agent disconnected.
I (801214) temp_sub_pub_and_led_control_demo: coreMQTT-Agent disconnected. Preventing coreMQTT-Agent commands from being enqueued.I (801224) wifi:flush txq
I (801224) wifi:stop sw txq
I (801234) wifi:lmac stop hw txq
ESP-ROM:esp32c3-api1-20210207
Build:Feb  7 2021
rst:0x3 (RTC_SW_SYS_RST),boot:0xc (SPI_FAST_FLASH_BOOT)
Saved PC:0x403805d8
0x403805d8: esp_restart_noos_dig at C:/Users/wallit/Work/.espressif/frameworks/esp-idf-v4.4/components/esp_system/esp_system.c:46
(discriminator 1)

SPIWP:0xee
mode:DIO, clock div:1
Valid secure boot key blocks: 0
secure boot verification succeeded
load:0x3fcd6268,len:0x356c
load:0x403ce000,len:0x930
load:0x403d0000,len:0x5538
entry 0x403ce000
I (80) boot: ESP-IDF v4.4 2nd stage bootloader
I (80) boot: compile time 16:08:31
I (80) boot: chip revision: 3
I (81) boot.esp32c3: SPI Speed      : 80MHz
I (86) boot.esp32c3: SPI Mode       : DIO
I (91) boot.esp32c3: SPI Flash Size : 4MB
I (96) boot: Enabling RNG early entropy source...
I (101) boot: Partition Table:
I (105) boot: ## Label            Usage          Type ST Offset   Length
I (112) boot:  0 esp_secure_cert  unknown          3f 06 0000d000 00006000
I (120) boot:  1 nvs              WiFi data        01 02 00013000 00006000
I (127) boot:  2 otadata          OTA data         01 00 00019000 00002000
I (135) boot:  3 phy_init         RF data          01 01 0001b000 00001000
I (142) boot:  4 ota_0            OTA app          00 10 00020000 00190000
I (150) boot:  5 ota_1            OTA app          00 11 001b0000 00190000
I (157) boot:  6 storage          WiFi data        01 02 00340000 00010000
I (165) boot:  7 nvs_key          NVS keys         01 04 00350000 00001000
I (173) boot: End of partition table
I (224) esp_image: segment 0: paddr=001b0020 vaddr=3c0e0020 size=2e280h (189056) map
I (257) esp_image: segment 1: paddr=001de2a8 vaddr=3fc91800 size=01d70h (  7536) load
I (259) esp_image: segment 2: paddr=001e0020 vaddr=42000020 size=db530h (898352) map
I (421) esp_image: segment 3: paddr=002bb558 vaddr=3fc93570 size=01a14h (  6676) load
I (422) esp_image: segment 4: paddr=002bcf74 vaddr=40380000 size=11720h ( 71456) load
I (441) esp_image: segment 5: paddr=002ce69c vaddr=50000010 size=00010h (    16) load
I (441) esp_image: segment 6: paddr=002ce6b4 vaddr=00000000 size=0191ch (  6428)
I (448) esp_image: Verifying image signature...
I (452) secure_boot_v2: Verifying with RSA-PSS...
I (460) secure_boot_v2: Signature verified successfully!
I (469) boot: Loaded app from partition at offset 0x1b0000
I (469) secure_boot_v2: enabling secure boot v2...
I (475) secure_boot_v2: secure boot v2 is already enabled, continuing..
I (482) boot: Checking flash encryption...
I (487) flash_encrypt: flash encryption is enabled (1 plaintext flashes left)
I (494) boot: Disabling RNG early entropy source...
I (512) cpu_start: Pro cpu up.
I (520) cpu_start: Pro cpu start user code
I (520) cpu_start: cpu freq: 160000000
I (520) cpu_start: Application information:
I (523) cpu_start: Project name:     FeaturedFreeRTOSIoTIntegration
I (530) cpu_start: App version:      v202204.00-dirty
I (535) cpu_start: Compile time:     Apr 29 2022 16:24:04
I (542) cpu_start: ELF file SHA256:  922df577fdce440c...
I (547) cpu_start: ESP-IDF:          v4.4
I (552) heap_init: Initializing. RAM available for dynamic allocation:
I (559) heap_init: At 3FCACEB0 len 00013150 (76 KiB): DRAM
I (566) heap_init: At 3FCC0000 len 0001F060 (124 KiB): STACK/DRAM
I (572) heap_init: At 50000020 len 00001FE0 (7 KiB): RTCRAM
I (579) spi_flash: detected chip: generic
I (583) spi_flash: flash io: dio
W (587) flash_encrypt: Flash encryption mode is DEVELOPMENT (not secure)
I (595) sleep: Configure to isolate all GPIO pins in sleep state
I (602) sleep: Enable automatic switching of GPIO sleep configuration
I (609) coexist: coexist rom version 9387209
I (614) cpu_start: Starting scheduler.
I (619) main:
...
I (869) app_driver: Initializing Temperature sensor
I (909) ota_over_mqtt_demo: OTA over MQTT demo, Application version 0.0.1
I (909) pp: pp rom version: 9387209
I (919) AWS_OTA: otaPal_GetPlatformImageState
I (929) esp_ota_ops: aws_esp_ota_get_boot_flags: 1
I (929) esp_ota_ops: [1] aflags/seq:0x1/0x2, pflags/seq:0x2/0x1
I (939) AWS_OTA: Current State=[RequestingJob], Event=[Start], New state=[RequestingJob]
I (949) net80211: net80211 rom version: 9387209
I (919) temp_sub_pub_and_led_control_demo: Sending subscribe request to agent for topic filter: /filter/TempSubPubLED with id 1
I (959) ota_over_mqtt_demo:  Received: 0   Queued: 0   Processed: 0   Dropped: 0
I (969) wifi:wifi driver task: 3fcbcd44, prio:23, stack:6656, core=0
I (979) system_api: Base MAC address is not set
I (979) system_api: read default base MAC address from EFUSE
I (999) wifi:wifi firmware version: 7679c42
I (999) wifi:wifi certification version: v7.0
I (999) wifi:config NVS flash: enabled
I (999) wifi:config nano formating: disabled
I (1009) wifi:Init data frame dynamic rx buffer num: 32
I (1009) wifi:Init management frame dynamic rx buffer num: 32
I (1019) wifi:Init management short buffer num: 32
I (1019) wifi:Init dynamic tx buffer num: 32
I (1019) wifi:Init static tx FG buffer num: 2
I (1029) wifi:Init static rx buffer size: 1600
I (1029) wifi:Init static rx buffer num: 10
I (1039) wifi:Init dynamic rx buffer num: 32
I (1039) wifi_init: rx ba win: 6
I (1039) wifi_init: tcpip mbox: 32
I (1049) wifi_init: udp mbox: 6
I (1049) wifi_init: tcp mbox: 6
I (1059) wifi_init: tcp tx win: 5744
I (1059) wifi_init: tcp rx win: 5744
I (1069) wifi_init: tcp mss: 1440
I (1069) wifi_init: WiFi IRAM OP enabled
I (1069) wifi_init: WiFi RX IRAM OP enabled
W (1079) BTDM_INIT: esp_bt_mem_release not implemented, return OK
I (1089) wifi_prov_scheme_ble: BT memory released
I (1089) app_wifi: Already provisioned, starting Wi-Fi STA
W (1099) BTDM_INIT: esp_bt_mem_release not implemented, return OK
I (1099) wifi_prov_scheme_ble: BTDM memory released
I (1109) phy_init: phy_version 907,3369105-dirty,Dec  3 2021,14:55:12
I (1159) wifi:mode : sta (84:f7:03:5f:f1:40)
I (1159) wifi:enable tsf
I (1229) wifi:new:<6,0>, old:<1,0>, ap:<255,255>, sta:<6,0>, prof:1
I (1229) wifi:state: init -> auth (b0)
I (1229) wifi:state: auth -> assoc (0)
I (1229) wifi:state: assoc -> run (10)
W (1249) wifi:<ba-add>idx:0 (ifx:0, 8c:6a:8d:fc:31:8e), tid:0, ssn:0, winSize:64
I (1249) wifi:connected with Stranger 5, aid = 18, channel 6, BW20, bssid = 8c:6a:8d:fc:31:8e
I (1249) wifi:security: WPA2-PSK, phy: bgn, rssi: -62
I (1259) wifi:pm start, type: 1

I (1259) wifi:set rx beacon pti, rx_bcn_pti: 14, bcn_timeout: 14, mt_pti: 25000, mt_time: 10000
W (1269) wifi:<ba-add>idx:1 (ifx:0, 8c:6a:8d:fc:31:8e), tid:6, ssn:0, winSize:64
I (1329) wifi:BcnInt:102400, DTIM:1
I (1909) core_mqtt_agent_manager: WiFi connected.
I (1909) app_wifi: Connected with IP Address:10.0.0.140
I (1909) esp_netif_handlers: sta ip: 10.0.0.140, mask: 255.255.255.0, gw: 10.0.0.1
I (1969) ota_over_mqtt_demo:  Received: 0   Queued: 0   Processed: 0   Dropped: 0
I (2999) ota_over_mqtt_demo:  Received: 0   Queued: 0   Processed: 0   Dropped: 0
I (3179) coreMQTT: Packet received. ReceivedBytes=2.
I (3179) coreMQTT: CONNACK session present bit not set.
I (3179) coreMQTT: Connection accepted.
I (3179) coreMQTT: Received MQTT CONNACK successfully from broker.
I (3189) coreMQTT: MQTT connection established with the broker.
I (3189) core_mqtt_agent_manager: Session present: 0

I (3199) sub_pub_unsub_demo: coreMQTT-Agent connected.
I (3209) ota_over_mqtt_demo: coreMQTT-Agent connected. Resuming OTA agent.
I (3209) core_mqtt_agent_manager: coreMQTT-Agent connected.
I (3219) temp_sub_pub_and_led_control_demo: coreMQTT-Agent connected.
I (3229) sub_pub_unsub_demo: Task "SubPub0" sending subscribe request to coreMQTT-Agent for topic filter: /filter/SubPub0 with id
```

