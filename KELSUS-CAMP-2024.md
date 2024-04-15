# Kelsus Camp 2024

- [0. Prerequisites](#0-prerequisites)
  - [0.1 Read README](#01-read-readme)
  - [0.2 Device WiFi Credentials](#0.2-device-wifi-credentials)
- [1. Publish to LED handler Topic](#1-publish-to-led-handler-topic)
- [2. Subscribe to Temperature Topic](#2-subscribe-to-temperature-topic)

## 0. Prerequisites

### 0.1 Read [README](./README.md)

### 0.2 Device WiFi Credentials
In `SoftAp` mobile application in `Provisioned seetings`, then in `Encrypted Communication` choose `Secured`.

## 1. Publish to LED handler Topic
Publish to `/filter/%s` to request LED changes. Replace %s for the thing name. 

To power on the LED publish to `filter/dev-joaquin`:
```json
{
  "led":{
    "power": 1
  }
}
```


To power off the LED publish to `filter/dev-joaquin`:
```json
{
  "led":{
    "power": 0
  }
}
```


## 2. Subscribe to Temperature Topic
Subscribe to `/filter/%s` to listen to temperature changes. Replace %s for the thing name. 

Subscribe to `filter/dev-joaquin`, and you will receive a message like this:
```json
{
  "temperatureSensor": {
    "thingName": "dev-joaquin",
    "temperatureValue": 0.000000,
    "iteration": 432
  }
}
```