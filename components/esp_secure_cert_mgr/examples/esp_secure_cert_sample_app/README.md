# ESP secure certificate sample app

The sample app demonstrates the use of APIs from *esp_secure_cert_mgr* to retrieve the contents of the *esp_secure_cert* partition. The example can also be used to verify the validity of the contents from the *esp_secure_cert* partition.

## Requirements
* The device must be pre-provisioned and have an *esp_secure_cert* partition.

## How to use the example
Before project configuration and build, be sure to set the correct chip target using `idf.py set-target <chip_name>`.
### Configure the project

* Selecting the type of *esp_secure_cert* partition:
The pre-provisioning utility supports the *esp_secure_cert* partition with two types, that are `nvs` and `cust_flash` partition.
You will have to select the respective partition type with which the module has been provisioned earlier.

Select the proper pre_prov partition type and respective csv file as follows:
#### 1) `esp_secure_cert` partition of type "cust_flash"
When the "esp_secure_cert" partition is of the "cust_flash" type, The data is directly stored on the flash in the raw format. Metadata is maintained at the start of the partition to manage the contents of the custom partition.

By default the type of *esp_secure_cert* partition is set to **cust_flash**.
Hence, No Additional configurations need to be done.


#### 2) `esp_secure_cert` partition of type "nvs"
When the "esp_secure_cert" partition is of "nvs" type, The data is directly stored on the flash in the "nvs" format. The "nvs_flash" component of ESP-IDF is used to handle the read/write operations of the data.

To use the esp_secure_cert_mgr for "nvs" type of partition. The following steps need to be followed:
1) Set the type of *esp_secure_cert_partition* to **nvs**
`Component config -> esp_secure_cert_mgr -> Choose the type of esp_secure_cert partition`
2) Select the appropriate partitions.csv file:
Set the custom partition file name to `partitions_nvs.csv` in
`Component config -> Partition Table -> Custom partition CSV file` 

### Build and Flash

Build the project and flash it to the board, then run the monitor tool to view the serial output:

```
idf.py -p PORT flash monitor
```

(Replace PORT with the name of the serial port to use.)

(To exit the serial monitor, type ``Ctrl-]``.)

See the Getting Started Guide for full steps to configure and use ESP-IDF to build projects.

### Example Output
```
I (331) sample_app: Device Cert:
Length: 1233
-----BEGIN CERTIFICATE-----
.
.
-----END CERTIFICATE-----

I (441) sample_app: CA Cert:
Length: 1285
-----BEGIN CERTIFICATE-----
.
.
-----END CERTIFICATE-----

I (561) sample_app: Successfuly obtained ciphertext, ciphertext length is 1200
I (571) sample_app: Successfuly obtained initialization vector, iv length is 16
I (571) sample_app: RSA length is 2048
I (581) sample_app: Efuse key id 1
I (581) sample_app: Successfully obtained the ds context
I (831) sample_app: Ciphertext validated succcessfully
```

## Addional configurations for `pre_prov` partition
Few of the modules which were pre-provisioned initially had the name of the pre-provisioning partition as `pre_prov`. For the modules which have pre-provisioning partition of name `esp_secure_cert` this part can be ignored.
For modules with the `pre_prov` partition of type *cust_flash* the configuration remain the same as the `esp_secure_cert` partition which is listed above.

For modules with `pre_prov` partition of type *nvs*, some additional configurations need to be done. The configurations can be done by simply replacing the *sdkconfig.defaults* file with *sdkconfig.pre_prov_nvs* file with following command:

```
cp sdkconfig.pre_prov_nvs sdkconfig.defaults
```
