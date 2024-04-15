# Kelsus Camp

[1. Publish to LED handler Topic](#1-publish-to-led-handler-topic)
[2. Subscribe to Temperature Topic](#2-subscribe-to-temperature-topic)

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