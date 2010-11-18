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

#include  <epicsVersion.h>      /* EPICS Version definition                                       */
#include  <epicsTypes.h>        /* EPICS Architecture-independent type definitions                */
#include  <epicsTime.h>         /* EPICS Time definitions                                         */
#include  <epicsMath.h>         /* EPICS Common math functions & definitions                      */

#include  <alarm.h>             /* EPICS Alarm status and severity definitions                    */
#include  <dbAccess.h>          /* EPICS Database Access definitions                              */
#include  <dbCommon.h>          /* EPICS Common record field definitions                          */
#include  <devSup.h>            /* EPICS Device support messages and definitions                  */
#include  <menuYesNo.h>         /* EPICS Yes/No record-support menu                               */

#include  <debugPrint.h>        /* SLAC Debug print utility                                       */


/**************************************************************************************************/
/*  MRF Event System Constants                                                                    */
/**************************************************************************************************/

#define MRF_NUM_EVENTS              256        /* Number of possible events                       */
#define MRF_EVENT_FIFO_SIZE         512        /* Size of EVR/EVG event FIFO                      */
#define MRF_MAX_DATA_BUFFER        2048        /* Maximum size of the distributed data buffer     */
#define MRF_FRAC_SYNTH_REF         24.0        /* Fractional Synth reference frequency (MHz).     */
#define MRF_DEF_CLOCK_SPEED       125.0        /* Default event clock speed is 125 MHz.           */
#define MRF_SN_BYTES                  6        /* Number of bytes in serial number                */
#define MRF_SN_STRING_SIZE           18        /* Size of serial number string (including NULL)   */
#define MRF_DESCRIPTION_SIZE         80        /* Size of description text string (inclucing NULL)*/

/**************************************************************************************************/
/*  MRF Supported Bus Types                                                                       */
/**************************************************************************************************/

#define MRF_BUS_COMPACT_PCI           0        /* 0 = Compact PCI (3U)                            */
#define MRF_BUS_PMC                   1        /* 1 = PMC                                         */
#define MRF_BUS_VME                   2        /* 2 = VME 64x                                     */


/**************************************************************************************************/
/*  MRF Board Types                                                                               */
/**************************************************************************************************/

#define MRF_CARD_TYPE_EVR             1        /* 1 = Event Receiver                              */
#define MRF_CARD_TYPE_EVG             2        /* 2 = Event Generator                             */


/**************************************************************************************************/
/*  MRF Board Series Codes                                                                        */
/**************************************************************************************************/

#define MRF_SERIES_200       0x000000C8        /* Series 200 Code (in Hex)                        */
#define MRF_SERIES_220       0x000000DC        /* Series 220 Code (in Hex)                        */
#define MRF_SERIES_230       0x000000E6        /* Series 230 Code (in Hex)                        */


/**************************************************************************************************/
/*  Site-Specific Defaults                                                                        */
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
/*  Function Prototype Definitions                                                                */
/**************************************************************************************************/

#ifdef __cplusplus

template<class Mutex>
class scopedLock
{
    Mutex& m;
    bool locked;
public:
    scopedLock(Mutex& mutex, bool lock=true) : m(mutex), locked(lock)
    {
        if (lock) m.lock();
    }
    ~scopedLock()
    {
        unlock();
    }
    inline void lock(){if (!locked) m.lock();locked=true;}
    inline void unlock(){if (locked) m.unlock();locked=false;}
};
#define SCOPED_LOCK2(m, name) scopedLock<epicsMutex> name(m)
#define SCOPED_LOCK(m) SCOPED_LOCK2(m, m##_guard)

/***************************************************************************************************
 * mrfDisableRecord () -- Disable a Record From Ever Being Processed
 **************************************************************************************************/
 /**
 * @par Description:
 *   Renders an EPICS record incapable of ever being processed.
 *
 * @par Function:
 * - Set the "Processing Active" (PACT) field to "true"
 * - Set the "Disable putFields" (DISP) field to "true"
 * - Set the "Disable Value" (DISV) equal to the "Disable Link Value" (DISA)
 * - Set the record status field (STAT) to "DISABLE_ALARM"
 * - Set the record severity field (SEVR) to "INVALID_ALARM"
 *
 * @param   pRec = (input) Pointer to the record to be disabled.
 *
 **************************************************************************************************/
inline void mrfDisableRecord (dbCommon *pRec)
{
    pRec->pact = pRec->disp = 1;
    pRec->disv = pRec->disa;
    pRec->stat = DISABLE_ALARM;
    pRec->sevr = pRec->diss = INVALID_ALARM;

}

#endif /* __cplusplus */

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------
 * Define the prototype for an EPICS Interrupt Service Routine
 * (for some reason, devLib.h neglects to define this for us).
 */
typedef void (*EPICS_ISR_FUNC) (void *);

#ifdef __cplusplus
}
#endif

/**************************************************************************************************/
/*  Device Support Entry Table Type Definitions                                                   */
/**************************************************************************************************/

/*=====================
 * Device Support Entry Table (DSET) for analog input and analog output records
 */
#define DSET_ANALOG_NUM   6      /* Number of entries in an analog I/O DSET                       */
struct AnalogDSET {
    long	number;	         /* Number of support routines                                    */
    DEVSUPFUN	report;		 /* Report routine                                                */
    DEVSUPFUN	init;	         /* Device suppport initialization routine                        */
    DEVSUPFUN	init_record;     /* Record initialization routine                                 */
    DEVSUPFUN	get_ioint_info;  /* Get io interrupt information                                  */
    DEVSUPFUN   perform_io;      /* Read or Write routine                                         */
    DEVSUPFUN   special_linconv; /* Special linear-conversion routine                             */
};/*end AnalogDSET*/

/*=====================
 * Device Support Entry Table (DSET) for binary input and binary output records
 */
#define DSET_BINARY_NUM   5      /* Number of entries in a binary I/O DSET                        */
struct BinaryDSET {
    long	number;	         /* Number of support routines                                    */
    DEVSUPFUN	report;		 /* Report routine                                                */
    DEVSUPFUN	init;	         /* Device suppport initialization routine                        */
    DEVSUPFUN	init_record;     /* Record initialization routine                                 */
    DEVSUPFUN	get_ioint_info;  /* Get io interrupt information                                  */
    DEVSUPFUN   perform_io;      /* Read or Write routine                                         */
};/*end BinaryDSET*/

/*=====================
 * Device Support Entry Table (DSET) for long input and long output records
 */
#define DSET_LONG_NUM     5      /* Number of entries in a loing I/O DSET                         */
struct LongDSET {
    long	number;	         /* Number of support routines                                    */
    DEVSUPFUN	report;		 /* Report routine                                                */
    DEVSUPFUN	init;	         /* Device suppport initialization routine                        */
    DEVSUPFUN	init_record;     /* Record initialization routine                                 */
    DEVSUPFUN	get_ioint_info;  /* Get io interrupt information                                  */
    DEVSUPFUN   perform_io;      /* Read or Write routine                                         */
};/*end LongDSET*/

/*=====================
 * Device Support Entry Table (DSET) for multi-bit binary input and output records
 */
#define DSET_MBB_NUM      5      /* Number of entries in a multi-bit binary I/O DSET              */
struct MbbDSET {
    long	number;	         /* Number of support routines                                    */
    DEVSUPFUN	report;		 /* Report routine                                                */
    DEVSUPFUN	init;	         /* Device suppport initialization routine                        */
    DEVSUPFUN	init_record;     /* Record initialization routine                                 */
    DEVSUPFUN	get_ioint_info;  /* Get io interrupt information                                  */
    DEVSUPFUN   perform_io;      /* Read or Write routine                                         */
};/*end MbbDSET*/


/**************************************************************************************************/
/*  Special Macros to Define Commonly Used Symbols                                                */
/*  (note that these values can be overridden in the invoking module)                             */
/**************************************************************************************************/


/*---------------------
 * Success return code
 */
#ifndef OK
#define OK     (0)
#endif

/*---------------------
 * Failure return code
 */
#ifndef ERROR
#define ERROR  (-1)
#endif

/*---------------------
 * Success, but do not perform linear conversions (ai & ao record device support routines)
 */
#ifndef NO_CONVERT
#define NO_CONVERT (2)
#endif

/**************************************************************************************************/
/*  Special Macros to Make the EPICS "menuYesNo" symbols easier to read                           */
/**************************************************************************************************/

#define menuNO    menuYesNoNO
#define menuYES   menuYesNoYES

/**************************************************************************************************/
/*  Definitions for Compatibiliby with Older Versions of EPICS                                    */
/**************************************************************************************************/

/*---------------------
 * Older versions (< 3.14.9) of recGblRecordError took a non-const string
 */
#if EPICS_VERSION==3 && EPICS_REVISION==14 && EPICS_MODIFICATION<9
#  define recGblRecordError(ERR, REC, STR) recGblRecordError(ERR, REC, (char*)(STR))
#endif

/*---------------------
 * Older versions (< 3.14.10) do not define POSIX_TIME
 */
#ifndef POSIX_TIME_AT_EPICS_EPOCH
#  define POSIX_TIME_AT_EPICS_EPOCH 631152000u
#endif

/*---------------------
 * Older versions (< 3.14.10) use DBE_LOG instead of DBE_ARCHIVE
 */
#ifndef DBE_ARCHIVE
#  define DBE_ARCHIVE DBE_LOG
#endif

/*---------------------
 * epicsMath.h defines "finite()" for vxWorks, but "isfinite()" is the standard.
 * finite() does not appear to be supported in epicsMath.h for all architectures.
 */
#ifndef isfinite
#  define isfinite finite
#endif


#endif
