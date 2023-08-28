# OTA service role & policies
References to [Link](../GettingStartedGuide.md#51-setup-pre-requisites-for-ota-cloud-resources)

### OTA Update Service role

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