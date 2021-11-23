/**************************************************************************************************
  Filename:       BlueBasic_Main.c
  Revised:        $Date: 2010-07-06 15:39:18 -0700 (Tue, 06 Jul 2010) $
  Revision:       $Revision: 22902 $

  Description:    This file contains the main and callback functions for
                  the Simple BLE Peripheral sample application.

  Copyright 2010 - 2011 Texas Instruments Incorporated. All rights reserved.

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
  PROVIDED �AS IS" WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS OR IMPLIED,
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

/**************************************************************************************************
 *                                           Includes
 **************************************************************************************************/
/* Hal Drivers */
#include "hal_types.h"
#include "hal_key.h"
#include "hal_timer.h"
#include "hal_drivers.h"
#include "hal_led.h"

/* OSAL */
#include "OSAL.h"
#include "OSAL_Tasks.h"
#include "OSAL_PwrMgr.h"
#include "osal_snv.h"
#include "OnBoard.h"

/**************************************************************************************************
 * FUNCTIONS
 **************************************************************************************************/

/**************************************************************************************************
 * Image header (for OAD builds)
 **************************************************************************************************/

#ifdef FEATURE_OAD_HEADER

typedef uint8 bStatus_t;

#include "oad.h"
#include "oad_target.h"

#define OAD_FLASH_PAGE_MULT  ((uint16)(HAL_FLASH_PAGE_SIZE / HAL_FLASH_WORD_SIZE))

#pragma location="IMAGE_HEADER"
const __code img_hdr_t _imgHdr = {
  0xFFFF,                     // CRC-shadow must be 0xFFFF for everything else
  OAD_IMG_VER( OAD_IMAGE_VERSION ), // 15-bit Version #, left-shifted 1; OR with Image-B/Not-A bit.
  OAD_IMG_R_AREA * OAD_FLASH_PAGE_MULT,
  {'B', 'B', 'B', 'B'},       // ImgB User-Id
  { 0xFF, 0xFF, 0xFF, 0xFF }  // Reserved
};
#pragma required=_imgHdr

#pragma location="AES_HEADER"
static const __code aes_hdr_t _aesHdr = {
 { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF },
 { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B },  // Dummy Nonce
 { 0xFF, 0xFF, 0xFF, 0xFF }   // Spare
};
#pragma required=_aesHdr

#endif

#if defined (FEATURE_CALIBRATION) && (!defined (XOSC32K_INSTALLED) || (defined (XOSC32K_INSTALLED) && (XOSC32K_INSTALLED == TRUE)))
#define MEASURE_PERIODE_XOSC32K() measure_periode_xosc32k()
uint16 periode_32k = 0;

// measure 32KHz periode in 32MHz ticks
// it intentionally locks up the CPU by more than 500 ms for a stable xtal
#pragma inline=forced
inline static void measure_periode_xosc32k()
  {
    HAL_DISABLE_INTERRUPTS();
    PMUX |= 0xB0;  // 1011 0000  switch 32 KHz clock to P0.3
    // since we manipulated the XOSC, it needs some time to settle
    T1CNTL = 0; // clear
    CLKCONCMD |= 0x04; // set tick speed to 16 MHz
    T1CTL = 0x0C | 0x01; // start free running Ticks/128
    while( T1STAT != 0x20)
      ; // wait until overflow  1/16 MHz*128*65536 = 524.288 ms
    CLKCONCMD &= ~0x1c; // set tick speed to 32MHz
    uint8 timeout = 0;
    // clocks are now setup
    // check if the 32KHz crystal runs
    T2CTRL = 1; // start async
    T2CTRL = 0; // stop  
    T1CNTL = 0; // clear
    T2CTRL = 3; // start sync
    while (!(T2CTRL & 4) && --timeout)
      ;   // wait until started
    T1CTL = 1; // start free running 32 MHz
    timeout = 0;
    T2CTRL = 2; // stop sync
    while (T2CTRL & 4 && --timeout)
      ;   // wait until stop
    T1CTL = 0; // stop
    if (timeout > 0)
    {
      periode_32k =  T1CNTL;
      periode_32k |= T1CNTH << 8; // should result in 891
      // nominal periode is 32MHz / 32768Hz = 976.5625
      periode_32k += 84; // add clock offset, should result in 977
    }
    // switch 32 MHz to P1.2 via Timer 1
    T1CNTL = 0; // clear
    T1CTL = 2; // modulo
    T1CCTL0 = 0x14;
    T1CC0L = 0;
    T1CC0H = 0;
    P1SEL |= 4;
    P2SEL |= 8;
    P1DIR |= 4;
    P2DIR |= 0x80;
    PERCFG |= 0x40; // timer 1 alt. 2
//    while (P2DIR & 0x80)
//        ;  // hang in
  }
#else
#define MEASURE_PERIODE_XOSC32K()
#endif

__data __no_init uint8 boot_counter;
__data extern uint8 JumpToImageAorB;

/**************************************************************************************************
 * @fn          main
 *
 * @brief       Start of application.
 *
 * @param       none
 *
 * @return      none
 **************************************************************************************************
 */
int main(void)
{
  if (boot_counter == 0xa5)
  {
    boot_counter = 0;
    JumpToImageAorB = 0;
    // Simulate a reset for the Application code by an absolute jump to the expected INTVEC addr.
    asm("LJMP 0x0830");
  }
  boot_counter |= 0xa0;
  boot_counter++;
  
  /* Initialize hardware */
  HAL_BOARD_INIT();
  
  /* measure XOSC32K and swizch the clock to P0.3 */
  MEASURE_PERIODE_XOSC32K();
  
  // Initialize board I/O
  InitBoard( OB_COLD );

  /* Initialze the HAL driver */
  HalDriverInit();

  /* Initialize LL */

  /* Initialize the operating system */
  osal_init_system();

  /* Enable interrupts */
  HAL_ENABLE_INTERRUPTS();

  // Final board initialization
  InitBoard( OB_READY );
  
#if defined ( FEATURE_BOOST_CONVERTER )
  // Turn boost converter on by default
  extern unsigned char BlueBasic_powerMode;
  BlueBasic_powerMode = 1;
#if FEATURE_BOOST_CONVERTER == P2_0
  P2DIR |= 1;
#else
#error "Unknown boost converter location"
#endif
  FEATURE_BOOST_CONVERTER = 1;
#endif

#if defined ( POWER_SAVING )
  osal_pwrmgr_device( PWRMGR_BATTERY );
#endif

  /* Start OSAL */
  osal_start_system(); // No Return from here

  return 0;
}

/**************************************************************************************************
                                           CALL-BACKS
**************************************************************************************************/


/*************************************************************************************************
**************************************************************************************************/
