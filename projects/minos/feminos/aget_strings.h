/*******************************************************************************

                           _____________________

 File:        aget_strings.h

 Description: Conversion of bit field of the AGET chip to human readable strings
 electronics.

 Author:      D. Calvet,        calvet@hep.saclay.cea.fr


 History:
  November 2011: adapted from T2K version

  March 2014: added Register 2 bit field interpretation arrays

*******************************************************************************/
#ifndef AGET_STRINGS_H
#define AGET_STRINGS_H

/* AGET Register 1 bit field interpretation  */
static char Aget_Test2str[4][16] = {
        "nothing",      // 0
        "calibration",  // 1
        "test",         // 2
        "functionality" // 3
};

static char Aget_Polarity2str[2][10] = {
        "Negative", // 0
        "Positive"  // 1
};

static char Aget_Vicm2str[4][6] = {
        "1.25V", // 0
        "1.35V", // 1
        "1.55V", // 2
        "1.65V"  // 3
};

static char Aget_Time2str[16][8] = {
        "69.67ns", // 0
        "117.3ns", // 1
        "232.3ns", // 2
        "279.7ns", // 3
        "333.9ns", // 4
        "382.7ns", // 5
        "501.8ns", // 6
        "541.3ns", // 7
        "568.2ns", // 8
        "632.5ns", // 9
        "720.9ns", // 10
        "760.3ns", // 11
        "830.7ns", // 12
        "869.2ns", // 13
        "976.5ns", // 14
        "1014ns"   // 15
};

static char Aget_Dac2str[16][4] = {
        "+0", // 0
        "+1", // 1
        "+2", // 2
        "+3", // 3
        "+4", // 4
        "+5", // 5
        "+6", // 6
        "+7", // 7
        "-1", // 8
        "-2", // 9
        "-3", // 10
        "-4", // 11
        "-5", // 12
        "-6", // 13
        "-7", // 14
        "-8"  // 15
};

static char Aget_Mode2str[2][16] = {
        "hit channels", // 0
        "all channels"  // 1
};

static char Aget_Fpn2str[2][10] = {
        "skip FPN", // 0
        "keep FPN"  // 1
};

static char Aget_TriggerVeto2str[4][16] = {
        "none",          // 0
        "4 us",          // 1
        "hit reg width", // 2
        "undefined"      // 3
};

static char Aget_SynchroDiscri2str[2][10] = {
        "asynch", // 0
        "SCA WCK" // 1
};

static char Aget_Tot2str[2][8] = {
        "no TOT", // 0
        "TOT"     // 1
};

static char Aget_RangeTw2str[2][8] = {
        "100 ns", // 0
        "200 ns"  // 1
};

static char Aget_TrigW2str[4][12] = {
        "94/218 ns",  // 0
        "77/170 ns",  // 1
        "110/235 ns", // 2
        "127/298 ns"  // 3
};

/* AGET Register 2 bit field interpretation  */
static char Aget_CurRa2str[4][12] = {
        "352-292 uA", // 0
        "450-352 uA", // 1
        "637-450 uA", // 2
        "1150-637 uA" // 3
};

static char Aget_CurBuf2str[4][12] = {
        "1.503 mA", // 0
        "1.914 mA", // 1
        "2.700 mA", // 2
        "4.870 mA"  // 3
};

/* AGET Register 6 and 7 bit field interpretation  */
static char Aget_ChannelGain2str[4][8] = {
        "120fC", // 0
        "240fC", // 1
        "1pC",   // 2
        "10pC"   // 3
};

/* AGET Register 10 and 11 bit field interpretation  */
static char Aget_ChannelInhibit2str[4][20] = {
        "chan ON  trig ON ", // 0
        "chan OFF trig ON ", // 1
        "chan ON  trig OFF", // 2
        "chan OFF trig OFF"  // 3
};

#endif
