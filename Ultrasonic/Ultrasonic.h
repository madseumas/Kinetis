/*
 * Ultrasonic.h

 *      Based heavily on code from Erich Styger (http://mcuoneclipse.com)
 */

#ifndef ULTRASONIC_H_
#define ULTRASONIC_H_

/**\file Ultrasonic.h
 * \brief Header for Ultrasonic.
 */

#include "TU2.h"

/**\enum US_EchoState
 * \brief State machine states
 */
typedef enum {
  ECHO_IDLE, /**< device not used */
  ECHO_TRIGGERED, /**< started trigger pulse */
  ECHO_MEASURE, /**< measuring echo pulse */
  ECHO_OVERFLOW, /**< measurement took too long */
  ECHO_FINISHED /**< measurement finished */
} US_EchoState; /**< enum US_EchoState */

/**\enum US_Sensor
 * \brief Sensor identifiers
 */
typedef enum {
	US_FRONT, /**< Front sensor */
	US_REAR, /**< Rear sensor */
	US_LEFT, /**< Left sensor */
	US_RIGHT /**<Right Sensor */
} US_Sensor;

/**\struct US_DeviceType
 * \brief US data structure
 */
typedef struct US_DeviceType {
  LDD_TDeviceData *trigDevice; /**< device handle for the Trigger pin */
  LDD_TDeviceData *echoDevice; /**< input capture device handle (echo pin) */
  volatile US_EchoState state; /**< state */
  TU2_TValueType capture; /**< input capture value */
  uint16_t lastValue_us; /**< last captured echo, in us */
  void (*setVal)(LDD_TDeviceData*); /**< function pointer for TRIG device */
  void (*clrVal)(LDD_TDeviceData*); /**< function pointer for TRIG device */
  US_Sensor id; /**< member id */
} US_DeviceType; /**< struct US_DeviceType */

/**\struct US_Devices
 * \brief holds a collection of US devices
 */
typedef struct US_Devices {
	US_DeviceType usDevice[4];
	uint8_t currentDevice;
} US_Devices;

/**\struct US_Message
 * \brief Struct for US Message queue
 */
typedef struct US_Message {
	US_Sensor sensor; /**< sensor identifier */
	uint16_t distance; /**< last ping distance */
} US_Message; /**< struct US_Message */

void US_Main(void);
void US_EventEchoCapture(LDD_TUserData *UserDataPtr);
void US_EventEchoOverflow(LDD_TUserData *UserDataPtr);
void US_Init(void);
uint16_t US_Measure_us(uint8_t s);

#endif /* ULTRASONIC_H_ */
