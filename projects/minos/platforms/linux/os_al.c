/*******************************************************************************

 File:        synchro.c

 Description: This module provides Linux implementation of an OS
              abstraction layer.
              It defines OS independant:
		- Mutex (exclusive access to shared ressources)
                   - Semaphore (synchronization mechanism)
		- Thread (creation, priority, etc...)

 Author:      D. Calvet,        calvet@hep.saclay.cea.fr
              I. Mandjavidze, mandjavi@hep.saclay.cea.fr

 History: 
 	Apr/98: function names changed
 	
*******************************************************************************/
/*#define _POSIX_C_SOURCE 199506L*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <errno.h>
#include <limits.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h> 
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/stat.h>

#include "os_al.h"

/* Debug Section */
#define OSAL_ERR       printf
#define OSAL_WAR       printf

#define DBG_Semaphore_Create   
#define DBG_Semaphore_Delete   
#define DBG_Semaphore_Wait     
#define DBG_Semaphore_Signal   

#define DBG_Mutex_Create   
#define DBG_Mutex_Delete   
#define DBG_Mutex_Lock     
#define DBG_Mutex_Unlock   

#define DBG_Thread_Create         
#define DBG_Thread_Exit           
#define DBG_Thread_Kill           
#define DBG_Thread_Join       
#define DBG_Thread_Delete         
#define DBG_Thread_Set_Priority    
#define DBG_Thread_Get_Priority    
#define DBG_Thread_Get_Myself      
#define DBG_Thread_Set_Signal     

#define DBG_try_proc_scheduler   

#define DBG_Pipe_Open  
#define DBG_Pipe_Close 
#define DBG_Pipe_Write          
#define DBG_Pipe_Read           

/* The conversion between DEMOC priorities and OS priorities */
#define OS_SCHED_PRIO_DEFAULT   0
#define OS_SCHED_PRIO_MIN       1
#define OS_SCHED_PRIO_LOW       25
#define OS_SCHED_PRIO_NORMAL    50
#define OS_SCHED_PRIO_HIGH      75
#define OS_SCHED_PRIO_MAX       99

#define OS_SCHED_PRIO_POLOCY    SCHED_FIFO

/* Struct for timeout semaphore routine */
typedef struct {
	int       timeout_usec;    /* timeout interval in microseconds */
	pthread_t thread_id;       /* thread id */
} SyncTimeoutStruct ;

/* This global variable is used for scheduling and memory locking */
#define PROC_SCHED_ERR  -1
#define PROC_SCHED_NONE  0
#define PROC_SCHED_IMP   1
#define PROC_SCHED_DONE  2
int proc_sched = PROC_SCHED_NONE;
/*
 * try_proc_scheduler sets scheduling priority for process and locks its
 * memory if the process has the superuser priority
 * return value:
 *  1 - on success
 *  0 - if process does not have supervisor priority
 * -1 - on failure
 */
int try_proc_scheduler()
{
	int policy;
	struct sched_param param;

	/* Check if the function has been already called */
	if( proc_sched == PROC_SCHED_DONE )
		return( 1 );
	else if( proc_sched == PROC_SCHED_IMP )
		return( 0 );

	/* Get process scheduling type */
	if( ( policy = sched_getscheduler( 0 ) ) < 0 )
	{
		perror( "try_proc_scheduler: sched_getscheduler" );
		proc_sched = PROC_SCHED_ERR;
		return -1;
	}
	DBG_try_proc_scheduler
	(
		"try_proc_scheduler: policy=%d FIFO=%d RR=%d OTHER=%d min=%d max=%d\n",
		policy, SCHED_FIFO, SCHED_RR, SCHED_OTHER,
		sched_get_priority_max(policy),
		sched_get_priority_min(policy)
	);

	DBG_try_proc_scheduler
	(
		"try_proc_scheduler: PRIO_POLICY=%d max=%d min=%d\n",
		OS_SCHED_PRIO_POLOCY,
		sched_get_priority_max(OS_SCHED_PRIO_POLOCY),
		sched_get_priority_min(OS_SCHED_PRIO_POLOCY)
	);

	/* If the policy is not requred one, then try to set it */
	if( policy != OS_SCHED_PRIO_POLOCY )
	{
		/* Get process scheduling priority */
		if( ( policy = sched_getparam( 0, &param ) ) < 0 )
		{
			perror( "try_proc_scheduler: sched_getparam" );
			proc_sched = PROC_SCHED_ERR;
			return -1;
		}
		DBG_try_proc_scheduler
		(
			"try_proc_scheduler: priority=%d min=%d max=%d\n",
			param.sched_priority,
			sched_get_priority_max(policy),
			sched_get_priority_min(policy)
		);

		/* Try to set process required scheduling to real time */
		param.sched_priority = OS_SCHED_PRIO_MIN;
		if( sched_setscheduler( 0, OS_SCHED_PRIO_POLOCY, &param ) < 0 )
		{
			if( errno == EPERM )
			{
				proc_sched = PROC_SCHED_IMP;
/*				perror( "try_proc_scheduler: sched_setscheduler" ); */
				return( 0 );
			}
			else
			{
				perror( "try_proc_scheduler: sched_setscheduler" );
				proc_sched = PROC_SCHED_ERR;
				return -1;
			}
		}

		/* Set process required scheduling type back to non real time */
		param.sched_priority = OS_SCHED_PRIO_DEFAULT;
		if( sched_setscheduler( 0, SCHED_OTHER, &param ) < 0 )
		{
			perror( "try_proc_scheduler: sched_setscheduler" );
			proc_sched = PROC_SCHED_ERR;
			return -1;
		}
	}

	/* Lock memory pages */
	if( mlockall( MCL_CURRENT | MCL_FUTURE ) )
	{
		perror( "try_proc_scheduler: mlockall" );
		proc_sched = PROC_SCHED_ERR;
		return -1;
	}

	/* Seems that all has been done */
	proc_sched = PROC_SCHED_DONE;
	return( 1 );
}

/******************************************************************************/
/* These functions provide synchronization means for threads ******************/
/******************************************************************************/

/******************************************************************************/
/* Semaphore_Create: Creates Counting Semaphore                               */
/*  0 on success                                                              */
/* -1 on failure                                                              */
/******************************************************************************/
int Semaphore_Create( void* *sem )
{
	sem_t *semaphore;

	if( !(semaphore = (sem_t *)malloc( sizeof( sem_t ) ) ) )
	{
		OSAL_ERR("Semaphore_Create failed malloc sem_t\n");
		return -1;
	}
	DBG_Semaphore_Create( "Semaphore_Create: semaphore=0x%x\n", semaphore );
	
	if( sem_init( semaphore, 0, 0 ) < 0 )
	{
		*sem = 0;
		perror( "Semaphore_Create: sem_init" );
		return -1;
	}
	DBG_Semaphore_Create( "Semaphore_Create: sem_init OK\n" );

	DBG_Semaphore_Create( "Semaphore_Create: semaphore=0x%x\n", semaphore );
	*sem = (void *)semaphore;
	DBG_Semaphore_Create( "Semaphore_Create: *sem=0x%x\n", *sem );
	return (0);
}

/******************************************************************************/
/* Semaphore_Delete: deletes Counting Semaphore                               */
/*  0 on success                                                              */
/* -1 on failure                                                              */
/******************************************************************************/
int Semaphore_Delete( void* *sem )
{
	DBG_Semaphore_Delete( "Semaphore_Delete: sem=0x%x\n", sem );

	if( sem_destroy( (sem_t *)(*sem) ) < 0 )
	{
		perror( "Semaphore_Delete: sem_destroy" );
		return( -1 );
	}

	free( *sem );

	*sem = 0;
	DBG_Semaphore_Delete( "Semaphore_Delete: done\n" );
	return 0;
}

/******************************************************************************/
/* Semaphore_Wait: waits on counting semaphore                                */
/*  0 on success                                                              */
/* -1 on failure                                                              */
/******************************************************************************/
int Semaphore_Wait(   void  *sem )
{
	DBG_Semaphore_Wait( "Semaphore_Wait: sem=0x%x\n", sem );

	if( sem_wait( (sem_t *)sem ) < 0 )
	{
		/*perror( "Semaphore_Wait: sem_wait failed" );*/
		return -1;
	}

	DBG_Semaphore_Wait( "Semaphore_Wait: done\n" );
	return 0;
}

/******************************************************************************/
/* Semaphore_Timeout_Thread: this thread handles timeout in                    */
/*  Semaphore_Wait_Timeout                                                    */
/* return none                                                                */
/******************************************************************************/
static void Semaphore_Timeout_Thread (void *param)
{
	SyncTimeoutStruct *pinfo = (SyncTimeoutStruct *) param;

	sleep (pinfo->timeout_usec / 1000000);      /*### SleepTime() */
	pthread_kill (pinfo->thread_id, SIGALRM);

	/* pause until cancelation */
	pause();
}

/******************************************************************************/
/* sig_none: do nothing (signal is only caught)                               */
/* return none                                                                */
/******************************************************************************/
static void sig_none (int sig)
{
	/* do nothing */
}

/******************************************************************************/
/* Semaphore_Wait_Timeout: Waits with timeout on a counting semaphore         */
/*  0 on success                                                              */
/* -1 on failure                                                              */
/* -2 on timeout or signal                                                    */
/******************************************************************************/
int Semaphore_Wait_Timeout (void  *sem, int timeout_usec)
{
	int               iError;
	ThreadStruct      thTimeout;
	SyncTimeoutStruct info;
	pthread_t         thread_id;
	struct sigaction  newact, oldact;
	sigset_t          sigset;

	struct timespec   ts;

	if (clock_gettime(CLOCK_REALTIME, &ts) == -1)
	{
		return (-1);
	}
	//printf("current time: %d\n", ts.tv_sec);
	ts.tv_sec+= (timeout_usec / 1000000);
	//printf("will time out at: %d\n", ts.tv_sec);

	if ((iError = sem_timedwait((sem_t*) sem, &ts)) < 0)
	{
		if (errno == ETIMEDOUT)
		{
			iError = -2;
		}
		else
		{
			iError = -1;
		}
	}
	
	/*
	if (clock_gettime(CLOCK_REALTIME, &ts) == -1)
	{
		return (-1);
	}
	printf("function done at: %d\n", ts.tv_sec);
	*/

	return(iError);

	/* Associates SIGALARM with sig_none() (do nothing) */
	sigemptyset(&sigset);
	newact.sa_handler = sig_none;
	newact.sa_mask    = sigset;
	newact.sa_flags   = 0;
	if (sigaction (SIGALRM, &newact, &oldact))
		return -1;

	/* creates timeout thread */
	info.timeout_usec = timeout_usec;
	info.thread_id    = pthread_self();
	thTimeout.routine = Semaphore_Timeout_Thread;
	thTimeout.param   = (void *) &info;
	if (Thread_Create (&thTimeout))
		return -1;

	/* waits signal or timeout interrupt */
	if(sem_wait ((sem_t *) sem))
		iError = (errno == EINTR ? -2 : -1);
	else
		iError = 0;

	Thread_Kill (&thTimeout);

	/* Deassociates SIGALARM with sig_none() */
	if (sigaction (SIGALRM, &oldact, NULL))
		iError =  -1;

	return iError;
}

/******************************************************************************/
/* Semaphore_Signal: signals counting semaphore                               */
/*  0 on success                                                              */
/* -1 on failure                                                              */
/******************************************************************************/
int Semaphore_Signal( void  *sem )
{
	DBG_Semaphore_Signal( "Semaphore_Signal: sem=0x%x\n", sem );

	if(  sem_post( (sem_t *)sem ) < 0 )
	{
		perror( "Semaphore_Signal: sem_signal failed" );
		return -1;
	}

	DBG_Semaphore_Signal( "Semaphore_Signal: done\n" );
	return 0;
}

/******************************************************************************/
/* These functions provide exclusive access to shared objects *****************/
/******************************************************************************/

/******************************************************************************/
/* Mutex_Create: Creates Mutex                                                */
/*  0 on success                                                              */
/* -1 on failure                                                              */
/******************************************************************************/
int Mutex_Create( void* *mtx )
{
	pthread_mutex_t     *mutex;

	if( ( mutex=(pthread_mutex_t *)malloc( sizeof( pthread_mutex_t ) ) ) == 0 )
	{
		*mtx = 0;
		OSAL_ERR( "Mutex_Create: failed malloc mutex struct\n" );
		return -1;
	}

	if( pthread_mutex_init( mutex, (pthread_mutexattr_t *)NULL ) == -1 )
	{
		*mtx = 0;
		perror( "Mutex_Create: pthread_mutex_init" );
		return( -1 );
	}
	DBG_Mutex_Create( "Mutex_Create: mutex initialized\n" );

	DBG_Mutex_Create( "Mutex_Create: mtx=0x%x\n", mtx );
	*mtx = (void *)mutex;
	DBG_Mutex_Create( "Mutex_Create: *mtx=0x%x\n", *mtx );

	return 0;
}

/******************************************************************************/
/* Mutex_Delete: Deletes Mutex                                                */
/*  0 on success                                                              */
/* -1 on failure                                                              */
/******************************************************************************/
int Mutex_Delete( void* *mtx )
{
	DBG_Mutex_Delete( "Mutex_Delete: mtx=0x%x\n", mtx );

	if( pthread_mutex_destroy( (pthread_mutex_t *)(*mtx) ) == -1 )
	{
		perror( "Mutex_Delete: pthread_mutex_destroy" );
		return( -1 );
	}

	*mtx = 0;
	DBG_Mutex_Delete( "Mutex_Delete: done\n" );
	return 0;
}

/******************************************************************************/
/* Mutex_Create: locks Mutex                                                  */
/*  0 on success                                                              */
/* -1 on failure                                                              */
/******************************************************************************/
int Mutex_Lock( void *mtx )
{
	DBG_Mutex_Lock( "Mutex_Lock: mtx=0x%x\n", mtx );

	if( pthread_mutex_lock( (pthread_mutex_t *)mtx ) < 0 )
	{
		perror( "Mutex_Lock: pthread_mutex_lock failed" );
		return -1;
	}

	DBG_Mutex_Lock( "Mutex_Lock: done\n" );
	return 0;
}

/******************************************************************************/
/* Mutex_Create: unlocks Mutex                                                */
/*  0 on success                                                              */
/* -1 on failure                                                              */
/******************************************************************************/
int Mutex_Unlock( void *mtx )
{
	DBG_Mutex_Unlock( "Mutex_Unlock: mtx=0x%x\n", mtx );

	if( pthread_mutex_unlock( (pthread_mutex_t *)mtx ) < 0 )
	{
		perror( "Mutex_Unlock: pthread_mutex_unlock failed" );
		return -1;
	}

	DBG_Mutex_Unlock( "Mutex_Unlock: done\n" );
	return 0;
}

/******************************************************************************/
/* These functions operate on threads *****************************************/
/******************************************************************************/

/******************************************************************************/
/* Thread_Create: Creates Thread                                              */
/*  0 on success                                                              */
/* -1 on failure                                                              */
/******************************************************************************/
int Thread_Create( ThreadStruct *thread )
{
	pthread_t       thread_id;
	pthread_attr_t  thread_attr;
	
	struct timespec rqtp;
	struct timespec rmtp;

	if( try_proc_scheduler() < 0 )
	{
		OSAL_ERR( "Thread_Get_Myself: try_proc_scheduler failed\n" );
		return -1;
	}

	DBG_Thread_Create( "Thread_Create: started\n");
	DBG_Thread_Create( "Thread_Create: sizeof(pthread_t)=%d\n",
		sizeof(pthread_t));

	DBG_Thread_Create( "Thread_Create: thread struct=0x%x\n", thread );
	DBG_Thread_Create( "Thread_Create: routine=0x%x param=0x%x\n",
		thread->routine, thread->param );

	if( pthread_attr_init( &thread_attr ) < 0 )
	{
		thread->thread_id = 0;
		perror( "Thread_Create: pthread_attr_init" );
		return( -1 );
	}
	DBG_Thread_Create( "Thread_Create: thread attributes initialised\n" );

	if( pthread_attr_setinheritsched( &thread_attr, PTHREAD_INHERIT_SCHED )<0 )
	{
		perror( "Thread_Create: pthread_attr_setinheritsched" );
		goto error_exit;
	}
	DBG_Thread_Create( "Thread_Create: PTHREAD_INHERIT_SCHED set \n" );
/*
	if( pthread_attr_setschedpolicy( &thread_attr, OS_SCHED_PRIO_POLOCY )<0 )
	{
		perror( "Thread_Create: pthread_attr_setschedpolicy" );
		goto error_exit;
	}
	DBG_Thread_Create( "Thread_Create: OS_SCHED_PRIO_POLOCY set \n" );
*/

	if(pthread_create( &thread_id,
	                   (pthread_attr_t *)(&thread_attr),
		         (void* (*)(void*))thread->routine,
		         thread->param))		
	{
		perror( "Thread_Create: pthread_create" );
		goto error_exit;
	}

	DBG_Thread_Create( "Thread_Create: pthread_create done\n");
	thread->thread_id = (long) thread_id;
	thread->handle    = 0;
	
	pthread_attr_destroy( &thread_attr );

	rqtp.tv_sec = 0;
	rqtp.tv_nsec = 1000;
	DBG_Thread_Create( "Thread_Create: tv_nsec=%d\n", rqtp.tv_nsec );
	if( nanosleep( &rqtp, &rmtp ) < 0 )
	{
		perror( "Thread_Create: nanosleep" );
		return -1;
	}
	DBG_Thread_Create( "Thread_Create: after nanosleep of tv_nsec=%d\n",
				rqtp.tv_nsec );

	if( ( thread->current_priority = Thread_Get_Priority( thread ) ) < 0 )
	{
		perror( "Thread_Create: thr_getprio" );
		goto error_exit;
	}
	
	DBG_Thread_Create( "Thread_Create: tid=0x%x hndl=0x%x prio=%d\n",
		thread->thread_id, thread->handle, thread->current_priority );
	return 0;
	
error_exit:
	thread->thread_id = -1;
	pthread_attr_destroy( &thread_attr );
	return -1;
}

/******************************************************************************/
/* Thread_Exit: the thread calls this function to leave the code              */
/*  0 on success                                                              */
/* -1 on failure                                                              */
/******************************************************************************/
int Thread_Exit( ThreadStruct *thread )
{

	DBG_Thread_Exit( "Thread_Exit: thread=0x%x\n", thread );
	DBG_Thread_Exit( "Thread_Exit: status=%d\n", thread->status );
	DBG_Thread_Exit( "Thread_Exit: id=%d 0x%x\n",
		thread->thread_id, thread->thread_id );

	pthread_detach( (pthread_t)(thread->thread_id) );
	thread->thread_id = -1;
	thread->handle    = 0;
	DBG_Thread_Exit( "Thread_Exit: id=%d\n", thread->thread_id );
	pthread_exit( (void *) &(thread->status) );

	OSAL_ERR( "Thread_Exit: after pthread_exit\n" );
	return 0;
}

/******************************************************************************/
/* Thread_Kill: function to kill a thread                                     */
/*  0 on success                                                              */
/* -1 on failure                                                              */
/******************************************************************************/
int Thread_Kill( ThreadStruct *thread )
{
	DBG_Thread_Kill( "Thread_Kill: thread=0x%x\n", thread );
	DBG_Thread_Kill( "Thread_Kill: thread_id=%d\n", thread->thread_id );

	if( pthread_cancel( (pthread_t)thread->thread_id ) < 0 )
	{
		perror( "Thread_Kill: pthread_kill" );
		return( -1 );
	}

	DBG_Thread_Kill( "Thread_Kill: done\n" );
	return 0;
}

/******************************************************************************/
/* Thread_Join: wait until a thread is not stoped                             */
/*  thread return status on success                                           */
/* -1 on failure                                                              */
/******************************************************************************/
int Thread_Join( ThreadStruct *thread )
{
	int status = 0;

	DBG_Thread_Join( "Thread_Join: thread=0x%x\n", thread );
	DBG_Thread_Join( "Thread_Join: thread_id=0x%x\n", thread->thread_id );

	if( pthread_join( (pthread_t)(thread->thread_id), (void **)&thread->status ) == -1 )
	{
		perror( "Thread_Join: pthread_join" );
		return( -1 );
	}
	return status;
}

/******************************************************************************/
/* No Idea yet                                                                */
/******************************************************************************/
int Thread_Delete( ThreadStruct *thread )
{
	OSAL_ERR( "Thread_Delete: Not yet implemented\n" );
	return -1;
}

/******************************************************************************/
/* Thread_Set_Priority: set priority                                          */
/*  0 on success                                                              */
/* -1 on failure                                                              */
/******************************************************************************/
int Thread_Set_Priority( ThreadStruct *thread, int priority )
{
	int status;
	int new_priority;
	struct sched_param param;

	/* If it is impossible to change the priority, return */
	if( proc_sched == PROC_SCHED_IMP )
	{
		return 0;
	}
	else if( proc_sched != PROC_SCHED_DONE )
	{
		OSAL_ERR( "Thread_Set_Priority: try_proc_scheduler was not called\n" );
		return -1;
	}

	DBG_Thread_Set_Priority("Thread_Set_Priority: thread=0x%x\n",    thread );
	DBG_Thread_Set_Priority("Thread_Set_Priority: thread_id=0x%x\n",
		thread->thread_id);
	DBG_Thread_Set_Priority("Thread_Set_Priority: priority=%d\n",    priority );

	if( priority == THREAD_PRIO_MIN )
		param.sched_priority = OS_SCHED_PRIO_MIN;
	else if( priority == THREAD_PRIO_DEFAULT )
		param.sched_priority = OS_SCHED_PRIO_DEFAULT;
	else if( priority == THREAD_PRIO_LOW )
		param.sched_priority = OS_SCHED_PRIO_LOW;
	else if( priority == THREAD_PRIO_NORMAL )
		param.sched_priority = OS_SCHED_PRIO_NORMAL;
	else if( priority == THREAD_PRIO_HIGH )
		param.sched_priority = OS_SCHED_PRIO_HIGH;
	else if( priority == THREAD_PRIO_MAX )
		param.sched_priority = OS_SCHED_PRIO_MAX;
	else
	{
		OSAL_ERR( "Thread_Set_Priority: unsupported priority %d\n", priority );
		return -1;
	}
	DBG_Thread_Set_Priority( "Thread_Set_Priority: sys_prio=%d\n",
		param.sched_priority );

	if( param.sched_priority == OS_SCHED_PRIO_DEFAULT )
	{
		if( pthread_setschedparam( (pthread_t)thread->thread_id,
			SCHED_OTHER, &param ) < 0 )
		{
			perror( "Thread_Set_Priority: pthread_setschedparam" );
			return -1;
		}
	}
	else
	{
		if( pthread_setschedparam( (pthread_t)thread->thread_id,
			OS_SCHED_PRIO_POLOCY, &param ) < 0 )
		{
			perror( "Thread_Set_Priority: pthread_setschedparam" );
			return -1;
		}
	}
	DBG_Thread_Set_Priority( "Thread_Set_Priority: pthread_setprio OK\n" );

	if( (new_priority = Thread_Get_Priority( thread ) ) < 0 )
	{
		OSAL_ERR( "Thread_Set_Priority: failed to chek priority\n" );
		return -1;
	}
	DBG_Thread_Set_Priority( "Thread_Set_Priority: new_prio=%d\n",
		new_priority );

	if( new_priority != priority )
	{
		OSAL_ERR( "Thread_Set_Priority: new %d not equeal req %d priority\n",
			new_priority, priority );
		return -1;
	}

	DBG_Thread_Set_Priority( "Thread_Set_Priority: done\n" );
	return 0;
}

/******************************************************************************/
/* Thread_Get_Priority: get priority                                          */
/*  priority on success                                                       */
/* -1 on failure                                                              */
/******************************************************************************/
int Thread_Get_Priority( ThreadStruct *thread )
{
	int sys_prio;
	int policy;
	struct sched_param param;
	
	DBG_Thread_Get_Priority("Thread_Get_Priority: thread=0x%x\n", thread );
	DBG_Thread_Get_Priority("Thread_Get_Priority: thread_id=0x%x\n",
		thread->thread_id);

	if( pthread_getschedparam( (pthread_t)thread->thread_id,&policy,&param )
	< 0 )
	{
		perror( "Thread_Get_Priority:pthread_getschedparam" );
		return -1;
	}
	DBG_Thread_Get_Priority
	(
		"Thread_Get_Priority: policy=%d priority=%d max=%d min=%d\n",
		policy, param.sched_priority,
		sched_get_priority_max( policy ),
		sched_get_priority_min( policy )
	);

	sys_prio = param.sched_priority;
	DBG_Thread_Get_Priority( "Thread_Get_Priority: policy=%d sys_prio=%d\n",
		policy, sys_prio );

	if( sys_prio > OS_SCHED_PRIO_HIGH )
		thread->current_priority = THREAD_PRIO_MAX;
	else if( sys_prio > OS_SCHED_PRIO_NORMAL )
		thread->current_priority = THREAD_PRIO_HIGH;
	else if( sys_prio > OS_SCHED_PRIO_LOW )
		thread->current_priority = THREAD_PRIO_NORMAL;
	else if( sys_prio > OS_SCHED_PRIO_MIN )
		thread->current_priority = THREAD_PRIO_LOW;
	else if( sys_prio > OS_SCHED_PRIO_DEFAULT )
		thread->current_priority = THREAD_PRIO_MIN;
	else
		thread->current_priority = THREAD_PRIO_DEFAULT;
	DBG_Thread_Get_Priority( "Thread_Get_Priority: priority=%d\n",
		thread->current_priority );

	return( thread->current_priority );
}

/******************************************************************************/
/* signal_set_fun: sets signal handler for all signals                        */
/*  0 on success                                                              */
/* -1 on failure                                                              */
/******************************************************************************/
void (*remoute_signal_handler)() = NULL;
int Thread_LSH_cntr = 0;
static void Thread_LSH()
{
	sigset_t sig;
	unsigned int *ptr;
	int index;

	Thread_LSH_cntr++;
	if( Thread_LSH_cntr > 1 )
	{
		OSAL_WAR( "Thread_LSH: called %d times\n", Thread_LSH_cntr );
		exit( 0 );
	}

	if( sigprocmask( SIG_SETMASK, NULL, &sig ) < 0 )
	{
		OSAL_ERR( "Thread_LSH: sigprocmask failed\n" );
		perror( "Thread_LSH: sigprocmask" );
	}
	OSAL_WAR( "Thread_LSH: call with signal mask=" );
	ptr = (unsigned int *)&sig;
	for( index=0; index<2; index++ )
		OSAL_WAR( "0x%x ", *ptr++ );
	OSAL_WAR( "\n" );

	if( remoute_signal_handler != NULL )
		remoute_signal_handler();
}

int Thread_Set_Signal( ThreadStruct *thread, void *sig_fun )
{
	DBG_Thread_Set_Signal( "Thread_Set_Signal: thread=0x%x\n",  
		thread );
	DBG_Thread_Set_Signal( "Thread_Set_Signal: thread_id=0x%x\n",
		thread->thread_id );

	remoute_signal_handler = sig_fun;

	if( signal( SIGINT, SIG_IGN ) != SIG_IGN )
		signal( SIGINT, (void (*)(int)) Thread_LSH );
	if( signal( SIGQUIT, SIG_IGN ) != SIG_IGN )
		signal( SIGQUIT, (void (*)(int)) Thread_LSH );
	if( signal( SIGKILL, SIG_IGN ) != SIG_IGN )
		signal( SIGKILL, (void (*)(int)) Thread_LSH );
	if( signal( SIGABRT, SIG_IGN ) != SIG_IGN )
		signal( SIGABRT, (void (*)(int)) Thread_LSH );
	if( signal( SIGBUS,  SIG_IGN ) != SIG_IGN )
		signal( SIGBUS,  (void (*)(int)) Thread_LSH );
	if( signal( SIGSEGV, SIG_IGN ) != SIG_IGN )
		signal( SIGSEGV, (void (*)(int)) Thread_LSH );
	if( signal( SIGFPE,  SIG_IGN ) != SIG_IGN )
		signal( SIGFPE,  (void (*)(int)) Thread_LSH );
	if( signal( SIGILL,  SIG_IGN ) != SIG_IGN )
		signal( SIGILL,  (void (*)(int)) Thread_LSH );
	if( signal( SIGTERM,  SIG_IGN ) != SIG_IGN )
		signal( SIGTERM,  (void (*)(int)) Thread_LSH );
	if( signal( SIGPIPE,  SIG_IGN ) != SIG_IGN )
		signal( SIGPIPE,  (void (*)(int)) Thread_LSH );

	DBG_Thread_Set_Signal( "Thread_Set_Signal: done\n" );
	return 0;
}

/******************************************************************************/
/* Gets threads id and sets it current priority                               */
/*  thread id on success                                                      */
/* -1 on failure                                                              */
/******************************************************************************/
long Thread_Get_Myself( ThreadStruct *thread )
{
	int prio;

	if( try_proc_scheduler() < 0 )
	{
		OSAL_ERR( "Thread_Get_Myself: try_proc_scheduler failed\n" );
		return -1;
	}

	DBG_Thread_Get_Priority("Thread_Get_Priority: thread=0x%x\n", thread );
	thread->thread_id = (unsigned long)pthread_self();
	DBG_Thread_Get_Myself( "Thread_Get_Myself: id=0x%x\n", thread->thread_id );

	if( (prio = Thread_Get_Priority( thread ) ) < 0 )
	{
		OSAL_ERR( "Thread_Get_Myself: failed to chek priority\n" );
		return -1;
	}
	DBG_Thread_Get_Myself( "Thread_Get_Myself: prio=%d\n", prio );

	return( thread->thread_id );
}

/******************************************************************************/
/* These functions operate on pipes   *****************************************/
/******************************************************************************/

/* Define max number of messages in queues and max size of messages */
#define OSAL_MAX_NB_OF_MSG 16
#define OSAL_MAX_MSG_SIZE  ((4056) - sizeof(struct msgbuf))

/*
 * Define Pipe structure
 */
typedef struct _Pipe
{
	int type;
	int s2c_msg_id;
	int c2s_msg_id;
	struct msgbuf *s2c_msg_buf;
	struct msgbuf *c2s_msg_buf;
} Pipe;
/* Key values for messages */
#define OSAL_MSG_KEY_S2C 0xD0D0
#define OSAL_MSG_KEY_C2S 0xD2D2
/* MType values for messages */
#define OSAL_MSG_MTYPE_S2C 5
#define OSAL_MSG_MTYPE_C2S OSAL_MSG_MTYPE_S2C
/* Types of nodes */
#define OSAL_MSG_SERVER 1
#define OSAL_MSG_CLIENT 0

/*
 * Pipe_Create : Creates named pipe in server
 *  0 on success
 * -1 on failure
 */
int Pipe_Create( void* *pi, char *pipe_name )
{
	OSAL_ERR( "Pipe_Create: not implemented\n" );
	return -1;
}

/*
 * Pipe_Delete : Deletes named pipe in server
 *  0 on success
 * -1 on failure
 */
int Pipe_Delete( void* *pi )
{
	OSAL_ERR( "Pipe_Delete: not implemented\n" );
	return -1;
}

/*
 * Pipe_Connect : Connect server to named pipe
 * this function is empty in Lynx
 *  0 on success
 * -1 on failure
 */
int Pipe_Connect( void *pi )
{
	return 0;
}

/*
 * Pipe_Open : Creates named pipe in client
 *  0 on success
 * -1 on failure
 */
int Pipe_Open( void* *pi, char *pipe_name )
{
	OSAL_ERR( "Pipe_Open: not implemented\n" );
	return -1;
}

/*
 * Pipe_Close : Deletes named pipe in client
 *  0 on success
 * -1 on failure
 */
int Pipe_Close( void* *pi )
{
	OSAL_ERR( "Pipe_Close: not implemented\n" );
	return -1;
}

/*
 * Pipe_Read : Reads a message from pipe
 *  0 on success
 * -1 on failure
 */
int Pipe_Read( void *pi, char *buf, int *sz )
{
	OSAL_ERR( "Pipe_Read: not implemented\n" );
	return -1;
}

/*
 * Pipe_Write : Writes a message to pipe
 *  0 on success
 * -1 on failure
 */
int Pipe_Write( void *pi, char *buf, int sz )
{
	OSAL_ERR( "Pipe_Write: not implemented\n" );
	return -1;
}
