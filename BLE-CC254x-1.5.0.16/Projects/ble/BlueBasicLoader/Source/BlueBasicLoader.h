/**************************************************************************************************
  Filename:       BlueBasic.h
**************************************************************************************************/

#ifndef BLUEBASIC_H
#define BLUEBASIC_H

#ifdef __cplusplus
extern "C"
{
#endif

/*********************************************************************
 * INCLUDES
 */

/*********************************************************************
 * CONSTANTS
 */

/*********************************************************************
 * MACROS
 */

// check for BBX product ID  
#ifdef USE_LED_BLE
#define LED_BLE_INIT \
  P1SEL = 0;\
  P1DIR = 0x01;\
  P1 = 0;\
  P0SEL = 0;\
  P0DIR = 0x10;\
  P0 = 0;
#define LED_BLE_ON P1 |= 1, P0 |= 0x10
#define LED_BLE_OFF P1 &= ~1, P0 &= ~0x10
#define LED_BLE_TOGGLE P1 ^= 1, P0 ^= 0x10
#define LED_BLE(a) P1 = ((P1 & ~1) | (a)), P0 = ((P0 & ~0x10) | ((a>0)<<4))
#endif  
  
/*********************************************************************
 * FUNCTIONS
 */

/*
 * Task Initialization for the BLE Application
 */
extern void BlueBasic_Init( uint8 task_id );

/*
 * Task Event Processor for the BLE Application
 */
extern uint16 BlueBasic_ProcessEvent( uint8 task_id, uint16 events );

extern uint8 toggleLed;
extern uint8 blueBasicLoader_TaskID;

/*********************************************************************
*********************************************************************/

#ifdef __cplusplus
}
#endif

#endif /* BLUEBASIC_H */
