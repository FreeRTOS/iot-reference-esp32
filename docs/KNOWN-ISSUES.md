# Known Issues

- [esptool.py: line 7: import: command not found](#esptoolpy-line-7-import-command-not-found)
- [zsh: command not found: esptool.py](#zsh-command-not-found-esptoolpy)
- [Certs not found](#certs-not-found-issue)
- [Issues with git submodule](#issues-with-git-submodule-read)


### esptool.py: line 7: import: command not found
```bash
get_idf
```

### zsh: command not found: esptool.py
```bash
get_idf
```

### Certs not found. [issue](https://github.com/FreeRTOS/iot-reference-esp32c3/issues/37)
Error output => 
```bash
E (687) esp_secure_cert_tlv: Cpuld not find the tlv of type 1
E (687) esp_secure_cert_tlv: Cpuld not find header for TLV of type 1
```
To fix it => Enable support for legacy formats in ESP Secure Cert Manager. Run:
```bash
idf.py menuconfig
``` 
Then enable legacy formats, go to: `Component config > ESP Secure Cert Manager -> Enable support for legacy formats`. 

### Issues with git submodule. [Read](../Readme.md##cloning-the-repository)