/*******************************************************************************
                           Minos
                           _____________________

 File:        hexreader.h

 Description: definition of functions to read a file in Intel HEX format.


 Author:      D. Calvet,        calvet@hep.saclay.cea.fr


 History:
  July 2008: created

*******************************************************************************/
#ifndef HEX_READER_H
#define HEX_READER_H

#include <stdio.h>

#define HEX_FILE_REC_TYPE_DATA 00
#define HEX_FILE_REC_TYPE_EOF 01
#define HEX_FILE_REC_TYPE_ESAR 02
#define HEX_FILE_REC_TYPE_SSAR 03
#define HEX_FILE_REC_TYPE_ELAR 04
#define HEX_FILE_REC_TYPE_SLAR 05

#define HEX_FILE_MAX_LINE_SIZE 120
#define HEX_FILE_MAX_DATA_BYTES_PER_LINE 32

typedef struct _HexContext {
    FILE* hexf;
    char rep[HEX_FILE_MAX_LINE_SIZE];
    char cur_line[HEX_FILE_MAX_LINE_SIZE];
    unsigned char cur_line_data[HEX_FILE_MAX_DATA_BYTES_PER_LINE];
    unsigned short cur_line_start_address;
    unsigned short cur_line_offset_address;
    unsigned char cur_line_sz;
    unsigned char cur_line_ix;
    int line_count;
    int byte_count;
    int is_eof;
} HexContext;

// Function prototypes
int HexReader_Open(HexContext* hc, char* hex_file_name);
int HexReader_Close(HexContext* hc);
int HexReader_ReadBytes(HexContext* hc, int cnt, unsigned int* adr, unsigned char* data);

#endif
