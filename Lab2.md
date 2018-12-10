# Lab 2. Establish AWS Greengrass Connectivity

In this lab we will establish basic connectivity to the AWS cloud for Ultra96 device running AWS Greengrass.  In order to do this, we need to establish unique device identities that enable authentication to AWS IoT Core.  This is done through a client certificate and private key.  You will need to create these credentials in your AWS account and then configure them to each device.

## Configure AWS IoT Credentials

In this section, you will configure and deploy AWS IoT Core credentials. The physical credential files, the private key and the certificate for each device will be placed in ```$HOME/aws-cloud-and-xilinx-workshop/edge/auth-PREFIX-node-zynq7k``` and ```$HOME/aws-cloud-and-xilinx-workshop/edge/auth-PREFIX-gateway-ultra96```, where PREFIX is your greengrass group and s3 bucket prefix.

1. On the Ultra96 debug interface navigate to the directory containing the scripts for deploying cloud objects.

   ```bash
   cd $HOME/aws-cloud-and-xilinx-workshop/cloud/script
   ```
   
2. Run the script that configures the credentials for the devices to connect to your AWS account through AWS IoT. The edge hardware that you are using in this workshop is uniquely identified with a group prefix within your AWS account. This allows people at multiple tables who may be sharing a corporate AWS account to operate with their own hardware. Make the prefix match the same value used in the previous lab.

   ```bash
   ./deploy-awsiot-objects.sh <prefix>
   ```

   When the script completes, the keys and certificates will be in the directories specified above.

3. To prepare the system for Labs 4 and 5 connect the eCon USB camera to the Ultra96 board J8 prior to deploying your Greengrass group.  See the picture below showing Ultra96 with the camera connected.

   ![alt text](images/Ultra96_WithCamera.jpg?raw=true "Ultra96 with USB Camera")


## Configure and Deploy AWS Greengrass on Xilinx Ultra96

The Ultra96 runs Linux and AWS Greengrass. Use the following steps to prepare the credentials for your Ultra96 board,
so that your Ultra96 can be used as a greengrass core.

1. Copy the private key and certificate to AWS Greengrass.

   ```bash
   sudo cp $HOME/aws-cloud-and-xilinx-workshop/edge/auth-*gateway-ultra96/*pem /greengrass/certs/
   ```

2. Copy the AWS Greengrass configuration file ```config.json``` to the AWS Greengrass installation.

   ```bash
   sudo cp $HOME/aws-cloud-and-xilinx-workshop/edge/auth-*gateway-ultra96/config.json /greengrass/config/
   ```

3. With the certificates and configuration file in place,  we can start 
   the AWS Greengrass core service. Run the following commands in the 
   Ultra96 terminal window.

	```bash
	sudo /greengrass/ggc/core/greengrassd start
	```
	Verify that you see the Greengrass daemon start by seeing a response in the CLI of "Greengrass successfully started with PID: XXXX".

4. We will now build a number of AWS Greengrass edge Lambda functions used throughout the workshop.  First build and upload the AWS Lambda function named ```xilinx-hello-world```.

	```bash
	cd $HOME/aws-cloud-and-xilinx-workshop/cloud/script
	./make-and-deploy-lambda.sh xilinx-hello-world
	```

	You can see from its output that the script performs the following acts:

	- Creates a Role for the function if the Role does not already exist
	- Packages the code located in ```$HOME/aws-cloud-and-xilinx-workshop/cloud/xilinx-hello-world``` to a zip file
	- Uploads the code to the AWS Lambda service
	- Applies a version number to the function
	- Creates an alias for the function

5. Although in this lab we are only using one AWS Lambda function, we repeat this step for all the AWS Lambda
   functions to save time for later labs.
   
	```bash
	./make-and-deploy-lambda.sh xilinx-bitstream-deploy-handler
	./make-and-deploy-lambda.sh xilinx-image-upload-handler
	./make-and-deploy-lambda.sh xilinx-video-inference-handler
	./make-and-deploy-lambda.sh aws_xilinx_workshop_core_shadow_proxy_handler
	./make-and-deploy-lambda.sh aws_xilinx_workshop_intelligent_io_error_handler
	./make-and-deploy-lambda.sh aws_xilinx_workshop_telemetry_enrichment_handler
	./make-and-deploy-lambda.sh aws_xilinx_workshop_aws_connectivity_handler
	```

6. Make the initial AWS Greengrass group configuration. The group creation 
   has been automated to reduce the amount of time required for this procedure.

   The prefix used here is the same as the Amazon S3 bucket prefix used in Lab 1.

	```bash
	cd $HOME/aws-cloud-and-xilinx-workshop/edge/script
	./greengrass-core-init.sh <prefix>
	```
	

7. Perform the initial deployment of AWS Greengrass.

	Run these commands in the Ultra96 terminal window.

	```bash
	cd $HOME/aws-cloud-and-xilinx-workshop/cloud/script
	./deploy-greengrass-group.sh <prefix>
	```
   Afer a few seconds, you should be able to see the group has been deployed 
   successfully.

8. Go to the AWS IoT Console page and click on **Test** on the left-hand side menu. 
9. Click on **Subscribe to a topic** under the **Subscriptions** header.
10. In the **Subscription topic** input box, enter ```hello/world```. 
11. Click the **Subscribe to topic** button.
    You should now see a MQTT response from the Ultra96 platform in the test window response.
    See picture below for expected response.

    ![alt text](images/Greengrass_HelloWorld_Test.PNG "Greengrass Successful Response")


## Outcomes

In this lab we established basic "hello world" connectivity from AWS Greengrass running on Embedded Linux on the Ultra96 platform.

[Next Lab](./Lab4.md)

[Index](./README.md)

Copyright (C) 2018 Amazon.com, Inc. and Xilinx Inc.  All Rights Reserved.
