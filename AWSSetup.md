# AWS IoT Core Setup
In case you already have an AWS account and user created, you can skip steps 1 and 2 and directly go to step 3 (Registering your board with AWS IoT).

## 1 Sign up for an AWS account

1. Open https://portal.aws.amazon.com/billing/signup.
2. Follow the online instructions. **NOTE:** Part of the sign-up procedure involves receiving a phone call and entering a verification code on the phone keypad.
3. Make a note of your AWS account number as it will be needed for the following steps.

## 2 Create an Administrator IAM user and grant it permissions

It’s strongly recommended that you adhere to the best practice of using the `Administrator` IAM user. The following steps show you how to create and securely lock away the root user credentials. One should only sign in as the root user to perform a few [account and service management tasks](https://docs.aws.amazon.com/general/latest/gr/root-vs-iam.html#aws_tasks-that-require-root).

1. Sign in to the [IAM console](https://console.aws.amazon.com/iam/) as the account owner by choosing **Root user** and entering your AWS account email address. On the next page, enter your password.
2. On the navigation bar, click your account name, and then click Account.

![image](https://user-images.githubusercontent.com/23126711/165860127-2e292c8e-7cd1-4ae6-a9b4-0465a3873e7b.png)

3. Scroll down to to **IAM User and Role Access to Billing Information** and click **Edit**.

![image](https://user-images.githubusercontent.com/23126711/165860178-43a17955-1e9b-4b06-81fc-a169b587c8e5.png)

4. Select the **Activate IAM Access** check box to activate access to the Billing and Cost Management console pages. This is necessary to create an IAM role with AdministrativeAccess in the following steps.

![image](https://user-images.githubusercontent.com/23126711/165860214-6c5aae4c-9ee0-4e07-8712-1cd7c067d175.png)

5. Click **Update**.
6. Return to the [IAM console](https://console.aws.amazon.com/iam/).
7. In the navigation pane, choose **Users** and then choose **Add user**.

![image](https://user-images.githubusercontent.com/23126711/165860249-faa9da14-cd2e-4e33-b1a1-97f279149dee.png)
![image](https://user-images.githubusercontent.com/23126711/165860274-6a3e30f3-a32d-4c49-806d-600b5e571e11.png)

8. For **User name**, enter `Administrator`. Select the check box next to **AWS Management Console access**. Then select **Custom password**, and then enter your new password in the text box.
9. (Optional) By default, AWS requires the new user to create a new password when first signing in. You can clear the check box next to **User must create a new password at next sign-in** to allow the new user to reset their password after they sign in.

![image](https://user-images.githubusercontent.com/23126711/165860310-579a0be4-8082-49c0-a6a9-1a7cf40b7088.png)

10. Click **Next: Permissions**.
11. Click **Create group**.

![image](https://user-images.githubusercontent.com/23126711/165860339-b3017148-3dec-4265-88a6-27a009e57a6b.png)

12. In the **Create group** dialog box, for **Group name** enter `Administrators`.
13. In the search box next to **Filter policies**, enter **Administrator Access.**
14. In the policy list, select the check box for **AdministratorAccess**. Then choose **Create group**.

![image](https://user-images.githubusercontent.com/23126711/165860389-02ac5479-fd38-4d4e-939c-fbae71e0bc37.png)

15. Back in the list of groups, select the check box for your new group. Choose **Refresh** if necessary to see the group in the list.

![image](https://user-images.githubusercontent.com/23126711/165860413-f07f5bee-18c2-455a-a18e-4926a78880da.png)

16. Click **Next: Tags**.
17. **(Optional)** Add metadata to the user by attaching tags as key-value pairs. For more information about using tags in IAM, see [Tagging IAM entities](https://docs.aws.amazon.com/IAM/latest/UserGuide/id_tags.html) in the *IAM User Guide*.
18. Click **Next: Review** to see the list of group memberships to be added to the new user. When you are ready to proceed, click **Create user**.

![image](https://user-images.githubusercontent.com/23126711/165860439-420b42d5-6303-47bd-9f6b-12c936326aca.png)

19. You should now see a page like the following, stating that your IAM user has been created. Click **Download .csv** in order to save your IAM credentials - store these in a safe place. These are necessary for signing into the AWS console with your newly created IAM user.

![image](https://user-images.githubusercontent.com/23126711/165860511-ad970190-ccfe-4df0-86c8-ab0c67b9b4d0.png)

20. On the navigation bar, click your account name, and then click **Sign Out**.
21. Click **Sign In to the Console** or go to https://console.aws.amazon.com/.
22. Select **IAM user** and enter your Account ID noted earlier, then click **Next**.

![image](https://user-images.githubusercontent.com/23126711/165860545-3d4d156b-d4ac-4e32-92b0-5b1161ce49b5.png)

23. Enter **Administrator** in **IAM user name,** enter the **Password** set earlier for the IAM user, then click **Sign in**.

![image](https://user-images.githubusercontent.com/23126711/165860570-dc8b1961-eb6c-4782-abd3-1ca0de07b91c.png)

24. You are now signed in as your **Administrator** IAM user. This IAM user will be used for following steps.

## 3 Registering your board with AWS IoT

Your board must be registered with AWS IoT to communicate with the AWS Cloud. To register your board with AWS IoT, you must have:

* **An AWS IoT policy:** An AWS IoT policy grants your device permissions to access AWS IoT resources. It is stored in the AWS Cloud.
* **An AWS IoT thing:** An AWS IoT thing allows you to manage your devices in AWS IoT. It represents your device and is stored in the AWS Cloud.
* **A private key and X.509 certificate:** A private key and its corresponding public key certificate allow your device to authenticate and communicate securely with AWS IoT.
* **An AWS endpoint:** An AWS endpoint is the URL, or the AWS IoT Core entry point, your devices will connect to.

### 3.1 Creating an AWS IoT policy

1. Go to the [AWS IoT console](https://console.aws.amazon.com/iotv2/).
2. In the navigation pane, click **Secure**, then click **Policies** in the drop-down menu.

![image](https://user-images.githubusercontent.com/23126711/165860604-9bc500fe-d1b2-45c8-a997-eb527a0c144c.png)

3. Click **Create Policy**.
4. Enter a name to identify the policy. For this example, the name **DevicePolicy** will be used.

![image](https://user-images.githubusercontent.com/23126711/165860619-ef9dfcf0-6e08-454b-a267-2cdfb8c6b2f9.png)

5. Identify your **AWS region** by clicking the location dropdown menu at the top of the page to the left of the account dropdown menu. The **AWS region** is the orange text on the right. For this example, the **AWS region** will be **us-west-2**.

![image](https://user-images.githubusercontent.com/23126711/165860649-05961265-f2a2-4608-a62c-03076bf16506.png)

6. Scroll down to **Policy document** and click the **JSON** button.

![image](https://user-images.githubusercontent.com/23126711/165860676-e4ad5798-7049-45fc-90ed-cfa9b6c646a8.png)

7. Inside the **Policy document**, copy and paste the following JSON into the policy editor window. Replace `aws-region` and `aws-account-id` with your AWS Region and Account ID.

```
{
    "Version": "2012-10-17",
    "Statement": [
    {
        "Effect": "Allow",
        "Action": "iot:Connect",
        "Resource":"arn:aws:iot:aws-region:aws-account-id:*"
    },
    {
        "Effect": "Allow",
        "Action": "iot:Publish",
        "Resource": "arn:aws:iot:aws-region:aws-account-id:*"
    },
    {
         "Effect": "Allow",
         "Action": "iot:Subscribe",
         "Resource": "arn:aws:iot:aws-region:aws-account-id:*"
    },
    {
         "Effect": "Allow",
         "Action": "iot:Receive",
         "Resource": "arn:aws:iot:aws-region:aws-account-id:*"
    }
    ]
}
```

![image](https://user-images.githubusercontent.com/23126711/165860693-358be23a-c2c1-4c6d-9398-9f93c3b020d0.png)

This policy grants the following permissions:

* **iot:Connect:** Grants your device the permission to connect to the AWS IoT message broker with any client ID.
* **iot:Publish:** Grants your device the permission to publish an MQTT message on any MQTT topic.
* **iot:Subscribe:** Grants your device the permission to subscribe to any MQTT topic filter.
* **iot:Receive:** Grants your device the permission to receive messages from the AWS IoT message broker on any MQTT topic.

For more information regarding [IAM policy creation](https://docs.aws.amazon.com/IAM/latest/UserGuide/access_policies_create.html) refer to [Creating IAM policies](https://docs.aws.amazon.com/IAM/latest/UserGuide/access_policies_create.html).

8. Click **Create**.

### 3.2 Creating an IoT thing, private key, and device certificate

1. Go to the [AWS IoT console](https://console.aws.amazon.com/iotv2/).
2. In the navigation pane, choose **Manage**, and then choose **Things**.

![image](https://user-images.githubusercontent.com/23126711/165860729-68ddc292-71a7-4bc6-a830-b671b7b5c5cf.png)

3. Click **Create things.**

![image](https://user-images.githubusercontent.com/23126711/165860761-43739bfc-47b8-4e07-9c19-70cc1964ad27.png)

4. Select **Create a single thing** then click **Next**.

![image](https://user-images.githubusercontent.com/23126711/165860791-b136b6c5-83f6-4860-aae2-9f5bbb37cdf1.png)

5. Enter a name for your thing, and then click **Next**. For this example, the thing name will be **ExampleThing**.

![image](https://user-images.githubusercontent.com/23126711/165860814-36cb347e-d9f0-433a-a954-c24ac04e84cc.png)

6. Select **Auto-generate a new certificate**, then click **Next**.

![image](https://user-images.githubusercontent.com/23126711/165860839-2a5d36c5-6b09-47cd-853e-3b3241981eb5.png)

7. Attach the policy you created above by searching for it and clicking the checkbox next to it, then click **Create thing**. **NOTE:** Multiple policies can be attached to a thing.

![image](https://user-images.githubusercontent.com/23126711/165860858-4bda82f0-1159-4fc3-9dfd-e21fe3276d18.png)

8. Download your **Private Key**, **Public Key**, and **Device Certificate** by clicking the **Download** links for each. These will be used by your device to connect to AWS IoT Core. **NOTE:** The private key and public key can only be downloaded here, so store them in a safe place. Also download the Amazon Root CA 1 certificate.

![image](https://user-images.githubusercontent.com/23126711/165860877-e4c08025-fb51-4718-8b22-839d20dd1971.png)

### 3.3 Obtaining your AWS endpoint

1. Go to [AWS IoT console](https://console.aws.amazon.com/iotv2/).
2. In the navigation pane, click **Settings**.

![image](https://user-images.githubusercontent.com/23126711/165860893-20a44ef2-d13b-494a-841b-df1092a79e9b.png)

3. Your **AWS endpoint** is under **Device data endpoint**. Note this endpoint as it will be used later when setting up your board.

![image](https://user-images.githubusercontent.com/23126711/165860926-55c402db-092c-4605-9143-8aaeae573988.png)
