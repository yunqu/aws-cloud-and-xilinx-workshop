/*
 * Amazon FreeRTOS MQTT UZed Demo V1.0
 * Copyright (C) 2017 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * http://aws.amazon.com/freertos
 * http://www.FreeRTOS.org
 */


/**
 * @file uzed_iot.c
 * @brief A simple MQTT sensor example for the MicroZed IOT Kit.
 *
 * It creates an MQTT client that periodically publishes sensor readings to
 * MQTT topics at a defined rate.
 *
 * The demo uses one task. The task implemented by
 * prvUZedIotTask() creates the GG MQTT client, subscribes to the
 * broker specified by the clientcredentialMQTT_BROKER_ENDPOINT constant,
 * performs the publish operations periodically forever.
 */

//////////////////// USER PARAMETERS ////////////////////
/* Sampling period, in ms. Two messages per period: pressure and temperature */
#define SAMPLING_PERIOD_MS		5000

/* Timeout used when establishing a connection, which required TLS
* negotiation. */
#define democonfigMQTT_UZED_TLS_NEGOTIATION_TIMEOUT        pdMS_TO_TICKS( 60000 )

/**
 * @brief Dimension of the character array buffers used to hold data (strings in
 * this case) that is published to and received from the MQTT broker (in the cloud).
 */
#define UZedMAX_DATA_LENGTH    256

/**
 * @brief A block time of 0 simply means "don't block".
 */
#define UZedDONT_BLOCK         ( ( TickType_t ) 0 )

/**
 * @brief If set to 1, use GreenGrass instead of raw MQTT
 */
#define UZED_USE_GG 1

/**
 * @brief MQTT client ID.
 *
 * It must be unique per MQTT broker.
 */
#if UZED_USE_GG
#define UZedCLIENT_ID          ( ( const uint8_t * ) "GGUZed" )
#else
#define UZedCLIENT_ID          ( ( const uint8_t * ) "MQTTUZed" )
#endif

//////////////////// END USER PARAMETERS ////////////////////

#if SAMPLING_PERIOD_MS < 100
#error Sampling period must be at least 100 ms
#endif

/*-----------------------------------------------------------*/

/* Standard includes. */
#include "string.h"
#include "stdio.h"
#include <stdarg.h>

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "message_buffer.h"

/* MQTT includes. */
#include "aws_mqtt_agent.h"

/* Credentials includes. */
#include "aws_clientcredential.h"
#include "aws_system_init.h"
#include "aws_pkcs11_config.h"

/* Demo includes. */
#include "aws_demo_config.h"
#include "xil_types.h"
#include "xparameters.h"
#include "xstatus.h"
#include "xiic.h"
#include "xgpiops.h"
#include "xspi_l.h"
#include "xemacps.h"
#include "uzed_iot.h"
#if UZED_USE_GG
#include "aws_ggd_config.h"
#include "aws_ggd_config_defaults.h"
#include "aws_greengrass_discovery.h"
#endif

/*-----------------------------------------------------------*/
// System parameters for the MicroZed IOT kit

#if UZED_USE_GG
#define GG_DISCOVERY_FILE_SIZE    4096
#endif

/**
 * @brief This is the LPS25HB on the Arduino shield board
 */
#define BAROMETER_SLAVE_ADDRESS		0x5D
/**
 * @brief This is the HTS221 on the Arduino shield board
 */
#define HYGROMETER_SLAVE_ADDRESS	0x5F

/**
 * @brief LED pin represents connection state
 */
#define LED_PIN	47

/**
 * @brief Barometer register defines
 */
#define BAROMETER_REG_REF_P_XL			0x15
#define BAROMETER_REG_REF_P_L			0x16
#define BAROMETER_REG_REF_P_H			0x17
#define BAROMETER_REG_WHO_AM_I			0x0F
#define BAROMETER_REG_RES_CONF			0x1A

#define BAROMETER_REG_CTRL_REG1			0x10
#define BAROMETER_BFLD_PD				(0<<7)
#define BAROMETER_ODR_2					(0<<6)
#define BAROMETER_ODR_1					(0<<5)
#define BAROMETER_ODR_0					(0<<4)
#define BAROMETER_ENABLE_LPFP				(0<<3)
#define BAROMETER_LPFP_CFG				(0<<2)
#define BAROMETER_BDU					(0<<1)
#define BAROMETER_SIM					(0<<0)


#define BAROMETER_REG_CTRL_REG2			0x11
#define BAROMETER_BFLD_BOOT				(1<<7)
#define BAROMETER_FIFO_ENABLE				(0<<6)
#define BAROMETER_STOP_ON_FTH				(0<<5)
#define BAROMETER_IF_ADD_INC				(1<<4)
#define BAROMETER_I2C_DIS				(0<<3)
#define BAROMETER_BFLD_SWRESET				(1<<2)
#define BAROMETER_BFLD_ZEROBIT                          (0<<1)
#define BAROMETER_BFLD_ONE_SHOT				(1<<0)

#define BAROMETER_REG_CTRL_REG3			0x12
#define BAROMETER_REG_INTERRUPT_CFG		0x0B
#define BAROMETER_REG_INT_SOURCE		0x25

#define BAROMETER_REG_STATUS_REG		0x27
#define BAROMETER_BFLD_P_DA				(1<<0)
#define BAROMETER_BFLD_T_DA				(1<<1)

#define BAROMETER_REG_PRESS_OUT_XL		0x28
#define BAROMETER_REG_PRESS_OUT_L		0x29
#define BAROMETER_REG_PRESS_OUT_H		0x2A
#define BAROMETER_REG_TEMP_OUT_L		0x2B
#define BAROMETER_REG_TEMP_OUT_H		0x2C
#define BAROMETER_REG_FIFO_CTRL			0x14
#define BAROMETER_REG_FIFO_STATUS		0x26
#define BAROMETER_REG_THS_P_L			0x0C
#define BAROMETER_REG_THS_P_H			0x0D
#define BAROMETER_REG_RPDS_L			0x18
#define BAROMETER_REG_RPDS_H			0x19

/**
 * @brief Hygrometer register defines
 */
#define HYGROMETER_REG_WHO_AM_I			0x0F
#define HYGROMETER_REG_AV_CONF			0x10

#define HYGROMETER_REG_CTRL_REG1		0x20
#define HYGROMETER_BFLD_PD				(1<<7)

#define HYGROMETER_REG_CTRL_REG2		0x21
#define HYGROMETER_BFLD_BOOT			(1<<7)
#define HYGROMETER_BFLD_ONE_SHOT		(1<<0)

#define HYGROMETER_REG_CTRL_REG3		0x22

#define HYGROMETER_REG_STATUS_REG		0x27
#define HYGROMETER_BFLD_H_DA			(1<<1)
#define HYGROMETER_BFLD_T_DA			(1<<0)

#define HYGROMETER_REG_HUMIDITY_OUT_L	0x28
#define HYGROMETER_REG_HUMIDITY_OUT_H	0x29
#define HYGROMETER_REG_TEMP_OUT_L		0x2A
#define HYGROMETER_REG_TEMP_OUT_H		0x2B

#define HYGROMETER_REG_CALIB_0			0x30	// Convenience define for beginning of calibration registers
#define HYGROMETER_REG_H0_rH_x2			0x30
#define HYGROMETER_REG_H1_rH_x2			0x31
#define HYGROMETER_REG_T0_degC_x8		0x32
#define HYGROMETER_REG_T1_degC_x8		0x33
#define HYGROMETER_REG_T1_T0_MSB		0x35
#define HYGROMETER_REG_H0_T0_OUT_LSB	0x36
#define HYGROMETER_REG_H0_T0_OUT_MSB	0x37
#define HYGROMETER_REG_H1_T0_OUT_LSB	0x3A
#define HYGROMETER_REG_H1_T0_OUT_MSB	0x3B
#define HYGROMETER_REG_T0_OUT_LSB		0x3C
#define HYGROMETER_REG_T0_OUT_MSB		0x3D
#define HYGROMETER_REG_T1_OUT_LSB		0x3E
#define HYGROMETER_REG_T1_OUT_MSB		0x3F

/**
 * @brief AXI QSPI Temperature sensor defines
 */
#define PL_SPI_BASEADDR			XPAR_AXI_QUAD_SPI_0_BASEADDR  // Base address for AXI SPI controller

#define PL_SPI_CHANNEL_SEL_0		0xFFFFFFFE					// Select spi channel 0
#define PL_SPI_CHANNEL_SEL_1		0xFFFFFFFD					// Select spi channel 1
#define PL_SPI_CHANNEL_SEL_NONE		0xFFFFFFFF					// Deselect all SPI channels

// Initialization settings for the AXI SPI controller's Control Register when addressing the MAX31855
// 0x186 = b1_1000_0110
//			1	Inhibited to hold off transactions starting
//			1	Manually select the slave
//			0	Do not reset the receive FIFO at this time
//			0	Do not reset the transmit FIFO at this time
//			0	Clock phase of 0
//			0	Clock polarity of low
//			1	Enable master mode
//			1	Enable the SPI Controller
//			0	Do not put in loopback mode

#define MAX31855_CLOCK_PHASE_CPHA		0
#define MAX31855_CLOCK_POLARITY_CPOL	0

#define MAX31855_CR_INIT_MODE		XSP_CR_TRANS_INHIBIT_MASK | XSP_CR_MANUAL_SS_MASK   | \
									XSP_CR_MASTER_MODE_MASK   | XSP_CR_ENABLE_MASK
#define MAX31855_CR_UNINHIBIT_MODE	                            XSP_CR_MANUAL_SS_MASK   | \
									XSP_CR_MASTER_MODE_MASK   | XSP_CR_ENABLE_MASK
#define AXI_SPI_RESET_VALUE			0x0A  //!< Reset value for the AXI SPI Controller

/**
 * @brief Utility macro to uniformly process errors
 */
#define MAY_DIE(code)	\
	{ \
	    code; \
		if(pSystem->rc != XST_SUCCESS) { \
            pSystem->bError = 1; \
			configPRINTF( (pSystem->pcErr, pSystem->rc ) ); \
			StopHere(); \
			goto L_DIE; \
		} \
	}

static inline BaseType_t MS_TO_TICKS(BaseType_t xMs)
{
	TickType_t xTicks = pdMS_TO_TICKS( xMs );

	if(xTicks < 1) {
		xTicks = 1;
	}
	return xTicks;
}

/*-----------------------------------------------------------*/

/**
 * @brief System handle contents
 */
#define SYSTEM_SENSOR_TOPIC_LENGTH    64
#define SYSTEM_SHADOW_TOPIC_LENGTH    128
typedef struct System {
	XIic 	iic;
	XGpioPs gpio;

	MQTTAgentHandle_t xMQTTHandle;
#if UZED_USE_GG
    GGD_HostAddressData_t xHostAddressData;
    char pcJSONFile[ GG_DISCOVERY_FILE_SIZE ];
#endif

	u8 pbHygrometerCalibration[16];

	int rc;
    const char* pcErr;
    uint8_t bError;
    uint8_t bLastReportedError;

    // Sensor start ok
    uint8_t bBarometerOk;
    uint8_t bHygrometerOk;
    uint8_t bThermocoupleOk;

    // Sensor values
    float fBarometerPressure;
    float fBarometerTemperature;
    float fHygrometerHumidity;
    float fHygrometerTemperature;
    float fThermocoupleTemperature;
    float fThermocoupleBoardTemperature;

    uint16_t usSensorTopicLength;
    uint8_t pbSensorTopic[SYSTEM_SENSOR_TOPIC_LENGTH + 1];

    uint16_t usShadowTopicLength;
    uint8_t pbShadowTopic[SYSTEM_SHADOW_TOPIC_LENGTH + 1];
} System;
System g_tSystem;

/*-----------------------------------------------------------*/
/**
 * @brief Convenience function for breakpoints
 */
static void StopHere(void);

/*-----------------------------------------------------------*/

/**
 * @brief Publishes specified messag
 *
 * @param[in] pSystem	            System info
 * @param[in] pPublishParameters	Publication Parameters
 */
static void prvPublish(System* pSystem, MQTTAgentPublishParams_t* pPublishParameters);

/**
 * @brief Publishes shadow from system handle
 *
 * @param[in] pSystem	System info
 */
static void prvPublishShadow(System* pSystem);

/**
 * @brief Publishes sensors from system handle
 *
 * @param[in] pSystem	System info
 */
static void prvPublishSensors(System* pSystem);

/**
 * @brief Creates an MQTT client and then connects to the MQTT broker.
 *
 * The MQTT broker end point is set by clientcredentialMQTT_BROKER_ENDPOINT.
 *
 * @return Exit task if failure
 */
static void prvCreateClientAndConnectToBroker( System* pSystem );

/*-----------------------------------------------------------*/

/**
 * @brief Starts complete system
 *
 * @return Exit task if failure
 */
static void StartSystem(System* pSystem);

/**
 * @brief Stops complete system
 *
 * @return Exit task
 */
static void StopSystem(System* pSystem);

/**
 * @brief Implements the task that connects to and then publishes messages to the
 * MQTT broker.
 *
 * Messages are published at 2Hz for a minute.
 *
 * @param[in] pvParameters Parameters passed while creating the task. Unused in our
 * case.
 */
static void prvUZedIotTask( void * pvParameters );

/*-----------------------------------------------------------*/

/**
 * @brief Blink system LED
 *
 * @param[in] pSystem	System handle
 * @param[in] xCount	Number of times to blink LED
 * @param[in] xFinalOn	Should the LED be left on or off at end
 */
static void BlinkLed(System* pSystem,BaseType_t xCount, BaseType_t xFinalOn);

/*-----------------------------------------------------------*/

/**
 * @brief Read multiple IIC registers
 *
 * @param[in] pSystem			System handle
 * @param[in] bSlaveAddress		Slave address on bus
 * @param[in] xCount			Number of registers to read
 * @param[in] bFirstSlaveReg	First register number on device
 * @param[in] pbBuf				Byte buffer to deposit data read from device
 */
static int ReadIicRegs(System* pSystem,u8 bSlaveAddress,BaseType_t xCount,u8 bFirstSlaveReg,u8* pbBuf);

/**
 * @brief Read single IIC register
 *
 * @param[in] pSystem		System handle
 * @param[in] bSlaveAddress	Slave address on bus
 * @param[in] bSlaveReg		Register number on slave device
 * @param[in] pbBuf			Byte buffer to deposit data read from device
 */
static int ReadIicReg(System* pSystem,u8 bSlaveAddress,u8 bSlaveReg,u8* pbBuf);

/**
 * @brief Write multiple IIC registers
 *
 * @param[in] pSystem		System handle
 * @param[in] bSlaveAddress	Slave address on bus
 * @param[in] xCount		Number of registers to write
 * @param[in] pbBuf			Byte buffer with data to write - >= 2 bytes - first byte is always register number on slave device
 */
static int WriteIicRegs(System* pSystem,u8 bSlaveAddress,BaseType_t xCount,u8* pbBuf);

/**
 * @brief Write single IIC register
 *
 * @param[in] pSystem	System handle
 * @param[in] bSlaveAddress	Slave address on bus
 * @param[in] bSlaveReg	Register number on slave device
 * @param[in] pbBuf		Byte buffer with data to write - 2 bytes - first byte is always register number on slave device
 */
static int WriteIicReg(System* pSystem,u8 bSlaveAddress,u8 bSlaveReg,u8 bVal);

/*-----------------------------------------------------------*/

/**
 * @brief Start Barometer
 *
 * @param[in] pSystem	System handle
 */
static void StartBarometer(System* pSystem);

/**
 * @brief Stop Barometer
 *
 * @param[in] pSystem	System handle
 */
static void StopBarometer(System* pSystem);


/**
 * @brief Sample Barometer and publish values
 *
 * @param[in] pSystem			System handle
 */
static void SampleBarometer(System* pSystem);

/*-----------------------------------------------------------*/

/**
 * @brief Start Hygrometer
 *
 * @param[in] pSystem	System handle
 */
static void StartHygrometer(System* pSystem);

/**
 * @brief Stop Hygrometer
 *
 * @param[in] pSystem	System handle
 */
static void StopHygrometer(System* pSystem);


/**
 * @brief Sample Hygrometer and publish values
 *
 * @param[in] pSystem			System handle
 */
static void SampleHygrometer(System* pSystem);

/*-----------------------------------------------------------*/

/**
 * @brief Start PL Temperature Sensor
 *
 * @param[in] pSystem	System handle
 */
static void StartPLTempSensor(System* pSystem);

/**
 * @brief Stop PL Temperature Sensor
 *
 * @param[in] pSystem	System handle
 */
static void StopPLTempSensor(System* pSystem);

/**
 * @brief PL Temperature Sensor: utility function to do SPI transaction
 *
 * @param[in] 	pSystem			System handle
 * @param[in] 	qBaseAddress	AXI SPI Controller Base Address
 * @param[in] 	xSPI_Channel	SPI Channel to use
 * @param[in] 	xByteCount		Number of bytes to transfer
 * @param[in] 	pqTxBuffer		Data to send
 * @param[out] 	pqRxBuffer		Data to receive
 */
static void XSpi_LowLevelExecute(System* pSystem, u32 qBaseAddress, BaseType_t xSPI_Channel, BaseType_t xByteCount, const u32* pqTxBuffer, u32* pqRxBuffer);

/**
 * @brief Sample Barometer and publish values
 *
 * @param[in] pSystem			System handle
 */
static void SamplePLTempSensor(System* pSystem);


/*--------------------------------------------------------------------------------*/
static void StopHere(void)
{
	;
}

/*--------------------------------------------------------------------------------*/

static void BlinkLed(System* pSystem,BaseType_t xCount, BaseType_t xFinalOn)
{
	BaseType_t x;
	const TickType_t xHalfSecond = MS_TO_TICKS( 500 );

	if(!pSystem->gpio.IsReady) {
		return;
	}
	for(x = 0; x < xCount; x++) {
		XGpioPs_WritePin(&pSystem->gpio, LED_PIN, 1);
		vTaskDelay(xHalfSecond);

		XGpioPs_WritePin(&pSystem->gpio, LED_PIN, 0);
		vTaskDelay(xHalfSecond);
	}
	if(xFinalOn) {
		XGpioPs_WritePin(&pSystem->gpio, LED_PIN, 1);
	}
}

/*-----------------------------------------------------------*/

static void prvPublish(System* pSystem, MQTTAgentPublishParams_t* pPublishParameters)
{
    MQTTAgentReturnCode_t xReturned;

    if(!pPublishParameters->usTopicLength) {
        pPublishParameters->usTopicLength = strlen((const char*)pPublishParameters->pucTopic);
        if(!pPublishParameters->usTopicLength) {
            return;
        }
    }
    if(!pPublishParameters->ulDataLength) {
        pPublishParameters->ulDataLength = strlen((const char*)pPublishParameters->pvData);
        if(!pPublishParameters->ulDataLength) {
            return;
        }
    }

    /* Publish the message. */
    xReturned = MQTT_AGENT_Publish( pSystem->xMQTTHandle, pPublishParameters, democonfigMQTT_TIMEOUT );
    switch(xReturned) {
    case eMQTTAgentSuccess:
    	configPRINTF( ( "Success: Published '%s'\r\n", (const char*)pPublishParameters->pucTopic, (const char*)pPublishParameters->pvData ) );
    	break;

    case eMQTTAgentFailure:
    	BlinkLed(pSystem, 1, pdFALSE);
        configPRINTF( ( "ERROR: Failed to publish '%s'\r\n", (const char*)pPublishParameters->pucTopic, (const char*)pPublishParameters->pvData ) );
        break;

    case eMQTTAgentTimeout:
    	BlinkLed(pSystem, 1, pdFALSE);
        configPRINTF( ( "ERROR: Timed out publishing '%s'\r\n", (const char*)pPublishParameters->pucTopic, (const char*)pPublishParameters->pvData ) );
        break;

    default:	//FallThrough
    case eMQTTAgentAPICalledFromCallback:
    	BlinkLed(pSystem, 1, pdFALSE);
        configPRINTF( ( "ERROR: Unexpected callback publishing '%s'\r\n", (const char*)pPublishParameters->pucTopic, (const char*)pPublishParameters->pvData ) );
    	configASSERT(pdTRUE);
    	break;	// Not reached
    }
}

static void prvPublishShadow(System* pSystem)
{
    MQTTAgentPublishParams_t xPublishParameters;
    char pcDataBuffer[ UZedMAX_DATA_LENGTH ];
    int iDataLength;

    if(pSystem->xMQTTHandle == NULL) {
    	return;
    }

    /*
     * Compose the message
     */
    iDataLength = snprintf(pcDataBuffer, UZedMAX_DATA_LENGTH, "{\"state\": { \"desired\": {\"led\":%u}}}", pSystem->bError);		
    pcDataBuffer[UZedMAX_DATA_LENGTH - 1] = 0;	// safety
    if(iDataLength >= UZedMAX_DATA_LENGTH) {
    	iDataLength = UZedMAX_DATA_LENGTH - 1;
    } else if(iDataLength < 0) {
    	iDataLength = 3;
    	pcDataBuffer[0] = '?';
    	pcDataBuffer[1] = '?';
    	pcDataBuffer[2] = '?';
    	pcDataBuffer[3] = '\0';
    }

    /* Setup the publish parameters. */
    memset( &( xPublishParameters ), 0, sizeof( xPublishParameters ) );
    xPublishParameters.pucTopic = (const uint8_t*)pSystem->pbShadowTopic;
    xPublishParameters.usTopicLength = pSystem->usShadowTopicLength;
    xPublishParameters.xQoS = eMQTTQoS0;
    xPublishParameters.pvData = (void*)pcDataBuffer;
    xPublishParameters.ulDataLength = ( uint32_t ) iDataLength;

    prvPublish(pSystem,&xPublishParameters);
}

static void prvPublishSensors(System* pSystem)
{
    MQTTAgentPublishParams_t xPublishParameters;
    char pcDataBuffer[ UZedMAX_DATA_LENGTH ];
    int iDataLength;

    if(pSystem->xMQTTHandle == NULL) {
    	return;
    }

    /*
     * Compose the message
     */
    iDataLength = snprintf(pcDataBuffer, UZedMAX_DATA_LENGTH,
        "{\n "
        "\"%s\": %.2f,\n"
        "\"%s\": %.2f,\n"
        "\"%s\": %.2f,\n"
        "\"%s\": %.2f,\n"
        "\"%s\": %.2f,\n"
        "\"%s\": %.2f\n"
        "}"
        ,
		"Pressure",             pSystem->fBarometerPressure,
		"Pressure_Sensor_Temp", pSystem->fBarometerTemperature,
		"Thermocouple_Temp",    pSystem->fThermocoupleTemperature,
		"Board_Temp_1",         pSystem->fThermocoupleBoardTemperature,
		"Relative_Humidity",    pSystem->fHygrometerHumidity,
		"Humidity_Sensor_Temp", pSystem->fHygrometerTemperature
        );
    pcDataBuffer[UZedMAX_DATA_LENGTH - 1] = 0;	// safety
    if((iDataLength < 0) || (iDataLength >= UZedMAX_DATA_LENGTH)) {
        pSystem->bError = 1;
        return;
    }

    /* Setup the publish parameters. */
    memset( &( xPublishParameters ), 0, sizeof( xPublishParameters ) );
    xPublishParameters.pucTopic = pSystem->pbSensorTopic;
    xPublishParameters.usTopicLength = pSystem->usSensorTopicLength;
    xPublishParameters.xQoS = eMQTTQoS0;
    xPublishParameters.pvData = (void*)pcDataBuffer;
    xPublishParameters.ulDataLength = ( uint32_t ) iDataLength;

    prvPublish(pSystem,&xPublishParameters);
}

/*--------------------------------------------------------------------------------*/

static void prvCreateClientAndConnectToBroker( System* pSystem )
{
    MQTTAgentConnectParams_t xConnectParameters;
    BaseType_t xStatus;

    configPRINTF( ( "Broker ID: '%s'\r\n", clientcredentialMQTT_BROKER_ENDPOINT ) );
    /* The MQTT client object must be created before it can be used.  The
     * maximum number of MQTT client objects that can exist simultaneously
     * is set by mqttconfigMAX_BROKERS. */
    if( eMQTTAgentSuccess == MQTT_AGENT_Create( &pSystem->xMQTTHandle ) ) {
#if UZED_USE_GG
        configPRINTF( ( "Attempting automated selection of Greengrass device\r\n" ) );
        memset( &pSystem->xHostAddressData, 0, sizeof( GGD_HostAddressData_t ) );
        xStatus = GGD_GetGGCIPandCertificate(pSystem->pcJSONFile,GG_DISCOVERY_FILE_SIZE,&pSystem->xHostAddressData);

        if(pdPASS == xStatus) {
            configPRINTF( ("Success: GGC is %s\r\n", pSystem->xHostAddressData.pcHostAddress ) );
        	xConnectParameters.pcURL = pSystem->xHostAddressData.pcHostAddress;
            xConnectParameters.xFlags = mqttagentREQUIRE_TLS | mqttagentURL_IS_IP_ADDRESS;
            xConnectParameters.xURLIsIPAddress = pdTRUE; /* Deprecated. */
            xConnectParameters.usPort = clientcredentialMQTT_BROKER_PORT;
            xConnectParameters.pucClientId = (const uint8_t*)clientcredentialIOT_THING_NAME;
            xConnectParameters.usClientIdLength = (uint16_t)strlen(clientcredentialIOT_THING_NAME);
            xConnectParameters.xSecuredConnection = pdTRUE; /* Deprecated. */
            xConnectParameters.pvUserData = NULL;
            xConnectParameters.pxCallback = NULL;
            xConnectParameters.pcCertificate = pSystem->xHostAddressData.pcCertificate;
            xConnectParameters.ulCertificateSize = pSystem->xHostAddressData.ulCertificateSize;
        } else {
            configPRINTF( ("Failed: GGD_GetGGCIPandCertificate()\n" ) );
            xConnectParameters.pcURL = 0;
            pSystem->rc = XST_FAILURE;
            pSystem->pcErr = "Auto-connect: Failed to retrieve Greengrass address and certificate\r\n";
            pSystem->xMQTTHandle = NULL;
        }
#else
        /* Connect directly to the broker. */
        xConnectParameters.pcURL = clientcredentialMQTT_BROKER_ENDPOINT; /* The URL of the MQTT broker to connect to. */
        xConnectParameters.xFlags = democonfigMQTT_AGENT_CONNECT_FLAGS;   /* Connection flags. */
        xConnectParameters.xURLIsIPAddress = pdFALSE;                              /* Deprecated. */
        xConnectParameters.usPort = clientcredentialMQTT_BROKER_PORT;     /* Port number on which the MQTT broker is listening. Can be overridden by ALPN connection flag. */
        xConnectParameters.pucClientId = UZedCLIENT_ID;                        /* Client Identifier of the MQTT client. It should be unique per broker. */
        xConnectParameters.usClientIdLength = (uint16_t)strlen((const char*)UZedCLIENT_ID);
        xConnectParameters.xSecuredConnection = pdFALSE;                              /* Deprecated. */
        xConnectParameters.pvUserData = NULL;                                 /* User data supplied to the callback. Can be NULL. */
        xConnectParameters.pxCallback = NULL;                                 /* Callback used to report various events. Can be NULL. */
        xConnectParameters.pcCertificate = NULL;                                 /* Certificate used for secure connection. Can be NULL. */
        xConnectParameters.ulCertificateSize = 0;                                     /* Size of certificate used for secure connection. */
#endif

        if(xConnectParameters.pcURL) {
            configPRINTF( ( "INFO: %s: Attempting to connect to '%s'\r\n",
            		UZED_USE_GG? "GreenGrass":"MQTT",
            		xConnectParameters.pcURL ) );
            if(eMQTTAgentSuccess == MQTT_AGENT_Connect(
                    pSystem->xMQTTHandle,
                    &xConnectParameters,
                    democonfigMQTT_UZED_TLS_NEGOTIATION_TIMEOUT
                    )
                ) {
                configPRINTF( ( "SUCCESS: connected\r\n" ) );
                pSystem->rc = XST_SUCCESS;
            } else {
                /* Could not connect, so delete the MQTT client. */
                ( void ) MQTT_AGENT_Delete( pSystem->xMQTTHandle );
                pSystem->rc = XST_FAILURE;
                pSystem->pcErr = "ERROR: Could not connect\r\n";
                pSystem->xMQTTHandle = NULL;
                configPRINTF( ( "%s\r\n", pSystem->pcErr ) );
            }
        }
    } else {
        pSystem->rc = XST_FAILURE;
    	pSystem->pcErr = "ERROR: Could not create MQTT Agent\r\n";
    	pSystem->xMQTTHandle = NULL;
        configPRINTF( ( "%s\r\n", pSystem->pcErr ) );
    }
}

/*--------------------------------------------------------------------------------*/

static int ReadIicRegs(System* pSystem,u8 bSlaveAddress,BaseType_t xCount,u8 bFirstSlaveReg,u8* pbBuf)
{
	BaseType_t xReceived;

	if(xCount > 1) {
		bFirstSlaveReg |= 0x80;
	}

	MAY_DIE({
		if(1 != XIic_Send(pSystem->iic.BaseAddress,bSlaveAddress,&bFirstSlaveReg,1,XIIC_REPEATED_START)) {
			pSystem->rc = 1;
			pSystem->pcErr = "ReadIicRegs::XIic_Send(Addr) -> 0x%08x\r\n";
		}
	});
	MAY_DIE({
		xReceived = XIic_Recv(pSystem->iic.BaseAddress,bSlaveAddress,pbBuf,xCount,XIIC_STOP);
		if(xReceived != xCount) {
			pSystem->rc = ((xReceived & 0xf) << 4) | (xCount & 0xf);
			pSystem->pcErr = "ReadIicRegs::XIic_Recv(Data) -> 0x%08x\r\n";
		}
	});

L_DIE:
	return pSystem->rc;
}

static int ReadIicReg(System* pSystem, u8 bSlaveAddress, u8 bFirstSlaveReg, u8* pbBuf)
{
	return ReadIicRegs(pSystem, bSlaveAddress, 1, bFirstSlaveReg, pbBuf);
}

static int WriteIicRegs(System* pSystem, u8 bSlaveAddress, BaseType_t xCount, u8* pbBuf)
{
	BaseType_t xSent;

	if(xCount > 2) {
		pbBuf[0] |= 0x80;
	}

	MAY_DIE({
		xSent = XIic_Send(pSystem->iic.BaseAddress,bSlaveAddress,pbBuf,xCount,XIIC_STOP);
		if(xCount != xSent) {
			pSystem->rc = ((xSent & 0xf) << 4) | (xCount & 0xf);
			pSystem->pcErr = "WriteIicRegs::XIic_Send(Buf) -> 0x%08x\r\n";
		}
	});

L_DIE:
	return pSystem->rc;
}

static int WriteIicReg(System* pSystem,u8 bSlaveAddress, u8 bFirstSlaveReg, u8 bVal)
{
	u8 pbBuf[2];

	pbBuf[0] = bFirstSlaveReg;
	pbBuf[1] = bVal;
	return WriteIicRegs(pSystem, bSlaveAddress, 2, pbBuf);
}

/*--------------------------------------------------------------------------------*/

static void StartBarometer(System* pSystem)
{
	u8 b;
	int iTimeout;
	TickType_t xOneMs = MS_TO_TICKS( 1 );

    pSystem->bBarometerOk = pdFALSE;
    pSystem->fBarometerPressure = 0;
    pSystem->fBarometerTemperature = 0;

	// Verify it is the right chip
	MAY_DIE({
		ReadIicReg(pSystem,BAROMETER_SLAVE_ADDRESS,BAROMETER_REG_WHO_AM_I,&b);
		pSystem->pcErr = "ReadIicReg(WHO_AM_I) -> 0x%08x\r\n";
	});

	MAY_DIE({
		if(0xB1 != b) {
			pSystem->rc = b?b:1;
			pSystem->pcErr = "BAROMETER_WHO_AM_I = 0x%08x != 0xB1\r\n";
		}
	});

	// Reset chip: first swreset, then boot
	MAY_DIE({
		WriteIicReg(pSystem,BAROMETER_SLAVE_ADDRESS,BAROMETER_REG_CTRL_REG2,BAROMETER_BFLD_SWRESET);
		pSystem->pcErr = "WriteIicReg(BAROMETER_REG_CTRL_REG2::BFLD_SWRESET) -> 0x%08x\r\n";
	});
	for(iTimeout = 100; iTimeout-- > 0; ) {
		MAY_DIE({
			ReadIicReg(pSystem,BAROMETER_SLAVE_ADDRESS,BAROMETER_REG_CTRL_REG2,&b);
			pSystem->pcErr = "ReadIicReg(BAROMETER_REG_CTRL_REG2) -> 0x%08x\r\n";
		});
		if(0 == (b & BAROMETER_BFLD_SWRESET)) {
			break;
		}
		vTaskDelay(xOneMs);
	}
	if(iTimeout <= 0) {
		MAY_DIE({
			pSystem->rc = XST_FAILURE;
			pSystem->pcErr = "Barometer swreset timeout\r\n";
		});
	}

	MAY_DIE({
		WriteIicReg(pSystem,BAROMETER_SLAVE_ADDRESS,BAROMETER_REG_CTRL_REG2,BAROMETER_BFLD_BOOT);
		pSystem->pcErr = "WriteIicReg(BAROMETER_REG_CTRL_REG2::BAROMETER_BFLD_BOOT -> 0x%08x\r\n";
	});
	for(iTimeout = 100; iTimeout-- > 0; ) {
		MAY_DIE({
			ReadIicReg(pSystem,BAROMETER_SLAVE_ADDRESS,BAROMETER_REG_CTRL_REG2,&b);
			pSystem->pcErr = "ReadIicReg(BAROMETER_REG_CTRL_REG2) -> 0x%08x\r\n";
		});
		if(0 == (b & BAROMETER_BFLD_BOOT)) {
			break;
		}
		vTaskDelay(xOneMs);
	}
	if(iTimeout <= 0) {
		MAY_DIE({
			pSystem->rc = XST_FAILURE;
			pSystem->pcErr = "Barometer boot timeout\r\n";
		});
	}

	MAY_DIE({
		WriteIicReg(pSystem,BAROMETER_SLAVE_ADDRESS,BAROMETER_REG_CTRL_REG2,BAROMETER_BFLD_ZEROBIT);
		pSystem->pcErr = "WriteIicReg(BAROMETER_REG_CTRL_REG2::BAROMETER_BFLD_ZEROBIT -> 0x%08x\r\n";
	});

	MAY_DIE({
		WriteIicReg(pSystem,BAROMETER_SLAVE_ADDRESS,BAROMETER_REG_CTRL_REG2,BAROMETER_FIFO_ENABLE);
		pSystem->pcErr = "WriteIicReg(BAROMETER_REG_CTRL_REG2::BAROMETER_FIFO_ENABLE -> 0x%08x\r\n";
	});

	MAY_DIE({
		WriteIicReg(pSystem,BAROMETER_SLAVE_ADDRESS,BAROMETER_REG_CTRL_REG2,BAROMETER_STOP_ON_FTH);
		pSystem->pcErr = "WriteIicReg(BAROMETER_REG_CTRL_REG2::BAROMETER_STOP_ON_FTH -> 0x%08x\r\n";
	});
	
	MAY_DIE({
		WriteIicReg(pSystem,BAROMETER_SLAVE_ADDRESS,BAROMETER_REG_CTRL_REG2,BAROMETER_IF_ADD_INC);
		pSystem->pcErr = "WriteIicReg(BAROMETER_REG_CTRL_REG2::BAROMETER_IF_ADD_INC -> 0x%08x\r\n";
	});

	MAY_DIE({
		WriteIicReg(pSystem,BAROMETER_SLAVE_ADDRESS,BAROMETER_REG_CTRL_REG2, BAROMETER_I2C_DIS);
		pSystem->pcErr = "WriteIicReg(BAROMETER_REG_CTRL_REG1::BAROMETER_I2C_DIS) -> 0x%08x\r\n";
	});
	MAY_DIE({
		WriteIicReg(pSystem,BAROMETER_SLAVE_ADDRESS,BAROMETER_REG_CTRL_REG1,BAROMETER_ODR_2);
		pSystem->pcErr = "WriteIicReg(BAROMETER_REG_CTRL_REG1::BAROMETER_ODR_2) -> 0x%08x\r\n";
	});
	MAY_DIE({
		WriteIicReg(pSystem,BAROMETER_SLAVE_ADDRESS,BAROMETER_REG_CTRL_REG1,BAROMETER_ODR_1);
		pSystem->pcErr = "WriteIicReg(BAROMETER_REG_CTRL_REG1::BAROMETER_ODR_1) -> 0x%08x\r\n";
	});
	MAY_DIE({
		WriteIicReg(pSystem,BAROMETER_SLAVE_ADDRESS,BAROMETER_REG_CTRL_REG1,BAROMETER_ODR_0);
		pSystem->pcErr = "WriteIicReg(BAROMETER_REG_CTRL_REG1::BAROMETER_ODR_0) -> 0x%08x\r\n";
	});
	MAY_DIE({
		WriteIicReg(pSystem,BAROMETER_SLAVE_ADDRESS,BAROMETER_REG_CTRL_REG1,BAROMETER_ENABLE_LPFP);
		pSystem->pcErr = "WriteIicReg(BAROMETER_REG_CTRL_REG1::BAROMETER_ENABLE_LPFP) -> 0x%08x\r\n";
	});
	MAY_DIE({
		WriteIicReg(pSystem,BAROMETER_SLAVE_ADDRESS,BAROMETER_REG_CTRL_REG1,BAROMETER_LPFP_CFG);
		pSystem->pcErr = "WriteIicReg(BAROMETER_REG_CTRL_REG1::BAROMETER_LPFP_CFG) -> 0x%08x\r\n";
	});
	MAY_DIE({
		WriteIicReg(pSystem,BAROMETER_SLAVE_ADDRESS,BAROMETER_REG_CTRL_REG1,BAROMETER_BDU);
		pSystem->pcErr = "WriteIicReg(BAROMETER_REG_CTRL_REG1::BAROMETER_BDU) -> 0x%08x\r\n";
	});
	MAY_DIE({
		WriteIicReg(pSystem,BAROMETER_SLAVE_ADDRESS,BAROMETER_REG_CTRL_REG1,BAROMETER_SIM);
		pSystem->pcErr = "WriteIicReg(BAROMETER_REG_CTRL_REG1::BAROMETER_SIM) -> 0x%08x\r\n";
	});
	MAY_DIE({
		WriteIicReg(pSystem,BAROMETER_SLAVE_ADDRESS,BAROMETER_REG_CTRL_REG1,BAROMETER_BFLD_PD);
		pSystem->pcErr = "WriteIicReg(BAROMETER_REG_CTRL_REG1::BAROMETER_BFLD_PD) -> 0x%08x\r\n";
	});
	vTaskDelay(xOneMs);

    pSystem->bBarometerOk = pdTRUE;
	configPRINTF( ( "Barometer started ok\r\n" ) );
    return;

L_DIE:
	configPRINTF( ( "ERROR: Barometer started not ok\r\n" ) );
	return;
}

static void StopBarometer(System* pSystem)
{
    pSystem->bBarometerOk = pdFALSE;
}

static void SampleBarometer(System* pSystem)
{
	BaseType_t xTimeout;
	u8 b;
	u8 pbBuf[6];
	s32 sqTmp;
	float f;
 	u8 count = 0;
	TickType_t xOneMs = MS_TO_TICKS( 1 );

    if(!pSystem->bBarometerOk) {
        return;
    }
    pSystem->rc = XST_SUCCESS;

	/*
	 * NOTE: The one shot auto clears but it seems to take 36ms
	 * Our sampling period is >= 100ms so the one shot will auto clear by the next sample time
	 */
	MAY_DIE({
		WriteIicReg(pSystem,BAROMETER_SLAVE_ADDRESS,BAROMETER_REG_CTRL_REG2,BAROMETER_BFLD_ONE_SHOT);
		pSystem->pcErr = "WriteIicReg(BAROMETER_REG_CTRL_REG2::BAROMETER_BFLD_ONE_SHOT) -> 0x%08x\r\n";
	});
	for(xTimeout = 50; xTimeout-- > 0; ) {
		MAY_DIE({
			ReadIicReg(pSystem,BAROMETER_SLAVE_ADDRESS,BAROMETER_REG_CTRL_REG2,&b);
			pSystem->pcErr = "ReadIicReg(BAROMETER_REG_CTRL_REG2) -> 0x%08x\r\n";
		});
		if(0 == (b & BAROMETER_BFLD_ONE_SHOT)) {
			break;
		}
		vTaskDelay(xOneMs);
	}
	MAY_DIE({
		if(xTimeout <= 0) {
			pSystem->rc = XST_FAILURE;
			pSystem->pcErr = "Timed out waiting for BAROMETER_BFLD_ONE_SHOT\r\n";
		}
	});

	for(xTimeout = 50; xTimeout-- > 0; ) {
		MAY_DIE({
			ReadIicRegs(pSystem,BAROMETER_SLAVE_ADDRESS,6,BAROMETER_REG_STATUS_REG,pbBuf);
			pSystem->pcErr = "ReadIicRegs(6@BAROMETER_REG_STATUS_REG) -> 0x%08x\r\n";
		});
		if((BAROMETER_BFLD_P_DA | BAROMETER_BFLD_T_DA) == (pbBuf[0] & (BAROMETER_BFLD_P_DA | BAROMETER_BFLD_T_DA))) {
			break;
		}
		vTaskDelay(xOneMs);
	}
	MAY_DIE({
		if(xTimeout <= 0) {
			pSystem->rc = XST_FAILURE;
			pSystem->pcErr = "Timed out waiting for P_DA and T_DA\r\n";
		}
	});

	for(count=1; count<6;count++)
		pbBuf[count] = 0;

        MAY_DIE({
            ReadIicRegs(pSystem,BAROMETER_SLAVE_ADDRESS,1,BAROMETER_REG_PRESS_OUT_XL,&pbBuf[1]);
            pSystem->pcErr = "ReadIicRegs(BAROMETER_REG_PRESS_OUT_XL) -> 0x%08x\r\n";
        });

        MAY_DIE({
            ReadIicRegs(pSystem,BAROMETER_SLAVE_ADDRESS,1,BAROMETER_REG_PRESS_OUT_L,&pbBuf[2]);
            pSystem->pcErr = "ReadIicRegs(BAROMETER_REG_PRESS_OUT_L) -> 0x%08x\r\n";
        });

        MAY_DIE({
            ReadIicRegs(pSystem,BAROMETER_SLAVE_ADDRESS,1,BAROMETER_REG_PRESS_OUT_H,&pbBuf[3]);
            pSystem->pcErr = "ReadIicRegs(BAROMETER_REG_PRESS_OUT_H) -> 0x%08x\r\n";
        });

        MAY_DIE({
            ReadIicRegs(pSystem,BAROMETER_SLAVE_ADDRESS,1,BAROMETER_REG_TEMP_OUT_L,&pbBuf[4]);
            pSystem->pcErr = "ReadIicRegs(BAROMETER_REG_TEMP_OUT_L) -> 0x%08x\r\n";
        });

        MAY_DIE({
            ReadIicRegs(pSystem,BAROMETER_SLAVE_ADDRESS,1,BAROMETER_REG_TEMP_OUT_H,&pbBuf[5]);
            pSystem->pcErr = "ReadIicRegs(BAROMETER_REG_TEMP_OUT_H) -> 0x%08x\r\n";
        });
		

	// See ST TN1228
	sqTmp = 0
			| ((u32)pbBuf[1] << 0)	// xl
			| ((u32)pbBuf[2] << 8)	// l
			| ((u32)pbBuf[3] << 16)	// h
			;
	if(sqTmp & 0x00800000) {
		sqTmp |= 0xFF800000;
	}
	f = (float)sqTmp / 4096.0F;
    pSystem->fBarometerPressure = f;

	sqTmp = 0
			| ((u32)pbBuf[4] << 0)	// l
			| ((u32)pbBuf[5] << 8)	// h
			;
	if(sqTmp & 0x00008000) {
		sqTmp |= 0xFFFF8000;
	}
	f = (float)sqTmp/100.0;

    pSystem->fBarometerTemperature = f;

L_DIE:
	return;
}

/*--------------------------------------------------------------------------------*/

static void StartHygrometer(System* pSystem)
{
	u8 b;
	int iTimeout;
	TickType_t xOneMs = MS_TO_TICKS( 1 );

    pSystem->bHygrometerOk = pdFALSE;
    pSystem->fHygrometerHumidity = 0;
    pSystem->fHygrometerTemperature = 0;

	// Verify it is the right chip
	MAY_DIE({
		ReadIicReg(pSystem,HYGROMETER_SLAVE_ADDRESS,HYGROMETER_REG_WHO_AM_I,&b);
		pSystem->pcErr = "ReadIicReg(HYGROMETER_WHO_AM_I) -> 0x%08x\r\n";
	});
	MAY_DIE({
		if(0xBC != b) {
			pSystem->rc = b?b:1;
			pSystem->pcErr = "HYGROMETER_WHO_AM_I = 0x%08x != BC\r\n";
		}
	});

	// Reset chip: boot
	MAY_DIE({
		WriteIicReg(pSystem,HYGROMETER_SLAVE_ADDRESS,HYGROMETER_REG_CTRL_REG2,HYGROMETER_BFLD_BOOT);
		pSystem->pcErr = "WriteIicReg(HYGROMETER_REG_CTRL_REG2::HYGROMETER_BFLD_BOOT -> 0x%08x\r\n";
	});
	for(iTimeout = 1000; iTimeout-- > 0; ) {
		MAY_DIE({
			ReadIicReg(pSystem,HYGROMETER_SLAVE_ADDRESS,HYGROMETER_REG_CTRL_REG2,&b);
			pSystem->pcErr = "ReadIicReg(BAROMETER_REG_CTRL_REG2) -> 0x%08x\r\n";
		});
		if(0 == (b & HYGROMETER_BFLD_BOOT)) {
			break;
		}
		vTaskDelay(xOneMs);
	}
	if(iTimeout <= 0) {
		MAY_DIE({
			pSystem->rc = XST_FAILURE;
			pSystem->pcErr = "Hygrometer boot timeout\r\n";
		});
	}

	/*
	 * Read and store calibration values
	 */
	MAY_DIE({
		ReadIicRegs(pSystem,HYGROMETER_SLAVE_ADDRESS,16,HYGROMETER_REG_CALIB_0,&pSystem->pbHygrometerCalibration[0]);
		pSystem->pcErr = "ReadIicRegs(HYGROMETER_REG_CALIB_0) -> 0x%08x\r\n";
	});


	/*
	 * Power up device
	 */
	MAY_DIE({
		WriteIicReg(pSystem,HYGROMETER_SLAVE_ADDRESS,HYGROMETER_REG_CTRL_REG1,HYGROMETER_BFLD_PD);
		pSystem->pcErr = "WriteIicReg(HYGROMETER_REG_CTRL_REG1::HYGROMETER_BFLD_PD) -> 0x%08x\r\n";
	});
	vTaskDelay(xOneMs);

    pSystem->bHygrometerOk = pdTRUE;
	configPRINTF( ( "Hygrometer started ok\r\n" ) );
    return;

L_DIE:
	configPRINTF( ( "ERROR: Hygrometer started not ok\r\n" ) );
	return;
}

static void StopHygrometer(System* pSystem)
{
    pSystem->bHygrometerOk = pdFALSE;
}

static void SampleHygrometer(System* pSystem)
{
	BaseType_t xTimeout;
	u8 b;
	u8 pbBuf[5];
	int	H0_T0_out, H1_T0_out, H_T_out;
	int H0_rh, H1_rh;
	u8	buffer[2];
	int tmp = 0;
	u16 value = 0;
	int T0_out, T1_out, T_out, T0_degC_x8_u16, T1_degC_x8_u16;
	int T0_degC, T1_degC;
	u8 buff2[4], tmp5 = 0;
	int tmp32 = 0;
	TickType_t xOneMs = MS_TO_TICKS( 1 );

    if(!pSystem->bHygrometerOk) {
        return;
    }
    pSystem->rc = XST_SUCCESS;

	/*
	 * NOTE: The one shot auto clears but it seems to take FIXME ms
	 * Our sampling period is >= 100ms so the one shot will auto clear by the next sample time??? FIXME
	 */
	MAY_DIE({
		WriteIicReg(pSystem,HYGROMETER_SLAVE_ADDRESS,HYGROMETER_REG_CTRL_REG2,HYGROMETER_BFLD_ONE_SHOT);
		pSystem->pcErr = "WriteIicReg(HYGROMETER_REG_CTRL_REG2::HYGROMETER_BFLD_ONE_SHOT) -> 0x%08x\r\n";
	});
	for(xTimeout = 10000; xTimeout-- > 0; ) {
		MAY_DIE({
			ReadIicReg(pSystem,HYGROMETER_SLAVE_ADDRESS,HYGROMETER_REG_CTRL_REG2,&b);
			pSystem->pcErr = "ReadIicReg(HYGROMETER_REG_CTRL_REG2) -> 0x%08x\r\n";
		});
		if(0 == (b & HYGROMETER_BFLD_ONE_SHOT)) {
			break;
		}
		vTaskDelay(xOneMs);
	}
	MAY_DIE({
		if(xTimeout <= 0) {
			pSystem->rc = XST_FAILURE;
			pSystem->pcErr = "Timed out waiting for HYGROMETER_BFLD_ONE_SHOT\r\n";
		}
	});

	for(xTimeout = 50; xTimeout-- > 0; ) {
		MAY_DIE({
			ReadIicRegs(pSystem,HYGROMETER_SLAVE_ADDRESS,5,HYGROMETER_REG_STATUS_REG,pbBuf);
			pSystem->pcErr = "ReadIicRegs(6@HYGROMETER_REG_STATUS_REG) -> 0x%08x\r\n";
		});
		if((HYGROMETER_BFLD_H_DA | HYGROMETER_BFLD_T_DA) == (pbBuf[0] & (HYGROMETER_BFLD_H_DA | HYGROMETER_BFLD_T_DA))) {
			break;
		}
		vTaskDelay(xOneMs);
	}
	MAY_DIE({
		if(xTimeout <= 0) {
			pSystem->rc = XST_FAILURE;
			pSystem->pcErr = "Timed out waiting for HYGROMETER P_DA and T_DA\r\n";
		}
	});

	/*
	 * REF: ST TN1218
	 * Interpreting humidity and temperature readings in the HTS221 digital humidity sensor
	 */

	buffer[0] = 0;
    buffer[1] = 0;

	/* 1. Read H0_rH and H1_rH coefficients */
	MAY_DIE({
		ReadIicReg(pSystem, HYGROMETER_SLAVE_ADDRESS, HYGROMETER_REG_H0_rH_x2 , &buffer[0]);
		pSystem->pcErr = "ReadIicReg(HYGROMETER_REG_H0_rH_x2)  -> 0x%08x\r\n";
	});
	MAY_DIE({
		ReadIicReg(pSystem, HYGROMETER_SLAVE_ADDRESS, HYGROMETER_REG_H1_rH_x2, &buffer[1]);
		pSystem->pcErr = "ReadIicReg(HYGROMETER_REG_H1_rH_x2) -> 0x%08x\r\n";
	});

	H0_rh = buffer[0]>>1;
	H1_rh = buffer[1]>>1;

	buffer[0] = 0; buffer[1] = 0;
	/*2. Read H0_T0_OUT */ 

	MAY_DIE({
		 ReadIicReg(pSystem, HYGROMETER_SLAVE_ADDRESS, HYGROMETER_REG_H0_T0_OUT_LSB, &buffer[0]);
		 pSystem->pcErr = "ReadIicReg(HYGROMETER_REG_H0_T0_OUT_LSB) -> 0x%08x\r\n";
	});
	MAY_DIE({
		 ReadIicReg(pSystem, HYGROMETER_SLAVE_ADDRESS, HYGROMETER_REG_H0_T0_OUT_MSB, &buffer[1]);
		 pSystem->pcErr = "ReadIicReg(HYGROMETER_REG_H0_T0_OUT_MSB -> 0x%08x\r\n";
	});

	H0_T0_out = (((u16)buffer[1])<<8) | (u16)buffer[0];

	buffer[0] = 0; buffer[1] = 0;
	/*3. Read H1_T0_OUT  */

	MAY_DIE({
		ReadIicReg(pSystem, HYGROMETER_SLAVE_ADDRESS, HYGROMETER_REG_H1_T0_OUT_LSB, &buffer[0]);
		pSystem->pcErr = "ReadIicReg(HYGROMETER_REG_H1_T0_OUT_LSB)  -> 0x%08x\r\n";
	});
	MAY_DIE({
		ReadIicReg(pSystem, HYGROMETER_SLAVE_ADDRESS, HYGROMETER_REG_H1_T0_OUT_MSB, &buffer[1]);
		pSystem->pcErr = "ReadIicReg(HYGROMETER_REG_H1_T0_OUT_MSB) -> 0x%08x\r\n";
	});

	H1_T0_out = (((u16)buffer[1])<<8) | (u16)buffer[0];

	buffer[0] = 0; buffer[1] = 0;
	/*4. Read H_T_OUT  */

	MAY_DIE({
		ReadIicReg(pSystem, HYGROMETER_SLAVE_ADDRESS, HYGROMETER_REG_HUMIDITY_OUT_L, &buffer[0]);
		pSystem->pcErr = "ReadIicReg( HYGROMETER_REG_HUMIDITY_OUT_L) -> 0x%08x\r\n";
	});
	MAY_DIE({
		ReadIicReg(pSystem, HYGROMETER_SLAVE_ADDRESS, HYGROMETER_REG_HUMIDITY_OUT_H, &buffer[1]);
		pSystem->pcErr = "ReadIicReg(HYGROMETER_REG_HUMIDITY_OUT_H -> 0x%08x\r\n";
	});

	H_T_out = (((u16)buffer[1])<<8) | (u16)buffer[0];

	/*5. Compute the RH [%] value by linear interpolation */
	value = 0;
	tmp = ((int)(H_T_out - H0_T0_out)) * ((int)(H1_rh - H0_rh));
	value = (u16) ((tmp/(H1_T0_out - H0_T0_out))+ H0_rh) ;

	/* Saturation condition*/
	if(value>1000) value = 1000;

    pSystem->fHygrometerHumidity = value;

        /**
	* @brief Read HTS221 temperature output registers, and calculate temperature.
	* @param Pointer to the returned temperature value that must be divided by 10 to get the value in ['C].
	* @retval Error code [HTS221_OK, HTS221_ERROR].
	*/
	tmp5 = 0; value = 0;
	buff2[0] = 0; buff2[1] = 0; buff2[2] = 0; buff2[3] = 0;

	/*1. Read from 0x32 & 0x33 registers the value of coefficients T0_degC_x8 and T1_degC_x8*/
    MAY_DIE({
        ReadIicReg(pSystem, HYGROMETER_SLAVE_ADDRESS, HYGROMETER_REG_T0_degC_x8, &buff2[0]);
        pSystem->pcErr = "ReadIicReg(HYGROMETER_REG_T0_degC_x8) -> 0x%08x\r\n";
	});
    MAY_DIE({
        ReadIicReg(pSystem, HYGROMETER_SLAVE_ADDRESS, HYGROMETER_REG_T1_degC_x8, &buff2[1]);
        pSystem->pcErr = "ReadIicReg(HYGROMETER_REG_T1_degC_x8) -> 0x%08x\r\n";
	});

	/*2. Read from 0x35 register the value of the MSB bits of T1_degC and T0_degC */
	MAY_DIE({
        ReadIicReg(pSystem, HYGROMETER_SLAVE_ADDRESS, HYGROMETER_REG_T1_T0_MSB, &tmp5);
		pSystem->pcErr = "ReadIicReg(HYGROMETER_REG_T1_T0_MSB) -> 0x%08x\r\n";
	});

	/*Calculate the T0_degC and T1_degC values*/
	T0_degC_x8_u16 = (((u16)(tmp5 & 0x03)) << 8) | ((u16)buff2[0]);
	T1_degC_x8_u16 = (((u16)(tmp5 & 0x0C)) << 6) | ((u16)buff2[1]);
	T0_degC = T0_degC_x8_u16>>3;
	T1_degC = T1_degC_x8_u16>>3;

	/*3. Read from 0x3C & 0x3D registers the value of T0_OUT*/
	/*4. Read from 0x3E & 0x3F registers the value of T1_OUT*/
	buff2[0] = 0;
    buff2[1] = 0;
    buff2[2] = 0;
    buff2[3] = 0;
	MAY_DIE({
        ReadIicReg(pSystem, HYGROMETER_SLAVE_ADDRESS, HYGROMETER_REG_T0_OUT_LSB, &buff2[0]);
        pSystem->pcErr = "ReadIicReg(HYGROMETER_REG_T0_OUT_LSB) -> 0x%08x\r\n";
	});
	MAY_DIE({
        ReadIicReg(pSystem, HYGROMETER_SLAVE_ADDRESS, HYGROMETER_REG_T0_OUT_MSB, &buff2[1]);
        pSystem->pcErr = "ReadIicReg(HYGROMETER_REG_T0_OUT_LSB) -> 0x%08x\r\n";
	});

	MAY_DIE({
        ReadIicReg(pSystem, HYGROMETER_SLAVE_ADDRESS, HYGROMETER_REG_T1_OUT_LSB, &buff2[2]);
        pSystem->pcErr = "ReadIicReg(HYGROMETER_REG_T1_OUT_LSB) -> 0x%08x\r\n";
	});
	MAY_DIE({
        ReadIicReg(pSystem, HYGROMETER_SLAVE_ADDRESS, HYGROMETER_REG_T1_OUT_MSB, &buff2[3]);
        pSystem->pcErr = "ReadIicReg(HYGROMETER_REG_T1_OUT_MSB) -> 0x%08x\r\n";
	});

	T0_out = (((u16)buff2[1])<<8) | (u16)buff2[0];
	T1_out = (((u16)buff2[3])<<8) | (u16)buff2[2];

	/* 5.Read from 0x2A & 0x2B registers the value T_OUT (ADC_OUT).*/
	buff2[0] = 0; buff2[1] = 0; buff2[2] = 0; buff2[3] = 0;
	MAY_DIE({
        ReadIicReg(pSystem, HYGROMETER_SLAVE_ADDRESS, HYGROMETER_REG_TEMP_OUT_L, &buff2[0]);
        pSystem->pcErr = "ReadIicReg(HYGROMETER_REG_TEMP_OUT_L) -> 0x%08x\r\n";
	});
	MAY_DIE({
        ReadIicReg(pSystem, HYGROMETER_SLAVE_ADDRESS, HYGROMETER_REG_TEMP_OUT_H, &buff2[1]);
        pSystem->pcErr = "ReadIicReg(HYGROMETER_REG_TEMP_OUT_H) -> 0x%08x\r\n";
	});

	T_out = (((u16)buff2[1])<<8) | (u16)buff2[0];

	/* 6. Compute the Temperature value by linear interpolation*/
	value = 0;

	tmp32 = (( int)(T_out - T0_out)) * (( int)(T1_degC - T0_degC));
	value = (tmp32 /(T1_out - T0_out)) + T0_degC;

    pSystem->fHygrometerTemperature = value;

L_DIE:
	return;
}

/*--------------------------------------------------------------------------------*/

static void StartPLTempSensor(System* pSystem)
{
	const TickType_t xOneMs = MS_TO_TICKS(1);

    pSystem->bThermocoupleOk = pdFALSE;
    pSystem->fThermocoupleBoardTemperature = 0;
    pSystem->fThermocoupleTemperature = 0;

	//Reset the SPI Peripheral, which takes 4 cycles, so wait a bit after reset
    XSpi_WriteReg(PL_SPI_BASEADDR, XSP_SRR_OFFSET, AXI_SPI_RESET_VALUE);
	vTaskDelay(xOneMs); //usleep(100);

	// Initialize the AXI SPI Controller with settings compatible with the MAX31855
    XSpi_WriteReg(PL_SPI_BASEADDR, XSP_CR_OFFSET, MAX31855_CR_INIT_MODE);

	// Deselect all slaves to start, then wait a bit for it to take affect
    XSpi_WriteReg(PL_SPI_BASEADDR, XSP_SSR_OFFSET, PL_SPI_CHANNEL_SEL_NONE);

	vTaskDelay(xOneMs); //usleep(100);

    pSystem->bThermocoupleOk = pdTRUE;
	configPRINTF( ("PL Thermocouple started - check state after first reading\r\n") );
}

static void StopPLTempSensor(System* pSystem)
{
    pSystem->bThermocoupleOk = pdFALSE;
}

static void XSpi_LowLevelExecute(System* pSystem, u32 qBaseAddress, BaseType_t xSPI_Channel, BaseType_t xByteCount, const u32* pqTxBuffer, u32* pqRxBuffer)
{
	BaseType_t xNumBytesRcvd = 0;
	BaseType_t xCount;
	const TickType_t xOneMs = MS_TO_TICKS(1);

	/*
	 * Initialize the Tx FIFO in the AXI SPI Controller with the transmit
	 * data contained in TxBuffer
	 */
	for (xCount = 0; xCount < xByteCount; pqTxBuffer++, xCount++)
	{
		XSpi_WriteReg(qBaseAddress, XSP_DTR_OFFSET, *pqTxBuffer);
	}

	// Assert the Slave Select, then wait a bit so it takes affect
	XSpi_WriteReg(qBaseAddress, XSP_SSR_OFFSET, xSPI_Channel);
	vTaskDelay(xOneMs); //usleep(100);

	/*
	 * Disable the Inhibit bit in the AXI SPI Controller's controler register
	 * This will release the AXI SPI Controller to release the transaction onto the bus
	 */
	XSpi_WriteReg(qBaseAddress, XSP_CR_OFFSET, MAX31855_CR_UNINHIBIT_MODE);

	/*
	 * Wait for the AXI SPI controller's transmit FIFO to transition to empty
	 * to make sure all the transmit data gets sent
	 */
	while (!(XSpi_ReadReg(qBaseAddress, XSP_SR_OFFSET) & XSP_SR_TX_EMPTY_MASK));

	/*
	 * Wait for the AXI SPI controller's Receive FIFO Occupancy register to
	 * show the expected number of receive bytes before attempting to read
	 * the Rx FIFO. Note the Occupancy Register shows Rx Bytes - 1
	 *
	 * If xByteCount number of bytes is sent, then by design, there must be
	 * xByteCount number of bytes received
	 */
	xByteCount--;
	while(xByteCount != XSpi_ReadReg(qBaseAddress, XSP_RFO_OFFSET)) {
		;
	}
	xByteCount++;

	/*
	 * The AXI SPI Controller's Rx FIFO has now received TxByteCount number
	 * of bytes off the SPI bus and is ready to be read.
	 *
	 * Transfer the Rx bytes out of the Controller's Rx FIFO into our code
	 * Keep reading one byte at a time until the Rx FIFO is empty
	 */
	xNumBytesRcvd = 0;
	while ((XSpi_ReadReg(qBaseAddress, XSP_SR_OFFSET) & XSP_SR_RX_EMPTY_MASK) == 0)
	{
		*pqRxBuffer++ = XSpi_ReadReg(qBaseAddress, XSP_DRR_OFFSET);
		xNumBytesRcvd++;
	}

	// Now that the Rx Data is retrieved, inhibit the AXI SPI Controller
	XSpi_WriteReg(qBaseAddress, XSP_CR_OFFSET, MAX31855_CR_INIT_MODE);
	// Deassert the Slave Select
	XSpi_WriteReg(qBaseAddress, XSP_SSR_OFFSET, PL_SPI_CHANNEL_SEL_NONE);

	/*
	 * If no data was sent or if we didn't receive as many bytes as
	 * were transmitted, then flag a failure
	 */
	if (xByteCount != xNumBytesRcvd) {
		pSystem->rc = ((xByteCount & 0xf) << 4) | (xNumBytesRcvd & 0xf);
        pSystem->pcErr = "XSpi_LowLevelExecute() -> 0x%08x\r\n";
		return;
	}

	pSystem->rc = XST_SUCCESS;
	return;
}

/**
 * @brief Sample Barometer and publish values
 *
 * @param[in] pSystem			System handle
 */
static void SamplePLTempSensor(System* pSystem)
{
	// TxBuffer is not used to communicate with the MAX31855 but it is still necessary
	//      for the XSPI utilities to function
	u32 pqTxBuffer[4] = {0,0,0,0};
	u32 pqRxBuffer[4] = {~0,~0,~0,~0};	// Initialize RxBuffer with all 1's
	s32 sqTemporaryValue = 0;
	s32 sqTemporaryValue2 = 0;
	float fMAX31855_internal_temp = 0.0f;
	float fMAX31855_thermocouple_temp = 0.0f;

    if(!pSystem->bThermocoupleOk) {
        return;
    }
    pSystem->rc = XST_SUCCESS;

	// Execute 4-byte read transaction.
	MAY_DIE({
        XSpi_LowLevelExecute(pSystem, (u32)PL_SPI_BASEADDR, (BaseType_t)PL_SPI_CHANNEL_SEL_0, (BaseType_t)4, pqTxBuffer, pqRxBuffer );
        if(XST_SUCCESS == pSystem->rc) {
            if(0) {
                ;
            } else if(pqRxBuffer[3] & 0x1) {
                pSystem->rc = XST_FAILURE;
                pSystem->pcErr = "Thermocouple: Open Circuit\r\n";
            } else if(pqRxBuffer[3] & 0x2) {
                pSystem->rc = XST_FAILURE;
                pSystem->pcErr = "Thermocouple: Short to GND\r\n";
            } else if(pqRxBuffer[3] & 0x4) {
                pSystem->rc = XST_FAILURE;
                pSystem->pcErr = "Thermocouple: Short to VCC\r\n";
            } else if(pqRxBuffer[1] & 0x01) {
                pSystem->rc = XST_FAILURE;
                pSystem->pcErr = "Thermocouple: Fault\r\n";
            }
        }
	});

    // Internal Temp
    {
        sqTemporaryValue = pqRxBuffer[2];  			// bits 11..4
        sqTemporaryValue = sqTemporaryValue << 4;		// shift left to make room for bits 3..0
        sqTemporaryValue2 = pqRxBuffer[3];				// bits 3..0 in the most significant spots
        sqTemporaryValue2 = sqTemporaryValue2 >> 4;	// shift right to get rid of extra bits and position
        sqTemporaryValue |= sqTemporaryValue2;		// Combine to get bits 11..0
        if((pqRxBuffer[2] & 0x80) == 0x80) {				// Check the sign bit and sign-extend if need be
            sqTemporaryValue |= 0xFFFFF800;
        }
        fMAX31855_internal_temp = (float)sqTemporaryValue / 16.0f;
        pSystem->fThermocoupleBoardTemperature = fMAX31855_internal_temp;
    }

    // Thermocouple Temp
    {
        sqTemporaryValue = pqRxBuffer[0];  			// bits 13..6
        sqTemporaryValue = sqTemporaryValue << 6;		// shift left to make room for bits 5..0
        sqTemporaryValue2 = pqRxBuffer[1];				// bits 5..0 in the most significant spots
        sqTemporaryValue2 = sqTemporaryValue2 >> 2;	// shift right to get rid of extra bits and position
        sqTemporaryValue |= sqTemporaryValue2;		// Combine to get bits 13..0
        if((pqRxBuffer[0] & 0x80) == 0x80) {				// Check the sign bit and sign-extend if need be
            sqTemporaryValue |= 0xFFFFE000;
        }
        fMAX31855_thermocouple_temp = (float)sqTemporaryValue / 4.0f;
        pSystem->fThermocoupleTemperature = fMAX31855_thermocouple_temp;
    }
    return;

L_DIE:
    return;
}

/*--------------------------------------------------------------------------------*/

static void StartSystem(System* pSystem)
{
	XIic_Config *pI2cConfig;
	XGpioPs_Config* pGpioConfig;
    int iLen;

    /*-----------------------------------------------------------------*/

	pSystem->bError = 0;
    pSystem->bBarometerOk = 0;
    pSystem->bHygrometerOk = 0;
    pSystem->bThermocoupleOk = 0;

    pSystem->rc = XST_SUCCESS;
    pSystem->pcErr = "\r\n";
    pSystem->xMQTTHandle = NULL;

    /*-----------------------------------------------------------------*/
    MAY_DIE({
        iLen = snprintf(
            (char*)pSystem->pbSensorTopic,
            SYSTEM_SENSOR_TOPIC_LENGTH+1,
            "compressor/%s-gateway-ultra96/cooling_system/1",
            clientcredentialGG_GROUP
            );
        if((iLen < 0) || (iLen > SYSTEM_SENSOR_TOPIC_LENGTH)) {
            pSystem->pbSensorTopic[0] = 0;
            pSystem->usSensorTopicLength = 0;
            pSystem->rc = XST_FAILURE;
            pSystem->pcErr = "Cannot compose system sensor topic: GroupID too long\r\n";
        } else {
            pSystem->pbSensorTopic[SYSTEM_SENSOR_TOPIC_LENGTH] = 0;
            pSystem->usSensorTopicLength = (uint16_t)strlen((const char*)pSystem->pbSensorTopic);
        }
    });

    MAY_DIE({
        iLen = snprintf(
            (char*)pSystem->pbShadowTopic,
            SYSTEM_SHADOW_TOPIC_LENGTH+1,
            "$aws/things/%s-gateway-ultra96/shadow/update",
            clientcredentialGG_GROUP
            );
        if((iLen < 0) || (iLen > SYSTEM_SENSOR_TOPIC_LENGTH)) {
            pSystem->pbShadowTopic[0] = 0;
            pSystem->usShadowTopicLength = 0;
            pSystem->rc = XST_FAILURE;
            pSystem->pcErr = "Cannot compose system shadow topic: GroupID too long\r\n";
        } else {
            pSystem->pbShadowTopic[SYSTEM_SHADOW_TOPIC_LENGTH] = 0;
            pSystem->usShadowTopicLength = (uint16_t)strlen((const char*)pSystem->pbShadowTopic);
        }
    });

    /*-----------------------------------------------------------------*/

	pGpioConfig = XGpioPs_LookupConfig(XPAR_PS7_GPIO_0_DEVICE_ID);
	configASSERT(pGpioConfig != NULL);

	MAY_DIE({
		pSystem->rc = XGpioPs_CfgInitialize(&pSystem->gpio, pGpioConfig, pGpioConfig->BaseAddr);
		pSystem->pcErr = "XGpioPs_CfgInitialize() -> 0x%08x\r\n";
	});
	XGpioPs_SetDirectionPin(&pSystem->gpio, LED_PIN, 1);
	XGpioPs_SetOutputEnablePin(&pSystem->gpio, LED_PIN, 1);
	BlinkLed(pSystem, 5, pdFALSE);

    /*-----------------------------------------------------------------*/

	pI2cConfig = XIic_LookupConfig(XPAR_IIC_0_DEVICE_ID);
	configASSERT(pI2cConfig != NULL);

	MAY_DIE({
		pSystem->rc = XIic_CfgInitialize(&pSystem->iic, pI2cConfig,	pI2cConfig->BaseAddress);
		pSystem->pcErr = "XIic_CfgInitialize() -> 0x%08x\r\n";
	});
	XIic_IntrGlobalDisable(pI2cConfig->BaseAddress);

	MAY_DIE({
		pSystem->rc = XIic_Start(&pSystem->iic);
		pSystem->pcErr = "XIic_Start() -> 0x%08x\r\n";
	});


    /*-----------------------------------------------------------------*/

	/* Create the MQTT client object and connect it to the MQTT broker. */
	MAY_DIE({
		prvCreateClientAndConnectToBroker(pSystem);
		if(XST_SUCCESS == pSystem->rc) {
			BlinkLed(pSystem, 5, pdTRUE);
		}
	});

	/*-----------------------------------------------------------------*/

    /*
     * Ignore system error, as each sensor has its own OK and will skip sampling
     */
    StartBarometer(pSystem);
    StartPLTempSensor(pSystem);
    StartHygrometer(pSystem);

    /*-----------------------------------------------------------------*/

	configPRINTF( ( "System started\r\n" ) );

	return;

	/*-----------------------------------------------------------------*/

L_DIE:
	StopSystem(pSystem);
}

static void StopSystem(System* pSystem)
{
	if(NULL != pSystem->xMQTTHandle) {
        prvPublishShadow(pSystem);
		/* Disconnect the client. */
		( void ) MQTT_AGENT_Disconnect( pSystem->xMQTTHandle, democonfigMQTT_TIMEOUT );
	}

	StopHygrometer(pSystem);
	StopPLTempSensor(pSystem);
	StopBarometer(pSystem);

	if(pSystem->iic.IsReady) {
		XIic_Stop(&pSystem->iic);
	}

	BlinkLed(pSystem, 5, pdFALSE);

	/* End the demo by deleting all created resources. */
	configPRINTF( ( "Sensor demo done.\r\n" ) );
	vTaskDelete( NULL ); /* Delete this task. */
}

/*--------------------------------------------------------------------------------*/

static void prvUZedIotTask( void * pvParameters )
{
	TickType_t xPreviousWakeTime;
    const TickType_t xSamplingPeriod = MS_TO_TICKS( SAMPLING_PERIOD_MS );
    u8 bFirst;
    System* pSystem = &g_tSystem;

	/* Avoid compiler warnings about unused parameters. */
    ( void ) pvParameters;

    StartSystem(pSystem);

	/* MQTT client is now connected to a broker.  Publish or perish! */
    /* Initialise the xLastWakeTime variable with the current time. */
    xPreviousWakeTime = xTaskGetTickCount();

    /*
     * Ignore errors in loop and continue forever
     */
    bFirst = 1;
	for(;;) {
		// Line up with next period boundary
		vTaskDelayUntil( &xPreviousWakeTime, xSamplingPeriod );

		// Publish all sensors
        pSystem->bError = 0;
		SampleBarometer(pSystem);
		SamplePLTempSensor(pSystem);
		SampleHygrometer(pSystem);

        prvPublishSensors(pSystem);
        if((pSystem->bLastReportedError != pSystem->bError) || bFirst) {
            pSystem->bLastReportedError = pSystem->bError;
            prvPublishShadow(pSystem);
        }
        bFirst = 0;
	}

	/* Not reached */
	StopSystem(pSystem);
}

/*-----------------------------------------------------------*/

void vStartMQTTUZedIotDemo( void )
{
    configPRINTF( ( "Creating UZedIot Task...\r\n" ) );

    /*
     * Create the task that publishes messages to the MQTT broker periodically
     */
    ( void ) xTaskCreate( prvUZedIotTask,        		            /* The function that implements the demo task. */
                          "UZedIot",                       		    /* The name to assign to the task being created. */
						  democonfigMQTT_UZED_IOT_TASK_STACK_SIZE, 	/* The size, in WORDS (not bytes), of the stack to allocate for the task being created. */
                          NULL,                                		/* The task parameter is not being used. */
                          democonfigMQTT_UZED_IOT_TASK_PRIORITY,    /* The priority at which the task being created will run. */
                          NULL                                      /* Not storing the task's handle. */
    					);
}

/*-----------------------------------------------------------*/
