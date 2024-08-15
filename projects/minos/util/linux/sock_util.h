/*******************************************************************************
                           T2K 280 m TPC readout
                           _____________________

 File:        sock_util.h

 Description: several socket utility functions in Linux flavour.


 Author:      D. Calvet,        calvet@hep.saclay.cea.fr


 History:
  February 2007: created

*******************************************************************************/
#ifndef _SOCK_UTIL_H
#define _SOCK_UTIL_H

#define socket_init() 0
#define socket_cleanup() 0
#define socket_get_error() errno
#define closesocket close
#define ioctlsocket ioctl

#endif
