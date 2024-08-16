/*******************************************************************************
                           T2K 280 m TPC readout
                           _____________________

 File:        read.c

 Description: read a string from stdin - the implementation of read
 supplied by Xilinx is bugged.
 CR-LF is replaced by LF at the end of a line and CR is not counted in the
 number of characters returned. Also a null character is added to terminate
 the string.


 Author:      D. Calvet,        calvet@hep.saclay.cea.fr


 History:
  July 2006: created
  2009/01/21: IM Some terminals send CR only instead of CR/LF sequence
                 Code modified to treat both cases

*******************************************************************************/

extern char inbyte(void);

/*
 * read bytes from stdin serial port
 */
int read_stdin(char* buf, int nbytes) {
    int i = 0;

    for (i = 0; i < nbytes; i++) {
        *(buf + i) = inbyte();
        if (*(buf + i) == '\n') // LF after already treated CR
        {
            i = -1;
        }
        if (*(buf + i) == '\r') // CR
        {
            *(buf + i) = '\n';
            *(buf + i + 1) = 0;
            break;
        }
    }
    return (i);
}
