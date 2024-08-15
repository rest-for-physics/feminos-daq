/*******************************************************************************

                           _____________________

 File:        after_strings.h

 Description: Conversion of bit field of the AFTER chip to human readable strings
 electronics.

 Author:      D. Calvet,        calvet@hep.saclay.cea.fr


 History:
  January 2012: adapted from T2K version

  July 2013: added After_Testmode2str array

*******************************************************************************/
#ifndef AFTER_STRINGS_H
#define AFTER_STRINGS_H

/* AFTER Register 1 bit field interpretation  */
static char After_Gain2str[4][6] = {
        "120fC", // 0
        "240fC", // 1
        "360fC", // 2
        "600fC"  // 3
};

static char After_Stime2str[16][8] = {
        "116ns",  // 0
        "200ns",  // 1
        "412ns",  // 2
        "505ns",  // 3
        "610ns",  // 4
        "695ns",  // 5
        "912ns",  // 6
        "993ns",  // 7
        "1054ns", // 8
        "1134ns", // 9
        "1343ns", // 10
        "1421ns", // 11
        "1546ns", // 12
        "1626ns", // 13
        "1834ns", // 14
        "1912ns"  // 15
};

static char After_Testmode2str[4][16] = {
        "nothing",      // 0
        "calibration",  // 1
        "test",         // 2
        "functionality" // 3
};

#endif
