# Getting Started Guide With Security Features

## 1 Pre-requisites

In the [GettingStartedGuide](GettingStartedGuide.md), one would have setup the ESP32-C3 device, installed the required software, setup AWS IoT, configured the demo project with the AWS IoT endpoint, Thing, private key and certificates, and built and run the demo.

## 2 Enable the DS peripheral

1. Run `idf.py menuconfig`
2. Select `Reference Integration`.
3. Select `Use DS Peripheral`.
4. Go back to the main menu.

## 3 Enable flash encryption

5. Select `Security features`.
6. Set `Enable flash encryption on boot (READ DOCS FIRST)` to true.
7. Select `Enable usage mode`.
8. Set `Development (NOT SECURE)` to true.
9. Go back to `Security features`.
10. Go back to main menu, Save and Exit.

## 4 Provision the ESP32-C3 with the private key, device certificate and CA certificate in Development Mode
The key and certificates which will be used to establish a secure TLS connection will be encrypted and stored in a special flash partition.

1. Create the `esp_secure_crt` partition binary. If this is the first time running this command, an eFuse block in the ESP32-C3 will be burnt with a generated key and this **CANNOT** be reversed:
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

1. For Secure Boot, an RSA 3072 private key must be generated which will be used to sign the secondary bootloader and the application binary. Please refer to the Secure Boot section in the [Featured IoT Reference Integration page for the ESP32-C3](https://www.freertos.org/ESP32C3) on FreeRTOS.org for further details. The private key can be generated with the following command:
```
openssl genrsa -out secure_boot_signing_key.pem 3072
```
This will output `secure_boot_signing_key.pem`, which can be renamed as you see fit. Keep this key in a safe place as it will be necessary for signing binaries in the future.
Note: If you have installed openssl and the openssl command fails with a command not found error, please ensure you have the openssl path exported when using your terminal/command prompt.

2. Run `idf.py menuconfig`
3. Select `Security features`.
4. Set `Enable hardware Secure Boot in bootloader (READ DOCS FIRST)` to true.
5. Set `Sign binaries during build` to true.
6. Set `Secure boot private signing key` to the path to the RSA 3072 private key you generated in step 1.
7. Go back to main menu, Save and Exit.

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
2. Flash the bootloader by copying and pasting the command under "Secure boot enabled, so bootloader not flashed automatically," (the second block of text) replacing:
**PORT** with the serial port to which the ESP32-C3 is connected. (Do not include the opening and closing braces around PORT in the command)
**BAUD** with 460800.

## 6 Build and flash the demo project

With Secure Boot enabled, application binaries must be signed before being flashed. With the configurations set in this document, this is automatically done any time a new application binary is built. Binaries are automatically signed using the RSA key we generated and configured in section 2.2 (Configure Secure Boot).

If flash encryption is enabled, the bootloader will generate the private key used to encrypt flash and store it in the ESP32-C3's eFuse. It will then encrypt the bootloader, the partition table, all `app` partitions, and all partitions marked `encrypted` in the partition table. 

Run the following command to build and flash the demo project:
```
idf.py -p PORT flash monitor
```
Replace **PORT** with the serial port to which the ESP32-C3 is connected.

## 7 Monitoring the demo

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

2. On the AWS IoT console, select "Test" then select "MQTT test client". Under "Subscribe to a topic", type "#" (# is to select all topics. You can also enter a specific topic such as /filter/Publisher0), click on "Subscribe", and confirm that the MQTT messages from the device are received.

## 8 Perform firmware Over-the-Air Updates with AWS IoT

In the previous [GettingStartedGuide](GettingStartedGuide.md), you would have setup the required OTA cloud resources.

### 8.1 Build an application binary with a higer version number, to be downloaded and activated on the device 

Create a binary with a higher version number. 
1. Run `idf.py menuconfig`
2. Select `Reference Integration` from the menu.
3. Under `Enable OTA demo` go to `OTA demo configurations`
4. Set the `Application version build` number to '1'.
5. Go back to main menu, Save and exit.
6. Run the following command to only build the demo project.
```
idf.py build
```
If successful, there will be a new binary under the 'build' directory - build/GoldenReferenceIntegration.bin. Copy this binary to another location, else it will be overwritten in the next step.

### 8.2 Build and flash the device with a binary with a lower version number
1. Follow the same steps in 8.1, but this time, set the `Application version build` number to '0'.
2. Build and flash this new application binary with a lower version number.
```
idf.py -p PORT flash monitor
```

### 8.3 Upload the binary with the higher version number (created in step 8.1) and create an OTA Update Job
1. In the navigation pane of the AWS IoT console, choose 'Manage', and then choose 'Jobs'.
Choose 'Create a job'.
2. Next to 'Create a FreeRTOS Over-the-Air (OTA) update job', choose 'Create FreeRTOS OTA update job'. Provide a name for the job and click on 'Next'.
3. You can deploy an OTA update to a single device or a group of devices. Under 'Devices to update', select the Thing you would have created earlier. You can find it listed under AWS IoT->Manage->Things. If you are updating a group of devices, select the check box next to the thing group associated with your devices. 
4. Under 'Select the protocol for file transfer', choose 'MQTT'.
5. Under 'Sign and choose your file', choose 'Sign a new file for me'.
6. Under 'Code signing profile', choose the code signing profile you would have created earlier.
7. Under 'File', choose 'Upload a new file' then click 'Choose file'. A file browser pops up. Select the signed binary image with the higher version number.
8. Under 'File upload location in S3', click 'Browse S3', then select the S3 bucket that you had earlier created for this job. Click 'Choose'
9. Under 'Path name of file on device', type 'NA'
10. Under 'IAM role for OTA update job', choose the role that you created earlier for the OTA update from the drop down list.
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

Once all the firmware image chunks are downloaded and the signature is validated, the device reboots with the new image. See the OTA section in the [Featured IoT Reference Integration page for the ESP32-C3](https://www.freertos.org/ESP32C3) on FreeRTOS.org for more details.
You can see the new version number of the demo binary. Look for the string "Application version"

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

