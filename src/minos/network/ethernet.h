/*******************************************************************************
                           Minos - Feminos/TCM
                           ___________________

 File:        ethernet.h

 Description: Definition of interface for communication over Ethernet.

 Author:      D. Calvet,        calvet@hep.saclay.cea.fr


 History:
  April 2006: created
  April 2013: original file sockudp.h renamed in ethernet.h

*******************************************************************************/
#ifndef ETHERNET_H
#define ETHERNET_H

#include "ethernet_ps.h"

int Ethernet_Open(Ethernet* et, EthernetParam* etp);
int Ethernet_Close(Ethernet* et);
void Ethernet_StatShow(Ethernet* et);
void Ethernet_StatClear(Ethernet* et);

#endif // ETHERNET_H
