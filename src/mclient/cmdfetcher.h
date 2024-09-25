/*******************************************************************************

 File:        cmdfetcher.h

 Description: definition of CmdFetcher


 Author:      D. Calvet,        calvet@hep.saclay.cea.fr


 History:
   December 2011 : created

*******************************************************************************/

#ifndef CMDFETCHER_H
#define CMDFETCHER_H

#include "os_al.h"
#include "platform_spec.h"

/*******************************************************************************
 Constants types and global variables
*******************************************************************************/
#define CMD_ARRAY_SIZE 25000
#define CMD_LINE_SIZE 200

typedef struct _CmdFetcher {
    int id;
    ThreadStruct thread;
    int state;

    int use_stdin;
    char cmd_file[80];
    char snd[CMD_ARRAY_SIZE][CMD_LINE_SIZE];
    int cmd_cnt;
    int verbose;
    int no_echo_this;
    int cmd_index;

    unsigned int cur_single_fem;
    unsigned int cur_multi_fem;
    int cur_fem_cnt;
    unsigned int fem_ix_min;
    unsigned int fem_ix_max;

    void* fa; // pointer to FEM Array
    void* eb; // pointer to Event Builder

    void* sem_cur_cmd_done;

} CmdFetcher;

/* Function prototypes */
void CmdFetcher_Init(CmdFetcher* cf);

int CmdFetcher_Main(CmdFetcher* cf);

int CmdFetcher_ParseCmdFile(CmdFetcher* cf);

#endif
