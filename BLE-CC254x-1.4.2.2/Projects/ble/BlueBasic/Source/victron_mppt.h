////////////////////////////////////////////////////////////////////////////////
// BlueBasic driver for Victron VE.Direct connected MPPT controller
////////////////////////////////////////////////////////////////////////////////
//
// Authors:
//      Kai Scheffer <kai@scheffer.ch>
//
// (C) 2019 Kai Scheffer, BlueBattery.com
//
// victron_mppt.h

#ifndef VICTRON_MPPT_H
#define VICTRON_MPPT_H

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

//#define MPPT_FORWARD_ASYNC
//#define MPPT_FORWARD_UNKNOWN

#ifndef MPPT_AS_VOTRONIC
// emulate data like Votronic
#define MPPT_AS_VOTRONIC  0
#endif

#if defined(MPPT_MODE_TEXT) && defined(MPPT_MODE_HEX) && (MPPT_MODE_HEX) && (MPPT_MODE_TEXT)
#error Could not enable MPPT text and HEX mode at the same time
#endif

#ifndef MPPT_MODE_HEX
#define MPPT_MODE_HEX 0
#endif

#ifndef MPPT_MODE_TEXT
#if MPPT_MODE_HEX
#define MPPT_MODE_TEXT 0
#else
#define MPPT_MODE_TEXT 1
#endif
#endif   
   
/*********************************************************************
 * FUNCTIONS
 */

extern void process_mppt(uint8 port, uint8 len);  
  
/*********************************************************************
*********************************************************************/

#ifdef __cplusplus
}
#endif

#endif /* VICTRON_MPPT_H */
