/*******************************************************************************
                           T2K 280 m TPC readout
                           _____________________

 File:        sock_util.h

 Description: several socket utility functions in Windows flavour.


 Author:      D. Calvet,        calvet@hep.saclay.cea.fr


 History:
  February 2007: created

*******************************************************************************/
#ifndef _SOCK_UTIL_H
#define _SOCK_UTIL_H

#define EWOULDBLOCK WSAEWOULDBLOCK

int socket_init();
int socket_cleanup();
int socket_get_error();

#endif
