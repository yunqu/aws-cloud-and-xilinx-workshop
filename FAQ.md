# FAQ

In this section we will provide answers to common questions that users may see
in this workshop.


### <a name="reboot"></a>How do I reset the Ultra96 board?

Press the POR_B / SW1 button on Ultra96 to initate a reset of the Ultra96 platform.  The POR_B button is located behind the two USB ports on the board.


### <a name="reboot"></a>What should I do after reboot on Ultra96?

Since the Greengrass core service is not started automatically after boot,
you need to [manually start it](#start-daemon).


### <a name="check-daemon"></a>How do I check whether Greengrass core service is running?

Run the following commands in the terminal to check whether the 
daemon process is running:

```shell
ps aux | grep /greengrass/gg/packages/1.7.0/bin/daemon
```

If the output contains a root entry for 
`/greengrass/ggc/packages/1.X.X/bin/daemon`, 
that means the daemon is running.


### <a name="start-daemon"></a>How do I start/restart Greengrass core service?

Start the daemon process by running:

```shell
sudo /greengrass/ggc/core/greengrassd start
```


### <a name="stop-daemon"></a>How do I stop Greengrass core service?
Stop the daemon process by running:

```shell
sudo /greengrass/ggc/core/greengrassd stop
```


### <a name="in-progress"></a>What should I do if the Greengrass group deployment is always in progress?

This is usually because your AWS Greengrass core service is not running in the 
background. First, [check whether the Greengrass core is running](#check-daemon).

If the Greengrass core is not running, 
[start the Greengrass core service](#start-daemon).

Once the daemon is running, [reset the group deployment](#reset) and 
[deploy the group again](#redeploy).


### <a name="failed"></a>What should I do if the Greengrass group deployment failed?

A typical cause of a failed group deployment is that your account or IoT 
things do not have the correct roles associated or policies attached.

First [reset your Greengrass group](#reset). Then you need to 
[recreate the Greengrass group](#recreate) without any error.


### <a name="reset"></a>How can I reset the Greengrass group deployment?

In the IoT Console, go to the Greengrass group being debugged and from 
the "Actions" menu select "Reset Deployments". You may also select 
"Do you want to force the reset" option.


### <a name="redeploy"></a>How can I deploy the same Greengrass group?

You can just run the following commands.

```shell
cd $HOME/aws-cloud-and-xilinx-workshop/cloud/script
./deploy-greengrass-group.sh <prefix>
```


### <a name="recreate"></a>How can I delete the Greengrass group and recreate the group?

Make sure the Greengrass group has been reset. 

Then you can delete the entire Greengrass group by selecting "Delete Group"
from the "Actions" menu.

Try to re-create the group and deploy. You should make sure all the steps 
execute without any error.

```shell
cd $HOME/aws-cloud-and-xilinx-workshop/edge/script
./greengrass-core-init.sh <prefix>
cd $HOME/aws-cloud-and-xilinx-workshop/cloud/script
./deploy-greengrass-group.sh <prefix>
```


### <a name="lambda"></a>How can I change the Lambda functions and then redeploy the Greengrass group?

If you have already completed the initial Lambda function build 
deployments created in Lab 2 by the `make-and-deploy-lambda.sh` script,
they should show up in the AWS Labmda service interface. 

Method 1:
* In AWS IoT Console, select "Services" and then "Lambda".
From the AWS Lambda interface you will see any function that you have built 
listed under the "Functions" menu. 
* Select the Lambda function that you want 
to change and you can view the source code in the "Function code" window. 
* Modify the code if you want.
* Once code modifications are completed, click "Save", then you select 
"Actions" menu and select "Publish new version" to make it available 
for deployment. Ensure that the Alias "PROD" is linked to the new version 
of the Lambda function. 
* When you deploy the Greengrass group, the updated
version should be automatically used.

Method 2:
* You can change the Python source code directly in the 
`$HOME/aws-cloud-and-xilinx-workshop/cloud/`.
* Then you can deploy those Lambda functions again using 
`make-and-deploy-lambda.sh`.


### <a name="delete-iot-things"></a>How can I delete the IoT things created by this workshop?

In the AWS IoT Console select "Manage" then "Things" from the left navigation pane.
Click on the "..." on the group and select "Delete".


### <a name="delete-iam-roles"></a>How can I delete the IAM roles created by this workshop?

In AWS IAM Management Console click "Roles" from the left navigation pane.
Select the roles you want to delete and click "Delete role". 

The roles created by this workshop are:
* "GreengrassServiceRole"
* "role-greengrass-group-\<prefix\>-gateway-ultra96"
* "lambdas-for-greengrass"

You may also want to disassociate the "GreengrassServiceRole" from your
account before deleting it.


### <a name="reset-ip"></a>Why is my board no longer able to access S3 bucket?

The bucket policy for the S3 bucket we created in the lab is restricting the 
access to the device (the same public IP address); so it is likely your
public IP has changed in this case.

Run the following the get the public IP:

```shell
curl ifconfig.co  --stderr /dev/null
```

Go to your S3 bucket web console. Click on the name of the S3 bucket created.
Go to "Permissions" tab, then "Bucket Policy". In the editor, change the 
original line into "aws:SourceIp": "<your.new.public.ip>/32".

### <a name="hardware"></a>Where can I get a set of hardware of my own?

The hardware is comprised of two main kits:
1. Ultra96
 * Ultra96 Board - AES-ULTRA96-G - [Manufacturer Link](https://www.avnet.com/shop/us/products/avnet-engineering-services/aes-ultra96-g-3074457345634920668/)
 * Ultra96 Power Supply - AES-ACC-U96-PWR - [Manufacturer Link](https://www.avnet.com/shop/us/products/avnet-engineering-services/aes-acc-u96-pwr-3074457345634920670/)
 * USB to Ethernet adapter -  [Example](https://www.amazon.com/Anker-Portable-Ethernet-Supporting-Notebook/dp/B00NOP70EC)
 * USB 2.0 A-Male to Micro B Cable - [Example](https://www.amazon.com/AmazonBasics-Male-Micro-Cable-Black/dp/B0711PVX6Z/ref=sr_1_1?ie=UTF8&qid=1539876427&sr=8-1&keywords=USB%2B2.0%2BCable%2B-%2BA-Male%2Bto%2BMicro-B%2Bmulti-pack&th=1)
 * Ethernet cable

2. USB3 Camera
 * eCon CU30 USB3 Camera - See3CAM_CU3 -[Manufacturer Link](https://www.e-consystems.com/ar0330-lowlight-usb-cameraboard.asp)

3. MicroZed IIoT Starter Kit.  The MicroZed IIoT Kit is made up of four hardware compoonents which are outlined below.
 * MicroZed 7010 SoM - AES-Z7MB-7Z010-G - [Manufacturer Link](https://www.avnet.com/shop/us/products/avnet-engineering-services/aes-z7mb-7z010-som-g-rev-f-3074457345635221615/)
 * MicroZed Arduino Carrier Card - AES-ARDUINO-CC-G - [Manufacturer Link](https://www.avnet.com/shop/us/products/avnet-engineering-services/aes-arduino-cc-g-3074457345635221613/)
 * ST Micro Sensor Shield - X-NUCLEO-IKS01A1 - [Manufacturer Link](https://www.st.com/en/ecosystems/x-nucleo-iks01a1.html)
 * Maxim Thermocouple Module - MAX31855PMB1# - [Manufacturer Link](https://www.maximintegrated.com/en/products/sensors/MAX31855.html)
 * USB 2.0 A-Male to Micro B Cable - [Example](https://www.amazon.com/AmazonBasics-Male-Micro-Cable-Black/dp/B0711PVX6Z/ref=sr_1_1?ie=UTF8&qid=1539876427&sr=8-1&keywords=USB%2B2.0%2BCable%2B-%2BA-Male%2Bto%2BMicro-B%2Bmulti-pack&th=1)
 * Ethernet cable


[Index](./README.md)


Copyright (C) 2018 Amazon.com, Inc. and Xilinx Inc.  All Rights Reserved.
