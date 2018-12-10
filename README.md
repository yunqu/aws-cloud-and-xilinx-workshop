# Workshop:  Integrate the AWS Cloud with Responsive Xilinx Machine Learning at the Edge

This repository outlines a workshop first implemented and shared at the AWS re:Invent conference in November 2018.  In working through the workshop labs you learn how you can integrate Xilinx edge machine learning with massive scale AWS Cloud analytics, machine learning model building, and dashboards. Based on a distributed industrial control scenario, you will learn how to combine AWS Cloud services with AWS Greengrass on Zynq Ultrascale+. After this workshop, you will have a concrete understanding of Machine Learning applicability at the edge and its relationship with the AWS Cloud.
 
The repository remains available as a snapshot of the workshop implementation at re:Invent 2018 such that developers that want a hands-on experience of the AWS and Xilinx technology integration can do so by purchasing their own HW set (outlined in FAQs) and following the instructions below.

# Lab 1: Setup the Environment

In this lab you will download the Xilinx tools required for customizing the runtime images of the embedded platforms and set-up your corresponding cloud environment.

[Go to Lab 1](./Lab1.md)

# Lab 2: Establish AWS Greengrass Connectivity

In this lab we will establish basic connectivity to the AWS cloud for Ultra96 device. In order to do this we need to establish unique device identities that link the device to your AWS IoT account. This is done through a unique certificate and encyrption key. You will need to create these credentials in your AWS account and then manually copy them to each device.

[Go to Lab 2](./Lab2.md)


# Lab 4: Deriving Machine Learning Inference Value

In this lab we will use the video machine learning inference function of the FPGA to trigger an interaction with AWS cloud.  The Ultra96 upon detection of a person send a copy of the video frame capture to AWS cloud which will then be used to add an alert to the system monitor dash board that someone is near the asset and display a picture of that person. 

[Go to Lab 4](./Lab4.md)


# Lab 5: Deploy Edge ML Update

In this lab we will deploy a new machine learning edge inference function to the FPGA of the unit controller running on Ultra96.  This new edge ML inference function will be used to monitor a local camera that is monitoring the remote asset to any persons in the area of the asset. 

[Go to Lab 5](./Lab5.md)

Copyright (C) 2018 Amazon.com, Inc. and Xilinx Inc.  All Rights Reserved.
