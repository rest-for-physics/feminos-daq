/*******************************************************************************
                           T2K 280 m TPC readout
                           _____________________

 File:        sock_util.c

 Description: several socket utility functions in Windows flavour.


 Author:      D. Calvet,        calvet@hep.saclay.cea.fr


 History:
  February 2007: created

*******************************************************************************/
#include "sock_util.h"
#include "platform_spec.h"

/*******************************************************************************
 socket_init()
*******************************************************************************/
int socket_init() {
    WORD sockVersion;
    WSADATA wsaData;
    int err;

    // Initialize socket
    sockVersion = MAKEWORD(2, 0);
    if ((err = WSAStartup(sockVersion, &wsaData)) != 0) {
        err = WSAGetLastError();
    }
    return (err);
}

/*******************************************************************************
 socket_cleanup()
*******************************************************************************/
int socket_cleanup() {
    return (WSACleanup());
}

/*******************************************************************************
 socket_cleanup()
*******************************************************************************/
int socket_get_error() {
    return (WSAGetLastError());
}
