̌# ESP Secure cert partition tests

This folder contains the test setup for testing he different flash formats supported by esp_secure_cert partition. The test is based on qemu and needs the qemu executables to be downloaded from [qemu/releases](https://github.com/espressif/qemu/releases)

## Generate qemu image

1) Go to esp_secure_cert_app directory: 
```
cd ../examples/esp_secure_cert_app/
```

2) Configure the project:
Please enable following menuconfig option to support all the formats:
* `Component config > ESP Secure Cert Manager -> Enable support for legacy formats`

2) Build the firmware:

```
idf.py build
```

3) Generate the qemu images for different flash formats:


- i) qemu image for cust_flash_tlv format:
```
./make-qemu-img.sh cust_flash_tlv/cust_flash_tlv.bin cust_flash_tlv/partition-table.bin cust_flash_tlv_qemu.img
```
> In this case the partition name in the partition table is `esp_secure_cert`.

- ii) qemu image for cust_flash_legacy format:
```
./make-qemu-img.sh cust_flash_legacy/cust_flash_legacy.bin cust_flash_legacy/partition-table.bin cust_flash_legacy_qemu.img
```
> In this case the partition name in the partition table is `esp_secure_cert`.

- iii) qemu image for cust_flash format:
```
./make-qemu-img.sh cust_flash/cust_flash.bin cust_flash/partition-table.bin cust_flash_qemu.img
```
> In this case the partition name in the partition table is `pre_prov`.

- iv) qemu image for nvs format:
```
./make-qemu-img.sh nvs/nvs.bin nvs/partition-table.bin nvs_qemu.img
```
> In this case the partition name in the partition table is `esp_secure_cert` and the nvs namespace name is `esp_secure_cert`.

- v) qemu image for nvs_legacy format:
```
./make-qemu-img.sh nvs_legacy/nvs_legacy.bin nvs_legacy/partition-table.bin nvs_legacy_qemu_new_part_name.img
```
> In this case the partition name in the partition table is `pre_prov` and the nvs namespace name is `pre_prov`.

- vi) qemu image for nvs_legacy format with new partition name:
```
./make-qemu-img.sh nvs_legacy/nvs_legacy.bin nvs/partition-table.bin nvs_legacy_qemu.img
```
> In this case the partition name in the partition table is `esp_secure_cert` and the nvs namespace name is `pre_prov`
4) Execute the binaries with qemu using following command:
```
qemu/bin/qemu-system-xtensa -nographic -machine esp32 -drive file=/* path to resp image*/,if=mtd,format=raw
```
