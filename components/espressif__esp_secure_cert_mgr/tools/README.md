# esp_secure_cert Configuration Tool
The script [configure_esp_secure_cert.py](https://github.com/espressif/esp_secure_cert_mgr/blob/main/tools/configure_esp_secure_cert.py) is used for configuring the ESP platform with PKI credentials into the esp_secure_cert partition which shall reside on its flash storage.
It also configures the DS peripheral on the ESP32-S2/ESP32-S3/ESP32-C3 SoC. The steps in the script are based on technical details of certain operations in the Digital Signature calculation, which can be found in the Digital Signature Section of [ESP32-S2 TRM](https://www.espressif.com/sites/default/files/documentation/esp32-s2_technical_reference_manual_en.pdf).

The script generates a partition named `esp_secure_cert` on host machine, that contains the parameters required by the DS peripheral. The `configure_esp_secure_cert.py` utility automatically flashes the partition to the ESP platform which has been connected.

# Installation

```
pip install esp-secure-cert-tool
```

# Configuration

1) Generate root ca and key pair:
```
openssl req -newkey rsa:2048 -nodes -keyout prvtkey.pem -x509 -days 3650 -out cacert.pem -subj "/CN=Test CA"
```

2) Generate client private key:
```
openssl genrsa -out client.key
```

3) Generate device cert:
```
openssl req -out client.csr -key client.key -new
openssl x509 -req -days 365 -in client.csr -CA cacert.pem -CAkey prvtkey.pem  -sha256 -CAcreateserial -out client.crt
```

# Generate `esp_secure_cert` partition
Following commands can be used to configure the DS peripheral and generate the `esp_secure_cert` partition.
The script can generate `cust_flash` as well as `nvs` type of `esp_secure_cert` partition. Please refer [upper level README](../README.md) for more details about type of partitions.

* When configuring the DS peripheral, by default the configuration script does not enable the read protection for the efuse key block in which the DS key is programmed. This is done for allowing flexibility while using the script for development purpose. Please provide the `--production` option as an additional argument to below command/s to enable the read protection for the respective efuse key block.

* Please remove the `--configure_ds` argument from these commands if use of the DS peripheral is disabled in the menu config.
> **WARNING**: This is not recommended for production purpose as the private key shall be stored as plaintext.

## Generate `esp_secure_cert` partition of type `cust_flash_tlv`:

This command shall generate a binary partition containing the PKI credentials stored in the TLV format.
```
configure_esp_secure_cert.py -p /* Serial port */ --keep_ds_data_on_host --efuse_key_id 1 --ca-cert cacert.pem --device-cert client.crt --private-key client.key --target_chip /* target chip */ --secure_cert_type cust_flash_tlv --configure_ds
```

## Legacy partition formats:

1. Generate `esp_secure_cert` partition of type `cust_flash`:

```
configure_esp_secure_cert.py -p /* Serial port */ --keep_ds_data_on_host --efuse_key_id 1 --ca-cert cacert.pem --device-cert client.crt --private-key client.key --target_chip /* target chip */ --secure_cert_type cust_flash --configure_ds
```

2. Generate `esp_secure_cert` partition of type `nvs`:
```
configure_esp_secure_cert.py -p /* Serial port */ --keep_ds_data_on_host --efuse_key_id 1 --ca-cert cacert.pem --device-cert client.crt --private-key client.key --target_chip /* target chip */ --secure_cert_type nvs --configure_ds
```

## Additional options for the utility
The following options can be provided additional to the main arguments given above.

### Set the `esp_secure_cert` partition offset
By default the `esp_secure_cert` partition shall be flashed at an offset of `0xD000` by the utility.
In order to flash the `esp_secure_cert` partition at a different offset, the following argument can be provided additionally to the configure_esp_secure_cert.py script.

```
configure_esp_secure_cert.py  --sec_cert_part_offset /* offset value in hex e.g. 0xD000 */
```
### Skip automatic flashing of `esp_secure_cert` partition

By default the `esp_secure_cert` partition shall be flashed automatically at the offset value provided to `sec_cert_part_offset` argument.
In order to stop the flashing process the following argument can be provided additionally to the configure_esp_secure_cert.py script

```
configure_esp_secure_cert.py --skip_flash
```

### Help section
The additional options supported by the utility can be found at `configure_esp_secure_cert.py --help`.
