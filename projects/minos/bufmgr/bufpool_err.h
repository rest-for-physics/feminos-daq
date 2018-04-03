/*******************************************************************************
                           T2K 280 m TPC readout
                           _____________________

 File:        bufpool_err.h

 Description: Error codes for Buffer Pool entity.


 Author:      D. Calvet,        calvet@hep.saclay.cea.fr
              

 History:
  June 2009: created
  
*******************************************************************************/

#ifndef BUFPOOL_ERR_H
#define BUFPOOL_ERR_H

#define ERR_BUFPOOL_FREE_BUFFER_NOT_FOUND         -400
#define ERR_BUFPOOL_NO_FREE_BUFFER                -401
#define ERR_BUFPOOL_ATTEMPT_RETURN_NONBUSY_BUFFER -402
#define ERR_BUFPOOL_BUFFER_FREE_COUNT_UNDERRANGE  -403
#define ERR_BUFPOOL_BUFFER_FREE_COUNT_OVERRANGE   -404
#define ERR_BUFPOOL_INVALID_BUFFER_ADDRESS        -405
#define ERR_BUFPOOL_MISALIGNED_BUFFER_ADDRESS     -406

#endif

