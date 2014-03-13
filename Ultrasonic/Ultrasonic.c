/*
 * Ultrasonic.c

 *      Based heavily on Erich Styger's Ultrasonic code (http://mcuoneclipse.com)
 */

#include "USTRIGFRONT.h"
#include "USTRIGREAR.h"
#include "USTRIGLEFT.h"
#include "USTRIGRIGHT.h"
#include "Ultrasonic.h"
#include "FRTOS1.h"
#include "TU2.h"
#include "WAIT1.h"

static uint32_t calcAirspeed_mms(uint8_t temperatureCelsius);
static uint16_t US_usToMM(uint16_t microseconds, uint16_t temperature);


US_Devices usDevices; /**< device handle for the ultrasonic devices */
volatile uint8_t usCounter; /**< global counter for looping through devices */
uint16_t temperature = 260; /**< used for distance calculations */

extern volatile xQueueHandle usQueueHandle; /**< ultrasonic sensors messaging queue - extern */
extern volatile xQueueHandle musQueueHandle; /**< main->ultrasonic messaging queue - extern */


/**\fn US_Main(void)
 * \brief Main task loop for US sensors
 *
 * Main task loop for Ultrasonic sensors
 */
void US_Main(void)
{

	uint16_t microseconds, distance, tMessage = 0;

	US_Init();

	/* delay for 1/4 second to allow other sensors to init */
	FRTOS1_vTaskDelay(250/portTICK_RATE_MS);

	US_Message message;

	for(;;)
	{

		/* check for a temperature message and update temperature */
		if(xQueueReceive(musQueueHandle, &tMessage, (portTickType)1))
		{
			temperature = tMessage;
		}

		/* Loop through each sensor, measure, and send off to the queue */
		for (usCounter = 0; usCounter < 4; usCounter++)
		{
			usDevices.currentDevice = usCounter;
			microseconds = US_Measure_us(usCounter);
			if (microseconds > 0)
			{
				distance = US_usToMM(microseconds, temperature);
			}
			else
			{
				distance = 0; /* overflowed */
			}
			message.sensor = usCounter;
			message.distance = distance;
			xQueueSend(usQueueHandle, (void*)&message, (portTickType)5); /*send the message, block up to 5 ticks if queue isn't free */
			FRTOS1_vTaskDelay(10/portTICK_RATE_MS);
		}
	}
}

/**\fn US_Init(void)
 * \brief Init for US sensorss s
 *
 *
 * Initializes Ultrasonic sensors
 */
void US_Init(void)
{
	/* Front sensor */
	usDevices.usDevice[0].state = ECHO_IDLE;
	usDevices.usDevice[0].capture = 0;
	usDevices.usDevice[0].trigDevice = USTRIGFRONT_Init(NULL);
	usDevices.usDevice[0].echoDevice = TU2_Init(&usDevices);
	usDevices.usDevice[0].clrVal = USTRIGFRONT_ClrVal;
	usDevices.usDevice[0].setVal = USTRIGFRONT_SetVal;
	usDevices.usDevice[0].id = US_FRONT;

	/* Rear sensor */
	usDevices.usDevice[1].state = ECHO_IDLE;
	usDevices.usDevice[1].capture = 0;
	usDevices.usDevice[1].trigDevice = USTRIGREAR_Init(NULL);
	usDevices.usDevice[1].echoDevice = usDevices.usDevice[0].echoDevice;
	usDevices.usDevice[1].clrVal = USTRIGREAR_ClrVal;
	usDevices.usDevice[1].setVal = USTRIGREAR_SetVal;
	usDevices.usDevice[1].id = US_REAR;

	/* Left sensor */
	usDevices.usDevice[2].state = ECHO_IDLE;
	usDevices.usDevice[2].capture = 0;
	usDevices.usDevice[2].trigDevice = USTRIGLEFT_Init(NULL);
	usDevices.usDevice[2].echoDevice = usDevices.usDevice[0].echoDevice;
	usDevices.usDevice[2].clrVal = USTRIGLEFT_ClrVal;
	usDevices.usDevice[2].setVal = USTRIGLEFT_SetVal;
	usDevices.usDevice[3].id = US_LEFT;

	/* Left sensor */
	usDevices.usDevice[3].state = ECHO_IDLE;
	usDevices.usDevice[3].capture = 0;
	usDevices.usDevice[3].trigDevice = USTRIGRIGHT_Init(NULL);
	usDevices.usDevice[3].echoDevice = usDevices.usDevice[0].echoDevice;
	usDevices.usDevice[3].clrVal = USTRIGRIGHT_ClrVal;
	usDevices.usDevice[3].setVal = USTRIGRIGHT_SetVal;
	usDevices.usDevice[3].id = US_RIGHT;
}

/**\fn US_EventEchoCapture(LDD_TUserData *UserDataPtr)
 * \brief Echo capture event
 *
 * Called by interrupt from TU for echo capture.
 */
void US_EventEchoCapture(LDD_TUserData *UserDataPtr)
{
	US_Devices *ptr = (US_Devices *)UserDataPtr;

	if (ptr->usDevice[ptr->currentDevice].state==ECHO_TRIGGERED)
	{ /* 1st edge, this is the raising edge, start measurement */
		TU2_ResetCounter(ptr->usDevice[ptr->currentDevice].echoDevice);
		ptr->usDevice[ptr->currentDevice].state = ECHO_MEASURE;
	}
	else if (ptr->usDevice[ptr->currentDevice].state==ECHO_MEASURE)
	{ /* 2nd edge, this is the falling edge: use measurement */
		(void)TU2_GetCaptureValue(ptr->usDevice[ptr->currentDevice].echoDevice, ptr->currentDevice, &ptr->usDevice[ptr->currentDevice].capture);
		ptr->usDevice[ptr->currentDevice].state = ECHO_FINISHED;
	}
}

/**\fn US_EventEchoOverflow(LDD_TUserData *UserDataPtr)
 * \brief Echo overflow event
 *
 * Called by interrupt from TU for echo overflow.
 */
void US_EventEchoOverflow(LDD_TUserData *UserDataPtr)
{
	US_Devices *ptr;
	ptr = (US_Devices*)UserDataPtr;
	ptr->usDevice[ptr->currentDevice].state = ECHO_OVERFLOW;
}

/**\fn US_Measure_us(uint8_t s)
 * \brief Measures ping time
 *
 * Measures the amount of time a ping takes to return
 */
uint16_t US_Measure_us(uint8_t s)
{
	/* send 10us pulse on TRIG line. */
	usDevices.usDevice[s].setVal(usDevices.usDevice[s].trigDevice);
	WAIT1_Waitus(10);
	usDevices.usDevice[s].state = ECHO_TRIGGERED;
	usDevices.usDevice[s].clrVal(usDevices.usDevice[s].trigDevice);

	while(usDevices.usDevice[s].state!=ECHO_FINISHED)
	{
		/* measure echo pulse */
		if (usDevices.usDevice[s].state==ECHO_OVERFLOW)
		{
			/* measurement took too long? */
  			usDevices.usDevice[s].state = ECHO_IDLE; /* go back to idle */
  			usDevices.usDevice[s].lastValue_us = 0;
			return 0; /* no echo, error case */
		}
	}
	usDevices.usDevice[s].lastValue_us = (usDevices.usDevice[s].capture*1000UL)/(TU2_CNT_INP_FREQ_U_0/1000);
	return usDevices.usDevice[s].lastValue_us;
}

static uint32_t calcAirspeed_mms(uint8_t temperatureCelsius)
{
	/* Return the airspeed depending on the temperature, in millimeters per second */
	uint32_t airspeed; /* millimeters per second */

	airspeed = 331300 + (606 * (temperatureCelsius/10)); /* dry air, 0% humidity, see http://en.wikipedia.org/wiki/Speed_of_sound */
	airspeed -= (airspeed/100000)*17; /* factor in a relative humidity of ~40% */
	return airspeed;
}


static uint16_t US_usToMM(uint16_t microseconds, uint16_t temperature)
{
	uint32_t airspeed_mms = calcAirspeed_mms(temperature);
	uint32_t tdistance = microseconds*100000UL;
	uint16_t distance = (tdistance/airspeed_mms)/2;
	return distance;
}
