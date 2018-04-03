/*******************************************************************************

 File:        evbuilder.h

 Description: Definitions of EventBuilder object.


 Author:      D. Calvet,        calvet@hep.saclay.cea.fr
              

 History:
   January 2012 : created

*******************************************************************************/

#ifndef EVENTBUILDER_H
#define EVENTBUILDER_H

#include "platform_spec.h"
#include "os_al.h"
#include <stdio.h>

/*******************************************************************************
 Constants types and global variables
*******************************************************************************/

#define MAX_NB_OF_SOURCES    32
#define MAX_QUEUE_SIZE      256

typedef struct _EventBuilder {
	int           id;
	ThreadStruct  thread;
	int           state;

	void *sem_wakeup; // Semaphore to wake-up event builder
	void *q_mutex; // Mutex for protecting access to event builder queues

	void *q_buf_i[MAX_NB_OF_SOURCES][MAX_QUEUE_SIZE]; // Array of Queues of buffer to process by event builder
	int q_buf_i_rd[MAX_NB_OF_SOURCES]; // read pointer
	int q_buf_i_wr[MAX_NB_OF_SOURCES]; // write pointer
	int q_buf_i_sz[MAX_NB_OF_SOURCES]; // queue size

	void *q_buf_o[MAX_NB_OF_SOURCES*MAX_QUEUE_SIZE]; // Single queue of buffer released by event builder
	int q_src_o[MAX_NB_OF_SOURCES*MAX_QUEUE_SIZE]; // Source of each buffer in the queue
	int q_buf_o_rd; // read pointer
	int q_buf_o_wr; // write pointer
	int q_buf_o_sz; // queue size

	unsigned int vflags; // verboseness flags for event data printout

	void *fa; // pointer to FEM Array

	char file_path[120]; // result file path
	int savedata; // 0: do not save data; 1: save to disk in ASCII; 2: save to disk in binary
	FILE *fout; // output file pointer

	unsigned int file_max_size; // maximum number of bytes per file
	unsigned int byte_wr; // number of bytes written to file

	int eb_mode;  // event builder mode transparent or active
	int had_sobe; // current event had Start Of Built Event emitted
	unsigned int pnd_src; // pending source pattern for completion of the current event

	unsigned int src_had_soe;  // indicates the set of sources that had Start Of Event found for this event
	unsigned short cur_ev_ty;  // event type of event under re-assembly
	unsigned int cur_ev_nb;    // event number of event under re-assembly
	unsigned short cur_ev_tsl; // time stamp low of event under re-assembly
	unsigned short cur_ev_tsm; // time stamp medium of event under re-assembly
	unsigned short cur_ev_tsh; // time stamp high of event under re-assembly

	char   run_str[300]; // run identifier string
	int    subrun_ix;   // index of current sub-run  
} EventBuilder;

typedef enum _EBFileActions {
	EBFA_OpenFirst,
	EBFA_CloseLast,
	EBFA_CloseCurrentOpenNext
} EBFileActions;

/*******************************************************************************
 Function prototypes
*******************************************************************************/
void EventBuilder_Clear(EventBuilder *eb);
int EventBuilder_Open(EventBuilder *eb);
void EventBuilder_Close(EventBuilder *eb);
int EventBuilder_Flush(EventBuilder *eb);
int EventBuilder_Loop(EventBuilder *eb);
int EventBuilder_PutBufferToProcess(EventBuilder *eb, void *bufi, int src);
int EventBuilder_GetBufferToRecycle(EventBuilder *eb, void* *bufo, int *src);
int EventBuilder_FileAction(EventBuilder *eb, EBFileActions action, int format);

#endif
