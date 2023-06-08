# COMMANDS

MQTT endpoint - a3dvihr7cl0cbk-ats.iot.us-east-1.amazonaws.com

### 2.3 Provision the ESP32-C3 with the private key, device certificate and CA certificate in Development Mode

1. Create the esp_secure_crt partition binary.

python components/esp_secure_cert_mgr/tools/configure_esp_secure_cert.py -p PORT --keep_ds_data_on_host --ca-cert CA_CERT_FILEPATH --device-cert DEVICE_CERT_FILEPATH --private-key PRIVATE_KEY_FILEPATH --target_chip esp32c3 --secure_cert_type cust_flash

python managed_components/espressif__esp_secure_cert_mgr/tools/configure_esp_secure_cert.py -p /dev/tty.usbserial-1440 --keep_ds_data_on_host --ca-cert main/certs/AmazonRootCA1.pem --device-cert main/certs/certificate.pem.crt --private-key main/certs/private.pem.key --target_chip esp32c3 --secure_cert_type cust_flash

2. Write the esp_secure_crt partition binary (stored in esp_ds_data/esp_secure_crt.bin) to the ESP32-C3's flash by running the following command

esptool.py --no-stub --port PORT write_flash 0xD000 esp_ds_data/esp_secure_cert.bin

esptool.py --no-stub --port /dev/tty.usbserial-1440 write_flash 0xD000 esp_secure_cert_data/esp_secure_cert.bin

### 3 Build and flash the demo project

0. If you are setting up the ESP32-C3 for the first time.

parttool.py -p PORT erase_partition --partition-name=nvs

parttool.py -p /dev/tty.usbserial-1440  erase_partition --partition-name=nvs

1. Run the following command to build and flash the demo project:

idf.py -p PORT flash monitor

idf.py -p /dev/tty.usbserial-1440 flash monitor

### Issues
https://github.com/FreeRTOS/iot-reference-esp32c3/issues/37