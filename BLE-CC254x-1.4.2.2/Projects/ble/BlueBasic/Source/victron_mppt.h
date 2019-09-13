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
