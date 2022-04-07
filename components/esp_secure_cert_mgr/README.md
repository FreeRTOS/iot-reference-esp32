# ESP SECURE CERTIFICATE MANAGER

The *esp_secure_cert_mgr* is a simplified interface to access the PKI credentials of a device pre-provisioned with the Espressif Provisioning Service.
The component is already a part of the [IDF Component Manager](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/tools/idf-component-manager.html).

## What is Pre-Provisioning?

With the Espressif Pre-Provisioning Service, the ESP modules are pre-provisioned with an encrypted RSA private key and respective X509 public certificate before they are shipped out to you. The PKI credentials can then be registered with the cloud service to establish a secure TLS channel for communication. With the pre-provisioning taking place in the factory, it provides a hassle-free PKI infrastructure to the Makers. You may use this repository to set up your test modules to validate that your firmware works with the pre-provisioned modules that you ordered through Espressif's pre-provisioning service.

## `esp_secure_cert` partition
When a device is pre-provisioned that means the PKI credentials are generated for the device. The PKI credentials are then stored in a partition named
*esp_secure_cert*.
For esp devices that support DS peripheral, the pre-provisioning is done by leveraging the security benefit of the DS peripheral. In that case, all of the data which is present in the *esp_secure_cert* partition is completely secure.

When the device is pre-provisioned with help of the DS peripheral then by default the partition primarily contains the following data:
1) Device certificate: It is the public key/ certificate for the device's private key. It is used in TLS authentication.
2) CA certificate: This is the certificate of the CA which is used to sign the device cert.
3) Ciphertext: This is the encrypted private key of the device. The ciphertext is encrypted using the DS peripheral, thus it is completely safe to store on the flash.

As listed above, the data only contains the public certificates and the encrypted private key and hence it is completely secure in itself. There is no need to further encrypt this data with any additional security algorithm.

The `esp_secure_cert` partition can be of two types:
1) *cust_flash*: In this case, the partition is a custom flash partition. The data is directly stored over the flash.
2) *nvs partition*: In this case, the partition is of the `nvs` type. the `nvs_flash` abstraction layer from the ESP-IDF is used to store and then retreive the contents of the `esp_secure_cert` partition.


## How to use the `esp_secure_cert_mgr` in your project ?
The *esp_secure_cert_mgr* provides the set of APIs that are required to access the contents of the `esp_secure_cert` partition. The information on using the *esp_secure_crt_mgr* component with help of the IDF component manager for your project can be found at [Using with a project](https://github.com/espressif/idf-component-manager#using-with-a-project). A demo example has also been provided with the `esp_secure_cert_mgr`, more details can be found out in the [example README](examples/esp_secure_cert_sample_app/README.md).

To use *esp_secure_crt_mgr* in a project, some configurations related to the type of *esp_secure_cert* partition need to be done. The instruction to configure the project for two types of *esp_secure_cert* are given below.

> To use the component with your project you need to know the type of *esp_secure_cert* partition of your pre-provisioned device.

## 1) `esp_secure_cert` partition of type "cust_flash"
When the "esp_secure_cert" partition is of the "cust_flash" type, The data is directly stored on the flash in the raw format. Metadata is maintained at the start of the partition to manage the contents of the custom partition.
The contents of the partition are already public or encrypted, hence they are perfectly safe to be stored in the unencrypted format.
![](esp_secure_cert_cust_flash.jpeg)

To use the *esp_secure_cert_mgr* for "cust_flash" type of partition.
The following steps need to be completed
1) Enable the following menuconfig option
`CONFIG_ESP_SECURE_CERT_CUST_FLASH_PARTITION`
2) Select the appropriate partitions.csv file:
The partitions.csv file should contain the following line which enables it to identify the `esp_secure_cert` partition.

```
# Name, Type, SubType, Offset, Size, Flags
esp_secure_cert, 0x3F, , 0xD000, 0x6000,
```

## 2) `esp_secure_cert` partition of type "nvs"
When the "esp_secure_cert" partition is of "nvs" type, The data is directly stored on the flash in the "nvs" format. The "nvs_flash" component of ESP-IDF is used to handle the read/write operations of the data.
The contents of the partition are either public or encrypted, hence they are perfectly safe to be stored in the unencrypted format.

To use the esp_secure_cert_mgr for "nvs" type of partition. The following steps need to be followed:
1) Enable the following menuconfig option
`CONFIG_ESP_SECURE_CERT_NVS_PARTITION`
2) Select the appropriate partitions.csv file:
The partitions.csv file should contain the following line which enables it to identify the `esp_secure_cert` partition.

```
# Name, Type, SubType, Offset, Size, Flags
esp_secure_cert, data, nvs, 0xD000, 0x6000,
esp_secure_cert_keys, data, nvs_keys, 0x13000, 0x1000,
```
