/***************************************************************************************************
|* mrfCommon.h -- Micro-Research Finland (MRF) Event System Series Common Defintions
|*
|*--------------------------------------------------------------------------------------------------
|* Author:   Eric Bjorklund
|* Date:     19 October 2009
|*
|*--------------------------------------------------------------------------------------------------
|* MODIFICATION HISTORY:
|* 19 Oct 2008  E.Bjorklund     Adapted from the original software for the APS Register Map
|*
|*--------------------------------------------------------------------------------------------------
|* MODULE DESCRIPTION:
|*   This header file contains various constants and defintions used by the Micro Research Finland
|*   event system. The definitions in this file are used by both driver and device support modules,
|*   as well as user code that calls the device support interface.
|*
|*------------------------------------------------------------------------------
|* HARDWARE SUPPORTED:
|*   Series 2xx Event Generator and Event Receiver Cards
|*   APS Register Mask
|*   Modular Register Mask
|*
|*------------------------------------------------------------------------------
|* OPERATING SYSTEMS SUPPORTED:
|*   vxWorks
|*   RTEMS
|*
\**************************************************************************************************/

/**************************************************************************************************
|*                                     COPYRIGHT NOTIFICATION
|**************************************************************************************************
|*  
|* THE FOLLOWING IS A NOTICE OF COPYRIGHT, AVAILABILITY OF THE CODE,
|* AND DISCLAIMER WHICH MUST BE INCLUDED IN THE PROLOGUE OF THE CODE
|* AND IN ALL SOURCE LISTINGS OF THE CODE.
|*
|**************************************************************************************************
|*
|* This software is distributed under the EPICS Open License Agreement which
|* can be found in the file, LICENSE, included with this distribution.
|*
\*************************************************************************************************/

#ifndef MRF_COMMON_H
#define MRF_COMMON_H

/**************************************************************************************************/
/*  Include Header Files from the Common Utilities                                                */
/**************************************************************************************************/

#include <epicsTypes.h>                        /* EPICS Architecture-independent type definitions */
#include <debugPrint.h>                        /* SLAC Debug print utility                        */


/**************************************************************************************************/
/*  MRF Event System Constants                                                                    */
/**************************************************************************************************/

#define MRF_NUM_EVENTS              256        /* Number of possible events                       */
#define MRF_MAX_DATA_BUFFER        2048        /* Maximum size of the distributed data buffer     */
#define MRF_EVENT_FIFO_SIZE         512        /* Size of EVR/EVG event FIFO                      */
#define MRF_FRAC_SYNTH_REF         24.0        /* Fractional Synth reference frequency (MHz).     */
#define MRF_DEF_CLOCK_SPEED       125.0        /* Default event clock speed is 125 MHz.           */
#define MRF_SN_BYTES                  6        /* Number of bytes in serial number                */
#define MRF_SN_STRING_SIZE           18        /* Size of serial number string (including NULL)   */


/**************************************************************************************************/
/*  MRF Series Board Codes                                                                        */
/**************************************************************************************************/

#define MRF_SERIES_200       0x000000C8        /* Series 200 Code (in Hex)                        */
#define MRF_SERIES_220       0x000000DC        /* Series 220 Code (in Hex)                        */
#define MRF_SERIES_230       0x000000E6        /* Series 230 Code (in Hex)                        */


/**************************************************************************************************/
/*  Site-Specific Configuration Parameters                                                        */
/*  (these parameters take their values from the MRF_CONFIG_SITE* files)                          */
/**************************************************************************************************/

/*=====================
 * Default Event Clock Frequency (in MegaHertz)
 */
#ifdef EVENT_CLOCK_FREQ
    #define EVENT_CLOCK_DEFAULT    EVENT_CLOCK_FREQ     /* Use site-selected event clock speed    */
#else
    #define EVENT_CLOCK_DEFAULT    0.00                 /* Defaults to cntrl word value or 125.0  */
#endif

/**************************************************************************************************/
/*  Special Macros to Document Global Vs Local Routines and Data                                  */
/*  (note that these values can be overridden in the invoking module)                             */
/**************************************************************************************************/

/*---------------------
 * Globally accessible routines
 */
#ifndef GLOBAL_RTN
#define GLOBAL_RTN
#endif

/*---------------------
 * Routines that are normally only locally accessible
 */
#ifndef LOCAL_RTN
#define LOCAL_RTN static
#endif

/*---------------------
 * Data that is normally only locally accessible
 */
#ifndef LOCAL
#define LOCAL static
#endif

/**************************************************************************************************/
/*  Special Macros to Define Symbolic Success/Error Return Codes                                  */
/*  (note that these values can be overridden in the invoking module)                             */
/**************************************************************************************************/

#ifndef OK
#define OK     (0)
#endif

#ifndef ERROR
#define ERROR  (-1)
#endif

#ifndef NULL
#define NULL   ('\0')
#endif

#endif
