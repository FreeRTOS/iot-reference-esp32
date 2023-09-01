# Useful Commands

- [Find used port](#find-used-port)
- [Build and flash the project](#build-and-flash-the-project)
- [If the ESP32-C3 was previously Wi-Fi provisioned, and you are on a different network and wish to re-provision with new network credentials](#if-the-esp32-c3-was-previously-wi-fi-provisioned-and-you-are-on-a-different-network-and-wish-to-re-provision-with-new-network-credentials)

* Always run this commands with the ESP-IDF Terrminal, otherwise you will receive an error `zsh: command not found: idf.py`. If you installed the VS Code plugin properly you will find it in the `Command Palette`.

### Find used port
Find used port by ESP32C3: `ls /dev/tty.*`, e.g.:  /dev/tty.usbserial-1430
```bash
ls /dev/tty.*
```

### Build and flash the project 
References to [Link](../GettingStartedGuide.md#3-build-and-flash-the-demo-project)

Exports variables
```bash
export PORT=/dev/tty.usbserial-1440
export CA_CERT_FILEPATH=main/certs/AmazonRootCA1.pem
export DEVICE_CERT_FILEPATH=main/certs/certificate.pem.crt
export PRIVATE_KEY_FILEPATH=main/certs/private.pem.key
```
* You can use the result of the `ls /dev/tty.usb*` command as input of the PORT variale, this only works if the command returns one result.
```bash
export PORT=$(ls /dev/tty.usb*)
```
Flash firmware and open monitor
```bash
idf.py -p $PORT flash monitor
```

All in one command: Flash firmware and open monitor
```bash
idf.py -p $(ls /dev/tty.usb*) flash monitor
```

### If the ESP32-C3 was previously Wi-Fi provisioned, and you are on a different network and wish to re-provision with new network credentials

Exports variables
```bash
export PORT=/dev/tty.usbserial-1440
export CA_CERT_FILEPATH=main/certs/AmazonRootCA1.pem
export DEVICE_CERT_FILEPATH=main/certs/certificate.pem.crt
export PRIVATE_KEY_FILEPATH=main/certs/private.pem.key
```
Clean `nvs` partition
```bash
parttool.py -p $PORT erase_partition --partition-name=nvs
```
```bash
# MAC systems only
parttool.py -p $(ls /dev/tty.usb*) erase_partition --partition-name=nvs
```