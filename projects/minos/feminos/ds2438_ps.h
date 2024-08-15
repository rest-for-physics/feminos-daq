/*******************************************************************************
                           T2K 280 m TPC readout
                           _____________________

 File:        ds2438_ps.h

 Description: Definition of platform specific items for controlling a DS2438.
              version for: PowerPC 405

 Author:      X. de la Bro�se,        labroise@cea.fr
              D. Calvet,        calvet@hep.saclay.cea.fr

 History:
  October 2007: created

*******************************************************************************/
#ifndef DS2438_PS_H
#define DS2438_PS_H

// Defines the number of times the Busy bit is checked before causing a timeout
// error after posting a RESET/Presence, a read bit or a write bit command to
// the DS2438. The operation takes up to 800 us. A timeout of >=1 ms is safe.
#define OW_MAX_RETRY 3000

// Defines the number of times the Busy line is checked before causing a timeout
// error after posting an A/D conversion to the DS2438.
// The longest operation takes up to 10 ms. A timeout >=15 ms is safe.
#define OW_MAX_RETRY_ON_CONVERT_BUSY 50000

#endif
