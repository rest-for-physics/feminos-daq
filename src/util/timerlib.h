/*
----------------------------------------------------------------------------------
-- Company:        DAPNIA, CEA Saclay
-- Engineer:       Irakli MANDJAVIDZE
--
-- Design Name:    Selective Read-out Processor (SRP)
-- Module Name:    timerlib.h
-- Project Name:   CMS ECAL Selective Read-out Processor
-- Target Devices: Xilinx Virtex2Pro
-- Tool versions:  Make with EDK/GNU
-- Description:    timer library header file
--
-- Create Date:    1.0 20080123
-- Revision:       1.1 20080124 IM Resolve Linux and SoC timer functions here
--                     20080125 IM define tv_sec  as unsigned long long
--                                      & tv_usec as unsigned int
--                 1.2 20091029 IM Add Windows NT support
--
-- Additional Comments:
--
----------------------------------------------------------------------------------
*/
#ifndef TIMER_LIB_H
#define TIMER_LIB_H

#ifdef LINUX
#include <libgen.h>   //for basename
#include <sys/time.h> //for gettimeofday
#endif

#ifndef LINUX

#ifndef _WINSOCKAPI_
/* Define timeval structure */
struct timeval {
    unsigned long tv_sec; /*      seconds */
    unsigned int tv_usec; /* microseconds */
};
#endif // ifndef _WINSOCKAPI_

/* Define timezone structure */
struct timezone {
    int tz_minuteswest; /* minutes W of Greenwich */
    int tz_dsttime;     /* type of dst correction */
};

/* Define function prototypes */
int gettimeofday(struct timeval* tv, struct timezone* tz);

/* Convenience macros for operations on timevals.
   NOTE: `timercmp' does not work for >= or <=.  */
#ifndef _WINSOCKAPI_
#define timerisset(tvp) ((tvp)->tv_sec || (tvp)->tv_usec)
#define timerclear(tvp) ((tvp)->tv_sec = (tvp)->tv_usec = 0)
#define timercmp(a, b, CMP) \
    (((a)->tv_sec == (b)->tv_sec) ? ((a)->tv_usec CMP(b)->tv_usec) : ((a)->tv_sec CMP(b)->tv_sec))
#define timeradd(a, b, result)                           \
    do {                                                 \
        (result)->tv_sec = (a)->tv_sec + (b)->tv_sec;    \
        (result)->tv_usec = (a)->tv_usec + (b)->tv_usec; \
        if ((result)->tv_usec >= 1000000) {              \
            ++(result)->tv_sec;                          \
            (result)->tv_usec -= 1000000;                \
        }                                                \
    } while (0)
#endif // ifndef _WINSOCKAPI_
#define timersub(a, b, result)                           \
    do {                                                 \
        (result)->tv_sec = (a)->tv_sec - (b)->tv_sec;    \
        (result)->tv_usec = (a)->tv_usec - (b)->tv_usec; \
        if ((result)->tv_usec < 0) {                     \
            --(result)->tv_sec;                          \
            (result)->tv_usec += 1000000;                \
        }                                                \
    } while (0)

#endif // #	ifndef LINUX

#endif // TIMER_LIB_H
