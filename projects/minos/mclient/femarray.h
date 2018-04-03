/*******************************************************************************

 File:        femarray.h

 Description: Definitions for an array of proxy of Feminos cards.


 Author:      D. Calvet,        calvet@hep.saclay.cea.fr
              

 History:
   December 2011 : created

*******************************************************************************/

#ifndef FEMARRAY_H
#define FEMARRAY_H

#include "os_al.h"
#include "femproxy.h"
#include <timerlib.h>
#include <stdio.h>

/*******************************************************************************
 Constants types and global variables
*******************************************************************************/

#define REMOTE_DST_PORT            1122

#define MAX_NUMBER_OF_FEMINOS        32

typedef struct _FemArray {
	int           id;
	ThreadStruct  thread;
	int           state;

	int rem_ip_beg[4];
	int rem_port;

	int loc_ip[4];

	unsigned int fem_proxy_set;
	FemProxy fp[MAX_NUMBER_OF_FEMINOS];

	void *sem_cur_cmd_done;
	void *snd_mutex;
	int verbose;

	int pending_rep_cnt;

	int is_list_fr_pnd;
	int is_first_fr;
	int list_fr_cnt;

	int daq_infinite;
	__int64 daq_size_left; // amount of data still to be received to complete the current DAQ phase
	__int64  daq_size_rcv; // amount of data already received for the current DAQ phase
	__int64  daq_size_lst; // amount of data collected at the last time 
	struct timeval daq_last_time; // last time when the previous field was updated

	int req_threshold; // Minimum number of credits to send a new request
	char cred_unit;    // Credit units, B: Bytes  F: Frames
	
	int drop_a_credit; // flag set to 1 to inject a fault by dropping a credit frame
	int delay_a_credit; // Set to non zero to delay a credit frame by the amount specified

	void  *bp; // Pointer to Buffer Pool
	void  *eb; // Pointer to Event Builder

	FILE *pedthr; // Pointer to File for storing pedestal or thresholds

} FemArray;

/*******************************************************************************
 Function prototypes
*******************************************************************************/
void FemArray_Clear(FemArray *fa);
int FemArray_Open(FemArray *fa);
void FemArray_Close(FemArray *fa);
int FemArray_SendCommand(FemArray *fa, unsigned int fem_beg, unsigned int fem_end, unsigned int fem_pat, char *cmd);
int FemArray_SendDaq(FemArray *fa, unsigned int fem_beg, unsigned int fem_end, unsigned int fem_pat, char *cmd);
int FemArray_ReceiveLoop(FemArray *fa);

#endif
