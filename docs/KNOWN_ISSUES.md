# Known Issues, Workarounds, etc.

Here there are some steps from [Getting Started Guide](GettingStartedGuide.md), that we need it to workaround to make it work.

## Useful Commands
- Find used port by ESP32C3: `ls /dev/tty.*`, e.g.:  /dev/tty.usbserial-1430

## Issues

- esptool.py: line 7: import: command not found
  - Run `get_idf`

- zsh: command not found: esptool.py
  - Run `get_idf`

- Enable support for legacy formats in ESP Secure Cert Manager. 
  - `Component config > ESP Secure Cert Manager -> Enable support for legacy formats`. 
  - Reference: https://github.com/FreeRTOS/iot-reference-esp32c3/issues/37

## Steps

### 2 Demo setup
### 2.3 Provision the ESP32-C3 with the private key, device certificate and CA certificate in Development Mode

1. Create the esp_secure_crt partition binary.

```
python components/esp_secure_cert_mgr/tools/configure_esp_secure_cert.py -p PORT --keep_ds_data_on_host --ca-cert CA_CERT_FILEPATH --device-cert DEVICE_CERT_FILEPATH --private-key PRIVATE_KEY_FILEPATH --target_chip esp32c3 --secure_cert_type cust_flash
```

e.g:
```
python managed_components/espressif__esp_secure_cert_mgr/tools/configure_esp_secure_cert.py -p /dev/tty.usbserial-1430 --keep_ds_data_on_host --ca-cert main/certs/AmazonRootCA1.pem --device-cert main/certs/certificate.pem.crt --private-key main/certs/private.pem.key --target_chip esp32c3 --secure_cert_type cust_flash
```

2. Write the esp_secure_crt partition binary (stored in esp_ds_data/esp_secure_crt.bin) to the ESP32-C3's flash by running the following command

```
esptool.py --no-stub --port PORT write_flash 0xD000 esp_ds_data/esp_secure_cert.bin
```

e.g:
```
esptool.py --no-stub --port /dev/tty.usbserial-1430 write_flash 0xD000 esp_secure_cert_data/esp_secure_cert.bin
```

### 3 Build and flash the demo project

1. Run the following command to build and flash the demo project:

```
idf.py -p PORT flash monitor
```

e.g:
```
idf.py -p /dev/tty.usbserial-1430 flash monitor
```

2. If the ESP32-C3 was previously Wi-Fi provisioned, and you are on a different network and wish to re-provision with new network credentials

```
parttool.py -p PORT erase_partition --partition-name=nvs
```

e.g:
```
parttool.py -p /dev/tty.usbserial-1430  erase_partition --partition-name=nvs
```

### 5 Perform firmware Over-the-Air Updates with AWS IoT
### 5.1 Setup pre-requisites for OTA cloud resources

#### OTA Update Service role

OTA Update Service role should end in something like this:

![OTA Update Service role 1](iot-reference-esp32c3-role-1.png)
![OTA Update Service role 2](iot-reference-esp32c3-role-2.png)

iot-reference-esp32c3-role-policy
```json
{
    "Version": "2012-10-17",
    "Statement": [
        {
            "Effect": "Allow",
            "Action": [
                "iam:GetRole",
                "iam:PassRole"
            ],
            "Resource": "arn:aws:iam::525045532992:role/iot-reference-esp32c3-role"
        }
    ]
}
```

iot-reference-esp32c3-s3-policy
```json
{
    "Version": "2012-10-17",
    "Statement": [
        {
            "Effect": "Allow",
            "Action": [
                "s3:GetObjectVersion",
                "s3:GetObject",
                "s3:PutObject"
            ],
            "Resource": [
                "arn:aws:s3:::iot-reference-esp32c3-updates/*"
            ]
        }
    ]
}
```