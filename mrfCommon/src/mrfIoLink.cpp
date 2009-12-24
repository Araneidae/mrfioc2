/***************************************************************************************************
|* mrfIoLink.cpp -- Class Definition File for the MRF I/O Link Parser Class
|*
|*--------------------------------------------------------------------------------------------------
|* Authors:  E.Bjorklund (LANSCE)
|* Date:     24 December 2009
|*
|*--------------------------------------------------------------------------------------------------
|* MODIFICATION HISTORY:
|* 24 Dec 2009  E.Bjorklund     Original.
|*
|*--------------------------------------------------------------------------------------------------
|* MODULE DESCRIPTION:
|*   This header file contains the class declarations for the mrfIoLink class.
|*   It parses the contents of the INP or OUT fields and returns the parameter values via
|*   "getter" functions.
|*
|*--------------------------------------------------------------------------------------------------
|* HARDWARE SUPPORTED:
|*   Series 2xx Event Generator and Event Receiver Cards
|*     APS Register Mask
|*     Modular Register Mask
|*
|*--------------------------------------------------------------------------------------------------
|* OPERATING SYSTEMS SUPPORTED:
|*   Any
|*
\**************************************************************************************************/

/***************************************************************************************************
|*                           COPYRIGHT NOTIFICATION
|***************************************************************************************************
|*
|* THE FOLLOWING IS A NOTICE OF COPYRIGHT, AVAILABILITY OF THE CODE,
|* AND DISCLAIMER WHICH MUST BE INCLUDED IN THE PROLOGUE OF THE CODE
|* AND IN ALL SOURCE LISTINGS OF THE CODE.
|*
|***************************************************************************************************
|*
|* This software is distributed under the EPICS Open License Agreement which
|* can be found in the file, LICENSE, included with this distribution.
|*
\**************************************************************************************************/

//==================================================================================================
//  mrfCommon Group Definition
//==================================================================================================
//! @defgroup   mrfCommon Common Utility Routines
//! @brief      Utility routines used by the MRF timing software.
//!
//! The common utility routines and definitions are found in the $(TIMING)/mrfCommon directory.
//! The utilities in this directory include:
//! - \b  mrfFracSynth - Routines for translating between event clock frequency and
//!                      fractional synthesizer control words
//! - \b  mrfIoLink    - Parses the parameter list specified in the INP or OUT link fields.
//! - \b  mrfIoOps     - Macros for performing register-based I/O operations
//!
//! @{
//!
//==================================================================================================

//==================================================================================================
// mrfIoLink Class Description
//==================================================================================================
//! @class      mrfIoLink
//! @brief      I/O link field parser
//!
//! @par Description:
//!   The MRF timing system record interface uses the INST_IO link type.
//!
//==================================================================================================

/**************************************************************************************************/
/*  Imported Header Files                                                                         */
/**************************************************************************************************/

#include <errno.h>              // Standard C error variable

#include <stdexcept>            // Standard C++ exception class
#include <string>               // Standard C++ string class
#include <map>                  // Standard C++ map template

#include <epicsTypes.h>         // EPICS Architecture-independent type definitions
#include <mrfCommon.h>          // MRF Common Definitions
#include <mrfIoLink.h>          // MRF I/O Link class definition

#define PARM_NOT_FOUND  (NumLegalNames + 1)

/**************************************************************************************************/
/*                              Class Member Function Definitions                                 */
/*                                                                                                */

//**************************************************************************************************
//  mrfIoLink () -- Class Constructor
//**************************************************************************************************
//! @par Description:
//!   Class Constructor
//!
//! @par Function:
//!   Creates an mrfIoLink object from the parameter string from the INP or OUT field and
//!   a table of valid parameter names.  The parameter string is parsed and stored in an
//!   internal structure that maps the parameter names it found with their values.  The
//!   name and value pairs are available through the class "getter" methods.
//!
//! @param   parmString = (input) The parameter string from the INP or OUT field of the record
//! @param   nameList   = (input) The table of legal parameter names for this record
//! @param   numNames   = (input) The number of entries in the legal name table
//!
//! @throw   runtime_error is thrown if there was an error parsing the parameter string
//!
//**************************************************************************************************

mrfIoLink::mrfIoLink (

    //=====================
    // Argument declarations
    //
    const char*             parmString,         // Parameter string from the I/O link field
    const mrfParmNameList   nameList,           // Table of valid parameter names
    const epicsInt32        numNames) :         // Number of entries in the table of parameter names

    //=====================
    // Initialize the data members
    // 
    LegalNames(nameList),                       // Table of legal parameter names
    NumLegalNames(numNames)                     // Number of entries in the parameter name parameter
{
    //=====================
    // Local variables
    //
    std::string::size_type     listSize;        // Length of the parameter string
    std::string                parmList;        // Parameter string from the I/O link
    std::string::size_type     parmLength;      // Length of the parameter/value pair
    std::string::size_type     startIndex;      // Marks the start of a parameter/value pair
    std::string::size_type     stopIndex = 0;   // Marks the end of a parameter/value pair


    //=====================
    // Convert the parameter string to a string class in order to expedite parsing
    //
    parmList = parmString;
    listSize = parmList.length();
    if (0 == listSize) return;

    //=====================
    // Loop to process each parameter/value pair in the parameter list
    //
    for (startIndex = 0;   startIndex < listSize;  startIndex = stopIndex + 1) {

        //=====================
        // Extract the next parameter/value pair form the parameter list
        //
        stopIndex = parmList.find(';', startIndex);
        if (std::string::npos == stopIndex) stopIndex = listSize;
        parmLength = stopIndex - startIndex;

        if (0 == parmLength) continue;

        //=====================
        // Parse the parameter/value pair
        //
        parseParameter (parmList.substr(startIndex, parmLength));
        startIndex = stopIndex + 1;

    }//end for each parameter in the string

}//end constructor

//**************************************************************************************************
//  has_a () -- Test To See If A Parameter Exists
//**************************************************************************************************
//! @par Description:
//!   Test to see if a particular parameter was specified in the I/O link field.
//!   This method can be used to implement optional parameters, since it does not
//!   throw an excepion if the parameter was not specified.
//!
//! @param      parm  = (input) The name of the parameter we wish to test.
//!
//! @return     Returns "true" if the parameter was specified in the I/O link field<br>
//!             Returns "fales" if the parameter was not specified in the I/O link field
//!
//! @par Member Variables Referenced:
//! - \e        ParmMap = (input) Maps parameter names to values.
//!
//**************************************************************************************************

bool
mrfIoLink::has_a (const char* parm) {

    //=====================
    // Look up the parameter name/value pair
    //
    std::string  parmName(parm);
    mrfParmMap::iterator parmIndex = ParmMap.find(parmName);

    //=====================
    // Return true if we found the parameter
    // Return false if we did not find the parameter
    //
    return (ParmMap.end() != parmIndex);

}//end has_a()

//**************************************************************************************************
//  getString () -- Return Parameter Value As A String
//**************************************************************************************************
//! @par Description:
//!   Return the value of the requested parameter as a string.
//!
//! @param      parm  = (input) The name of the parameter we wish to get the value of
//!
//! @return     Returns the parameter value as a C++ string
//!
//! @throw      runtime_error is thrown if the requested parameter was not in the I/O link string
//!
//! @par Member Variables Referenced:
//! - \e        ParmMap = (input) Maps parameter names to values.
//!
//**************************************************************************************************

std::string&
mrfIoLink::getString (const char* parm) {

    //=====================
    // Look up the parameter name/value pair
    //
    std::string parmName(parm);
    mrfParmMap::iterator parmIndex = ParmMap.find(parmName);

    //=====================
    // Abort if the requested parameter was not specified in the I/O link
    //
    if (ParmMap.end() == parmIndex) {
        throw std::runtime_error (
            "I/O link parameter \""
            + parmName
            + "\" was not present in the I/O link field for this record.");
    }//end if parameter name not present

    //=====================
    // Return the value string
    //
    return (parmIndex->second);

}//end getString()

//**************************************************************************************************
//  getInteger () -- Return Parameter Value As An Integer
//**************************************************************************************************
//! @par Description:
//!   Return the value of the requested parameter as a signed integer.
//!
//! @par Function:
//!   Retrieves the string value for the specified parameter and checks to see if it can
//!   be parsed as a signed integer.  If the value begins with "0x" or "0X", it will be
//!   interpreted as a hexadecimal number.  Otherwise it will be interpreted as decimal.
//!
//! @param      parm  = (input) The name of the parameter we wish to get the value of
//!
//! @return     Returns the parameter value as a signed integer
//!
//! @throw      runtime_error is thrown if the requested parameter was not in the I/O link string
//!             or the value did not represent a valid signed integer.
//!
//! @par Member Variables Referenced:
//! - \e        ParmMap = (input) Maps parameter names to values.
//!
//**************************************************************************************************

epicsInt32
mrfIoLink::getInteger (const char* parm) {

    //=====================
    // Local variables
    //
    epicsInt32               base;              // Numeric base
    char*                    pTail;             // Tail pointer used by "strtol()"
    std::string::size_type   startIndex = 0;    // Index to start of numeric digits
    epicsStatus              status = OK;       // Local status variable
    const std::string*       validChars;        // Pointer to valid character set
    epicsInt32               value;             // Numeric value holder
    std::string              valueString;       // Parameter's value string

    //=====================
    // Define valid character sets for each numeric base we support
    //
    static const std::string   decimalChars ("0123456789");
    static const std::string   hexChars     ("0123456789abcdefABCDEF");

    //=====================
    // Retrieve the value string for the specified parameter name
    // Start with assumption that the value is base ten.
    //
    valueString = getString(parm);
    base = 10;
    validChars = &decimalChars;

    //=====================
    // Check for leading sign character
    //
    if (('+' == valueString[startIndex]) || ('-' == valueString[startIndex]))
        startIndex++;

    //=====================
    // Check for a base indicator
    //
    if ('0' == valueString[startIndex]) {
        startIndex++;

        //=====================
        // Check for hexadecimal number
        //
        if (('x' == valueString[startIndex]) || ('X' == valueString[startIndex])) {
            startIndex++;
            base = 16;
            validChars = &hexChars;
        }//end if hexadecimal number

    }//end if number base was specified

    //=====================
    // Make sure the value string can be converted to an integer of the specified base
    //
    if (std::string::npos != valueString.find_first_not_of(*validChars,startIndex))
        status = ERROR;

    //=====================
    // Try to convert the value string to an integer
    //
    else {
        errno = 0;
        value = strtol(valueString.c_str(), &pTail, base);
        if (0 != errno) status = ERROR;
    }//end if value string has no invalid characters

    //=====================
    // Abort if the value string could not be converted to an integer
    //
    if (OK != status) {
        throw std::runtime_error (
            "I/O link parameter \""
             + std::string(parm)
             + "\" value ("
             + valueString 
             + ") is not a valid signed integer.");
    }//end if value string was not a valid integer

    //=====================
    // Return the converted value
    //
    return value;

}//end getInteger()

//**************************************************************************************************
//  mrfIoLink () -- Class Destructor
//**************************************************************************************************
//! @par Description:
//!   Class Destructor
//!
//! @par Function:
//!   No additional action is taken when the object is destroyed.
//!
//**************************************************************************************************

mrfIoLink::~mrfIoLink() {}

/**************************************************************************************************/
/*                                    Private Methods                                             */
/*                                                                                                */


/**************************************************************************************************
|* parseParmeter () -- Parse a Single Parameter Name/Value Pair
|*-------------------------------------------------------------------------------------------------
|* FUNCTION:
|*    o Separate the name/value pair into its parts.
|*    o Delete leading and trailing blanks from each part.
|*    o Check for illegal or duplicate parameter names and missing values.
|*    o Add the Name/Value pair to the parameter map
|*
|*-------------------------------------------------------------------------------------------------
|* CALLING SEQUENCE:
|*    parseParameter (parmString)
|*
|*-------------------------------------------------------------------------------------------------
|* INPUT PARAMETERS:
|*    parmString  = (string &)  Parameter/Value pair to parse
|*
|*-------------------------------------------------------------------------------------------------
|* IMPLICIT OUTPUTS (member variables):
|*    ParmMap     = (map) Maps parameter names to values.
|*
|*-------------------------------------------------------------------------------------------------
|* THROWS:
|*    A runtime_error is thrown if there was an error parsing the Name/Value pair
|*
\**************************************************************************************************/
void
mrfIoLink::parseParameter (const std::string& parmString) {

    //=====================
    // Local variables
    //
    static const std::string  spaceChars (" \t");   // List of blank space characters

    epicsInt32                nameIndex;            // Parameter's table index
    std::string::size_type    parmLength;           // Length of the parameter name string
    std::string::size_type    startIndex;           // First non-blank char in the parm string
    std::string::size_type    stopIndex;            // Last non-blank char in the parm string
    std::string::size_type    valueIndex;           // Start of parameter value substromg
    std::string::size_type    valueLength;          // Length of parameter value substring

    //=====================
    // Locate the start of the parameter name (first non-blank character)
    //
    startIndex = parmString.find_first_not_of(spaceChars);
    if (std::string::npos == startIndex)
        return;

    //=====================
    // Locate the "=" delimeter between the parameter name and the parameter value
    //
    valueIndex = parmString.find_first_of('=',startIndex);
    if (std::string::npos == valueIndex) {
        throw std::runtime_error (
            "Missing \"=\" between I/O link parameter name and value ("
            + parmString.substr(startIndex)
            + ")." );
    }//end if "=" delimeter was not found

    //=====================
    // Now locate the end of the parameter name
    //
    stopIndex = parmString.find_first_of(spaceChars, startIndex);
    if ((std::string::npos == stopIndex) || (stopIndex > valueIndex))
        stopIndex = valueIndex;

    parmLength = stopIndex - startIndex;

    //=====================
    // Look up the parameter in the parameter table
    //
    nameIndex = PARM_NOT_FOUND;
    for (epicsInt32 i = 0;   i < NumLegalNames;  i++) {
        if (LegalNames[i].length() == parmLength) {
            if (LegalNames[i] == parmString.substr(startIndex, parmLength)) {
                nameIndex = i;
                break;
            }//end if table lookup succeeded
        }//end if sizes match
    }//end for each defined parameter

    //=====================
    // Abort if we could not find the parameter name in the name table
    //
    if (PARM_NOT_FOUND == nameIndex) {
        throw std::runtime_error("\"" 
            + parmString.substr(startIndex,parmLength)
            + "\" is not a legal I/O link parameter name for this record");
    }//end if parameter name was not found

    //=====================
    // Abort if the same parameter name occurs more than once in the I/O link string
    //
    mrfParmMap::iterator parm = ParmMap.find(LegalNames[nameIndex]);
    if (parm != ParmMap.end()) {
        throw std::runtime_error(
            "Duplicate I/O link parameter specified ("
            + parmString.substr(startIndex,parmLength)
            + ")." );
    }//end if the parameter name occured more than once in the link string
        
    //=====================
    // Locate the start of the parameter value
    // Trim off leading blanks
    // Abort if the parameter has no value
    //
    valueIndex = parmString.find_first_not_of(spaceChars, valueIndex+1);
    if (std::string::npos == valueIndex) {
        throw std::runtime_error("I/O link parameter \""
            + parmString.substr(startIndex,parmLength)
            + "\" has no value.");
    }//end if parameter value was not found

    //=====================
    // Trim trailing blanks from the parameter value
    //
    stopIndex = parmString.find_last_not_of(spaceChars);
    valueLength = (stopIndex + 1) - valueIndex;

    //=====================
    // Map the parameter name to its value string
    //
    ParmMap[LegalNames[nameIndex]] = parmString.substr(valueIndex, valueLength);

}//end parseParameter()

//!
//| @}
//end group mrfCommon
