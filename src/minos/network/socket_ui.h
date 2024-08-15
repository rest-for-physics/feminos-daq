/*******************************************************************************
                           Minos - Feminos/TCM
                           ___________________

 File:        socket_ui.h

 Description: Definition of simplified socket interface. User interface side

 Author:      D. Calvet,        calvet@hep.saclay.cea.fr


 History:
  April 2013: created from code extracted in ethernet.h

*******************************************************************************/
#ifndef SOCKET_UI_H
#define SOCKET_UI_H

#include "ethernet.h"

int Socket_Open(Ethernet* et, unsigned short port, unsigned char proto, SocketState state);
int Socket_Close(Ethernet* et, int sock);
int Socket_Recv(Ethernet* et, int* from, void** data, short* sz);
int Socket_Send(Ethernet* et, int to, void* data, short* sz);

#endif // SOCKET_UI_H
