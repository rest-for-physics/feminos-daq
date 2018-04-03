/*******************************************************************************
                           T2K 280 m TPC readout
                           _____________________

 File:        platform_spec.h

 Description: Specific include file and definitions for Windows PC

 Author:      D. Calvet,        calvet@hep.saclay.cea.fr
              

 History:
  March 2006: created

*******************************************************************************/
#ifndef PLATFORM_SPEC_H
#define PLATFORM_SPEC_H

#include <windows.h>
#include <winsock.h>

#define xil_printf printf
#define yield() Sleep(0)

#endif