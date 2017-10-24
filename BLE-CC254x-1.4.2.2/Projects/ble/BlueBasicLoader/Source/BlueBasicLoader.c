/**************************************************************************************************
  Filename:       BlueBasic.c
  Revised:        $Date: 2010-08-06 08:56:11 -0700 (Fri, 06 Aug 2010) $
  Revision:       $Revision: 23333 $

  Description:    Basic interpreter for the CC254x BLE device.

  Copyright 2014 Tim Wilkinson. All rights reserved.

  Based on sample code with the following copyright notice:

  Copyright 2010 - 2013 Texas Instruments Incorporated. All rights reserved.

  IMPORTANT: Your use of this Software is limited to those specific rights
  granted under the terms of a software license agreement between the user
  who downloaded the software, his/her employer (which must be your employer)
  and Texas Instruments Incorporated (the "License").  You may not use this
  Software unless you agree to abide by the terms of the License. The License
  limits your use, and you acknowledge, that the Software may not be modified,
  copied or distributed unless embedded on a Texas Instruments microcontroller
  or used solely and exclusively in conjunction with a Texas Instruments radio
  frequency transceiver, which is integrated into your product.  Other than for
  the foregoing purpose, you may not use, reproduce, copy, prepare derivative
  works of, modify, distribute, perform, display or sell this Software and/or
  its documentation for any purpose.

  YOU FURTHER ACKNOWLEDGE AND AGREE THAT THE SOFTWARE AND DOCUMENTATION ARE
  PROVIDED �AS IS� WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS OR IMPLIED,
  INCLUDING WITHOUT LIMITATION, ANY WARRANTY OF MERCHANTABILITY, TITLE,
  NON-INFRINGEMENT AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT SHALL
  TEXAS INSTRUMENTS OR ITS LICENSORS BE LIABLE OR OBLIGATED UNDER CONTRACT,
  NEGLIGENCE, STRICT LIABILITY, CONTRIBUTION, BREACH OF WARRANTY, OR OTHER
  LEGAL EQUITABLE THEORY ANY DIRECT OR INDIRECT DAMAGES OR EXPENSES
  INCLUDING BUT NOT LIMITED TO ANY INCIDENTAL, SPECIAL, INDIRECT, PUNITIVE
  OR CONSEQUENTIAL DAMAGES, LOST PROFITS OR LOST DATA, COST OF PROCUREMENT
  OF SUBSTITUTE GOODS, TECHNOLOGY, SERVICES, OR ANY CLAIMS BY THIRD PARTIES
  (INCLUDING BUT NOT LIMITED TO ANY DEFENSE THEREOF), OR OTHER SIMILAR COSTS.

  Should you have any questions regarding your right to use this Software,
  contact Texas Instruments Incorporated at www.TI.com.
**************************************************************************************************/

/*********************************************************************
 * INCLUDES
 */

#include "bcomdef.h"
#include "OSAL.h"
#include "OSAL_PwrMgr.h"

#include "OnBoard.h"

#include "gatt.h"

#include "hci.h"

#include "gapgattserver.h"
#include "gattservapp.h"
#include "devinfoservice.h"
#include "linkdb.h"

#include "observer.h"
#include "peripheral.h"

#include "gapbondmgr.h"

#include "BlueBasicLoader.h"

#if defined FEATURE_OAD
  #include "oad.h"
  #include "oad_target.h"
#endif

/*********************************************************************
 * MACROS
 */

/*********************************************************************
 * CONSTANTS
 */

// Whether to enable automatic parameter update request when a connection is formed
#define DEFAULT_ENABLE_UPDATE_REQUEST         TRUE

// What is the advertising interval when device is discoverable (units of 625us, 160=100ms, 1600=1s)
#define DEFAULT_ADVERTISING_INTERVAL          160

// Limited discoverable mode advertises for 30.72s, and then stops
// General discoverable mode advertises indefinitely

// Connection interval (units of 1.25ms)
#define DEFAULT_DESIRED_MIN_CONN_INTERVAL             16 // 20ms
#define DEFAULT_DESIRED_MAX_CONN_INTERVAL             32 // 40ms

// Slave latency to use if automatic parameter update request is enabled
#define DEFAULT_DESIRED_SLAVE_LATENCY         0

// Supervision timeout value (units of 10ms, 1000=10s) if automatic parameter update request is enabled
#define DEFAULT_DESIRED_CONN_TIMEOUT          1000

// Connection Pause Peripheral time value (in seconds)
#define DEFAULT_CONN_PAUSE_PERIPHERAL         6


#define INVALID_CONNHANDLE                    0xFFFF

#define BLUEBASICLOADER_START_DEVICE_EVT        0x0001

/*********************************************************************
 * TYPEDEFS
 */

/*********************************************************************
 * GLOBAL VARIABLES
 */
uint8 blueBasicLoader_TaskID;   // Task ID for internal task/event processing

/*********************************************************************
 * EXTERNAL VARIABLES
 */

/*********************************************************************
 * EXTERNAL FUNCTIONS
 */

/*********************************************************************
 * LOCAL VARIABLES
 */

#include "gatt_uuid.h"
#include "gap.h"

#define CONSOLE_PROFILE_SERV_UUID 0x3A, 0xA7, 0xBD, 0x64, 0x0a, 0xF7, 0xA3, 0xB5, 0x8D, 0x44, 0x16, 0x16, 0x91, 0x9E, 0xFB, 0x25

static CONST uint8 consoleAdvert[] =
{
  0x02, GAP_ADTYPE_FLAGS, GAP_ADTYPE_FLAGS_GENERAL|GAP_ADTYPE_FLAGS_BREDR_NOT_SUPPORTED,
  0x11, GAP_ADTYPE_128BIT_MORE, CONSOLE_PROFILE_SERV_UUID,
  0x09, GAP_ADTYPE_LOCAL_NAME_COMPLETE, 'B', 'A', 'S', 'I', 'C', '#', '?', '?'
};

#if 0
// GAP - SCAN RSP data (max size = 31 bytes)
static uint8 scanRspData[] =
{
  // connection interval range
  0x05,   // length of this data
  GAP_ADTYPE_SLAVE_CONN_INTERVAL_RANGE,
  LO_UINT16( DEFAULT_DESIRED_MIN_CONN_INTERVAL ),   // 100ms
  HI_UINT16( DEFAULT_DESIRED_MIN_CONN_INTERVAL ),
  LO_UINT16( DEFAULT_DESIRED_MAX_CONN_INTERVAL ),   // 1s
  HI_UINT16( DEFAULT_DESIRED_MAX_CONN_INTERVAL ),

  // Tx power level
  0x02,   // length of this data
  GAP_ADTYPE_POWER_LEVEL,
  0       // 0dBm
};
#endif

/*********************************************************************
 * LOCAL FUNCTIONS
 */

/*********************************************************************
 * PROFILE CALLBACKS
 */

// GAP Role Callbacks
static gapRolesCBs_t blueBasic_PeripheralCBs =
{
  NULL,                     // Profile State Change Callbacks
  NULL,                     // When a valid RSSI is read from controller
//  NULL                      // Device found
};

#ifdef GAP_BOND_MGR
// GAP Bond Manager Callbacks
static gapBondCBs_t blueBasic_BondMgrCBs =
{
  NULL,                     // Passcode callback (not used by application)
  NULL                      // Pairing / Bonding state Callback (not used by application)
};
#endif

/*********************************************************************
 * PUBLIC FUNCTIONS
 */

/*********************************************************************
 * @fn      BlueBasic_Init
 *
 * @brief   Initialization function for the Blue Basic App Task.
 *          This is called during initialization and should contain
 *          any application specific initialization (ie. hardware
 *          initialization/setup, table initialization, power up
 *          notificaiton ... ).
 *
 * @param   task_id - the ID assigned by OSAL.  This ID should be
 *                    used to send messages and set timers.
 *
 * @return  none
 */
void BlueBasic_Init( uint8 task_id )
{
  blueBasicLoader_TaskID = task_id;

  // Setup the GAP
  VOID GAP_SetParamValue( TGAP_CONN_PAUSE_PERIPHERAL, DEFAULT_CONN_PAUSE_PERIPHERAL );

#if 0  
  // Set advertising interval
  GAP_SetParamValue( TGAP_LIM_DISC_ADV_INT_MIN, DEFAULT_ADVERTISING_INTERVAL );
  GAP_SetParamValue( TGAP_LIM_DISC_ADV_INT_MAX, DEFAULT_ADVERTISING_INTERVAL );
  GAP_SetParamValue( TGAP_GEN_DISC_ADV_INT_MIN, DEFAULT_ADVERTISING_INTERVAL );
  GAP_SetParamValue( TGAP_GEN_DISC_ADV_INT_MAX, DEFAULT_ADVERTISING_INTERVAL );
#endif
  GAPRole_SetParameter( GAPROLE_ADVERT_DATA, sizeof(consoleAdvert), (void*)consoleAdvert );

#if 0  
  // Setup the GAP Peripheral Role Profile
  {
    uint8 initial_advertising_enable = TRUE;

    // By setting this to zero, the device will go into the waiting state after
    // being discoverable for 30.72 second, and will not being advertising again
    // until the enabler is set back to TRUE
    //uint16 gapRole_AdvertOffTime = 0;

    uint8 enable_update_request = DEFAULT_ENABLE_UPDATE_REQUEST;
    uint16 desired_min_interval = DEFAULT_DESIRED_MIN_CONN_INTERVAL;
    uint16 desired_max_interval = DEFAULT_DESIRED_MAX_CONN_INTERVAL;
    uint16 desired_slave_latency = DEFAULT_DESIRED_SLAVE_LATENCY;
    uint16 desired_conn_timeout = DEFAULT_DESIRED_CONN_TIMEOUT;

    // Set the GAP Role Parameters
    GAPRole_SetParameter( GAPROLE_ADVERT_ENABLED, sizeof( uint8 ), &initial_advertising_enable );
    //GAPRole_SetParameter( GAPROLE_ADVERT_OFF_TIME, sizeof( uint16 ), &gapRole_AdvertOffTime );

    //GAPRole_SetParameter( GAPROLE_SCAN_RSP_DATA, sizeof ( scanRspData ), scanRspData );

    GAPRole_SetParameter( GAPROLE_PARAM_UPDATE_ENABLE, sizeof( uint8 ), &enable_update_request );
    GAPRole_SetParameter( GAPROLE_MIN_CONN_INTERVAL, sizeof( uint16 ), &desired_min_interval );
    GAPRole_SetParameter( GAPROLE_MAX_CONN_INTERVAL, sizeof( uint16 ), &desired_max_interval );
    GAPRole_SetParameter( GAPROLE_SLAVE_LATENCY, sizeof( uint16 ), &desired_slave_latency );
    GAPRole_SetParameter( GAPROLE_TIMEOUT_MULTIPLIER, sizeof( uint16 ), &desired_conn_timeout );
  }
#endif  
  
  
#ifdef GAP_BOND_MGR
  // Setup the GAP Bond Manager
  {
    uint32 passkey = 0; // passkey "000000"
    uint8 pairMode = GAPBOND_PAIRING_MODE_WAIT_FOR_REQ;
    uint8 mitm = TRUE;
    uint8 ioCap = GAPBOND_IO_CAP_DISPLAY_ONLY;
    uint8 bonding = TRUE;
    GAPBondMgr_SetParameter( GAPBOND_DEFAULT_PASSCODE, sizeof ( uint32 ), &passkey );
    GAPBondMgr_SetParameter( GAPBOND_PAIRING_MODE, sizeof ( uint8 ), &pairMode );
    GAPBondMgr_SetParameter( GAPBOND_MITM_PROTECTION, sizeof ( uint8 ), &mitm );
    GAPBondMgr_SetParameter( GAPBOND_IO_CAPABILITIES, sizeof ( uint8 ), &ioCap );
    GAPBondMgr_SetParameter( GAPBOND_BONDING_ENABLED, sizeof ( uint8 ), &bonding );
  }
#endif

#if 0  // BlueSolar LED is on P0(4)  
  P0SEL = 0; // Configure Port 0 as GPIO
  P1SEL = 0; // Configure Port 1 as GPIO
  P2SEL = 0; // Configure Port 2 as GPIO

  P0DIR = 0x10; // Port 0 pins P0.4 as output, all others as input
  P1DIR = 0x00; // All port 1 pins (P1.0-P1.7) as input
  P2DIR = 0x00; // All port 1 pins (P2.0-P2.4) as input

  P0 = 0x10; // LED on 
  P1 = 0;   // All pins on port 1 to low
  P2 = 0;   // All pins on port 2 to low
#endif  
  
  // Initialize GATT attributes
  GGS_AddService( GATT_ALL_SERVICES );            // GAP
  GATTServApp_AddService( GATT_ALL_SERVICES );    // GATT attributes
#if defined FEATURE_OAD
  VOID OADTarget_AddService();                    // OAD Profile
#endif
  DevInfo_AddService();                           // Device information

  // Setup a delayed profile startup
  osal_set_event( blueBasicLoader_TaskID, BLUEBASICLOADER_START_DEVICE_EVT );
}

/*********************************************************************
 * @fn      BlueBasic_ProcessEvent
 *
 * @brief   Blue Basic Application Task event processor.  This function
 *          is called to process all events for the task.  Events
 *          include timers, messages and any other user defined events.
 *
 * @param   task_id  - The OSAL assigned task ID.
 * @param   events - events to process.  This is a bit map and can
 *                   contain more than one event.
 *
 * @return  events not processed
 */
uint16 BlueBasic_ProcessEvent( uint8 task_id, uint16 events )
{
  VOID task_id; // OSAL required parameter that isn't used in this function

  if ( events & SYS_EVENT_MSG )
  {
    uint8 *pMsg;

    if ( (pMsg = osal_msg_receive( blueBasicLoader_TaskID )) != NULL )
    {
      // Release the OSAL message
      VOID osal_msg_deallocate( pMsg );
    }

    // return unprocessed events
    return (events ^ SYS_EVENT_MSG);
  }

  if ( events & BLUEBASICLOADER_START_DEVICE_EVT )
  {
    // Start the Device
    VOID GAPRole_StartDevice( &blueBasic_PeripheralCBs );

#ifdef GAP_BOND_MGR
    // Start Bond Manager
    VOID GAPBondMgr_Register( &blueBasic_BondMgrCBs );
#endif

 //  P0 = 0; // BlueSolar LED off
   
    return ( events ^ BLUEBASICLOADER_START_DEVICE_EVT );
  }

  // Discard unknown events
  return 0;
}

/*********************************************************************
*********************************************************************/
