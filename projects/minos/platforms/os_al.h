/*******************************************************************************

 File:        os_al.h

 Description: This module provides an OS abstraction layer.
              It declares OS independant:
		- Mutex (exclusive access to shared ressources)
		- Semaphore (synchronization mechanism)
		- Thread (creation, priority, etc...)
              See os_al.c in the OS specific directory for implementation.

 Author:      D. Calvet,        calvet@hep.saclay.cea.fr
              I. Mandjavidze, mandjavi@hep.saclay.cea.fr

 History:
 	Apr/98: function names changed
 	   
*******************************************************************************/

#ifndef OS_AL_H
#define OS_AL_H

/* Operating system independent synchronization mechanism for threads */
int Semaphore_Create(       void* *sem                   );
int Semaphore_Delete(       void* *sem                   );
int Semaphore_Wait(         void  *sem                   );
int Semaphore_Wait_Timeout( void  *sem, int timeout_usec );
int Semaphore_Signal(       void  *sem                   );

/* Operating system independent mutual exclusion mechanism for threads */
int Mutex_Create( void* *mtx );
int Mutex_Delete( void* *mtx );
int Mutex_Lock(   void  *mtx );
int Mutex_Unlock( void  *mtx );

/* Operating system independent thread */
typedef struct _ThreadStruct
{
	long          thread_id;
	unsigned int  handle;
	void          (*routine)();
	void          *param;
	int           current_priority;
	int           status;
} ThreadStruct;

#define THREAD_PRIO_MIN     0
#define THREAD_PRIO_LOW     1
#define THREAD_PRIO_NORMAL  2
#define THREAD_PRIO_HIGH    3
#define THREAD_PRIO_MAX     4
#define THREAD_PRIO_DEFAULT 5

int  Thread_Create(       ThreadStruct *thread                );
int  Thread_Exit(         ThreadStruct *thread                );
int  Thread_Kill(         ThreadStruct *thread                );
int  Thread_Delete(       ThreadStruct *thread                );
int  Thread_Join(         ThreadStruct *thread                );
int  Thread_Set_Priority( ThreadStruct *thread, int priority  );
int  Thread_Get_Priority( ThreadStruct *thread                );
long Thread_Get_Myself(   ThreadStruct *thread                );
int  Thread_Set_Signal(   ThreadStruct *thread, void *sig_fun );

/* Operating system independent bi-directionnal named pipes for messages */
int Pipe_Create(  void* *pi, char *pipe_name );
int Pipe_Delete(  void* *pi );
int Pipe_Connect( void  *pi );
int Pipe_Open(    void* *pi, char *pipe_name );
int Pipe_Close(   void* *pi );
int Pipe_Read(    void  *pi, char *buf, int *sz );
int Pipe_Write(   void  *pi, char *buf, int sz );

#endif /* #ifndef OS_AL_H */
