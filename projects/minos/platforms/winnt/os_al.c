/*******************************************************************************

 File:        os_al.c

 Description: This module provides an OS abstraction layer.
              It defines OS independant:
                   - Mutex (exclusive access to shared ressources)
                   - Semaphore (synchronization mechanism)
                   - Thread (creation, priority, etc...)

              This is the Windows NT implementation.

 Author:      D. Calvet,        calvet@hep.saclay.cea.fr
              I. Mandjavidze, mandjavi@hep.saclay.cea.fr
              J.M. Conte,     jmconte@hep.saclay.cea.fr

 History:
    April/98: function names changed

*******************************************************************************/

#include "os_al.h"
#include "platform_spec.h"
#include <stdio.h>

/* Debug Section */
#define OS_AL_ERR printf

#define DBG_Pipe_Create
#define DBG_Pipe_Delete
#define DBG_Pipe_Open
#define DBG_Pipe_Connect
#define DBG_Pipe_Close

#define DBG_Semaphore_Create
#define DBG_Semaphore_Delete
#define DBG_Semaphore_Wait
#define DBG_Semaphore_Wait_Timeout
#define DBG_Semaphore_Signal

#define DBG_Mutex_Create
#define DBG_Mutex_Delete
#define DBG_Mutex_Lock
#define DBG_Mutex_Unlock

#define DBG_Thread_Create
#define DBG_Thread_Delete
#define DBG_Thread_Set_Priority
#define DBG_Thread_Get_Priority
#define DBG_Thread_Exit
#define DBG_Thread_Join

#define MAX_SEM_COUNT 10000000

/******************************************************************************/
/* Semaphore_Create: Creates a synchronization object                         */
/*  0 on success                                                              */
/* -1 on failure                                                              */
/******************************************************************************/
int Semaphore_Create(void** sem) {
    if ((*sem = (HANDLE) CreateSemaphore(NULL, 0, MAX_SEM_COUNT, NULL)) == NULL) {
        perror("Semaphore_Create failed\n");
        return (-1);
    }
    DBG_Semaphore_Create("Semaphore_Create: sem=0x%x\n", sem);
    return (0);
}

/******************************************************************************/
/* Semaphore_Delete: Deletes a synchronization object                         */
/*  0 on success                                                              */
/* -1 on failure                                                              */
/******************************************************************************/
int Semaphore_Delete(void** sem) {
    CloseHandle((HANDLE) *sem);
    *sem = 0;
    return (0);
}

/******************************************************************************/
/* Semaphore_Wait: Waits for a synchronization object to be signaled          */
/*  0 on success                                                              */
/* -1 on failure                                                              */
/******************************************************************************/
int Semaphore_Wait(void* sem) {
    if (WaitForSingleObject(sem, INFINITE) == WAIT_OBJECT_0) {
        DBG_Semaphore_Wait("Semaphore_Wait: sem 0x%x signaled\n", sem);
        return (0);
    } else {
        OS_AL_ERR("Semaphore_Wait: Wait for sem 0x%x failed\n", sem);
        return (-1);
    }
}

/******************************************************************************/
/* Semaphore_Wait_timeout: Waits with timeout for a synchronization object to */
/* be signaled                                                                */
/*  0 on success                                                              */
/* -1 on failure                                                              */
/* -2 on timeout or signal                                                    */
/******************************************************************************/
int Semaphore_Wait_Timeout(void* sem, int timeout_usec) {
    switch (WaitForSingleObject(sem, timeout_usec / 1000)) {
        case WAIT_OBJECT_0:
            DBG_Semaphore_Wait_Timeout("Semaphore_Wait_timeout: sem 0x%x signaled\n", sem);
            return (0);
        case WAIT_TIMEOUT:
            DBG_Semaphore_Wait_Timeout("Semaphore_Wait_timeout: sem 0x%x timeout\n", sem);
            return (-2);
        default:
            OS_AL_ERR("Semaphore_Wait_Timeout: Wait for sem 0x%x failed\n", sem);
            return (-1);
    }
}

/******************************************************************************/
/* Semaphore_Signal: Signals a synchronization object                         */
/*  0 on success                                                              */
/* -1 on failure                                                              */
/******************************************************************************/
int Semaphore_Signal(void* sem) {
    if (ReleaseSemaphore(sem, 1, NULL)) {
        DBG_Semaphore_Signal("Semaphore_Signal: sem 0x%x signaled\n", sem);
        return (0);
    } else {
        OS_AL_ERR("Semaphore_Signal: could not signal sem 0x%x\n", sem);
        return -1;
    }
}

/******************************************************************************/
/* These functions provide exclusive access to shared objects *****************/
/******************************************************************************/

/******************************************************************************/
/* Mutex_Create: Creates Mutex                                                */
/*  0 on success                                                              */
/* -1 on failure                                                              */
/******************************************************************************/
int Mutex_Create(void** mtx) {
    if ((*mtx = (HANDLE) CreateMutex(NULL, FALSE, NULL)) == NULL) {
        perror("Mutex_Create failed\n");
        return (-1);
    }
    DBG_Mutex_Create("Mutex_Create: mtx=0x%x\n", mtx);
    return (0);
}

/******************************************************************************/
/* Mutex_Delete: Deletes Mutex                                                */
/*  0 on success                                                              */
/* -1 on failure                                                              */
/******************************************************************************/
int Mutex_Delete(void** mtx) {
    DBG_Mutex_Delete("Mutex_Delete: mtx=0x%x\n", mtx);
    CloseHandle((HANDLE) *mtx);
    *mtx = 0;
    return (0);
}

/******************************************************************************/
/* Mutex_Lock: Locks Mutex                                                    */
/*  0 on success                                                              */
/* -1 on failure                                                              */
/******************************************************************************/
int Mutex_Lock(void* mtx) {
    if (WaitForSingleObject(mtx, INFINITE) == WAIT_OBJECT_0) {
        DBG_Mutex_Lock("Mutex_Lock: locked mtx 0x%x\n", mtx);
        return (0);
    } else {
        OS_AL_ERR("Mutex_Lock: Could not lock mtx 0x%x\n", mtx);
        return (-1);
    }
}

/******************************************************************************/
/* Mutex_Unlock: Unlocks Mutex                                                */
/*  0 on success                                                              */
/* -1 on failure                                                              */
/******************************************************************************/
int Mutex_Unlock(void* mtx) {
    if (ReleaseMutex(mtx)) {
        DBG_Mutex_Unlock("Mutex_Unlock: unlocked mtx 0x%x\n", mtx);
        return (0);
    } else {
        OS_AL_ERR("Mutex_Unlock: Could not unlock mtx 0x%x\n", mtx);
        return (-1);
    }
}

/******************************************************************************/
/* Thread_Create: Creates a Thread                                            */
/*  0 on success                                                              */
/* -1 on failure                                                              */
/******************************************************************************/
int Thread_Create(ThreadStruct* thread) {
    HANDLE thr;

    thr = CreateThread(NULL,
                       0,
                       (LPTHREAD_START_ROUTINE) thread->routine,
                       (LPVOID) (thread->param),
                       0,
                       (LPDWORD) (&thread->thread_id));
    if (thr != NULL) {
        thread->handle = (unsigned int) thr;
        DBG_Thread_Create("Thread_Create: created thread 0x%x\n", thr);
        return (0);
    } else {
        OS_AL_ERR("Thread_Create: failed\n");
        return (-1);
    }
}

/******************************************************************************/
/* Thread_Exit: the thread calls this function to leave the code              */
/*  0 on success                                                              */
/* -1 on failure                                                              */
/******************************************************************************/
int Thread_Exit(ThreadStruct* thread) {
    DBG_Thread_Exit("Thread_Exit: thread=0x%x\n", thread);
    DBG_Thread_Exit("Thread_Exit: status=%d\n", thread->status);

    thread->thread_id = -1;
    thread->handle = 0;
    ExitThread((DWORD) thread->status);

    DBG_Thread_Exit("Thread_Exit: done\n");
    return 0;
}

/******************************************************************************/
/* Thread_Kill: function to kill a thread                                     */
/*  0 on success                                                              */
/* -1 on failure                                                              */
/******************************************************************************/
int Thread_Kill(ThreadStruct* thread) {
    return TerminateThread((HANDLE) thread->handle, (DWORD) thread->status) ? 0 : -1;
}

/******************************************************************************/
/* Thread_Join: wait until a thread is not stoped                             */
/*  thread return status on success                                           */
/* -1 on failure                                                              */
/******************************************************************************/
int Thread_Join(ThreadStruct* thread) {
    unsigned int status = 0;

    DBG_Thread_Join("Thread_Join: thread=0x%x\n", thread);
    DBG_Thread_Join("Thread_Join: thread_id=%d\n", thread->thread_id);

    if ((status == WaitForSingleObject((HANDLE) thread->handle, INFINITE)) == WAIT_FAILED) {
        perror("Thread_Join: pthread_join");
        return (-1);
    }
    return status;
}

/******************************************************************************/
/* Thread_Delete: delete a thread                                             */
/*  0 on success                                                              */
/* -1 on failure                                                              */
/******************************************************************************/
int Thread_Delete(ThreadStruct* thread) {
    OS_AL_ERR("Thread_Delete: Not yet implemented\n");
    return -1;
}

/******************************************************************************/
/* Thread_Set_Priority: set thread priority                                   */
/*  0 on success                                                              */
/* -1 on failure                                                              */
/******************************************************************************/
int Thread_Set_Priority(ThreadStruct* thread, int priority) {
    int prio_nt;

    switch (priority) {
        case THREAD_PRIO_MIN:
            prio_nt = THREAD_PRIORITY_LOWEST;
            break;

        case THREAD_PRIO_LOW:
            prio_nt = THREAD_PRIORITY_BELOW_NORMAL;
            break;

        case THREAD_PRIO_NORMAL:
        case THREAD_PRIO_DEFAULT:
            prio_nt = THREAD_PRIORITY_BELOW_NORMAL;
            break;

        case THREAD_PRIO_HIGH:
            prio_nt = THREAD_PRIORITY_ABOVE_NORMAL;
            break;

        case THREAD_PRIO_MAX:
            prio_nt = THREAD_PRIORITY_HIGHEST;
            break;

        default:
            prio_nt = THREAD_PRIORITY_NORMAL;
            break;
    }

    if (!SetThreadPriority((HANDLE) thread->handle, prio_nt)) {
        OS_AL_ERR("Thread_Set_Priority: failed\n");
        return -1;
    }
    return (0);
}

/******************************************************************************/
/* Thread_Get_Priority: get thread priority                                   */
/*  0 on success                                                              */
/* -1 on failure                                                              */
/******************************************************************************/
int Thread_Get_Priority(ThreadStruct* thread) {
    int prio_nt;

    prio_nt = GetThreadPriority((HANDLE) thread->handle);

    switch (prio_nt) {
        case THREAD_PRIORITY_LOWEST:
            thread->current_priority = THREAD_PRIO_MIN;
            break;

        case THREAD_PRIORITY_BELOW_NORMAL:
            thread->current_priority = THREAD_PRIO_LOW;
            break;

        case THREAD_PRIORITY_NORMAL:
            thread->current_priority = THREAD_PRIO_NORMAL;
            break;

        case THREAD_PRIORITY_ABOVE_NORMAL:
            thread->current_priority = THREAD_PRIO_HIGH;
            break;

        case THREAD_PRIORITY_HIGHEST:
            thread->current_priority = THREAD_PRIO_MAX;
            break;

        default:
            thread->current_priority = THREAD_PRIO_NORMAL;
            break;
    }

    return (thread->current_priority);
}

/******************************************************************************/
/* Thread_Get_Myself: get handle to thread                                    */
/*  0 on success                                                              */
/* -1 on failure                                                              */
/******************************************************************************/
long Thread_Get_Myself(ThreadStruct* thread) {
    thread->handle = (unsigned int) GetCurrentThread();
    thread->current_priority = GetThreadPriority((HANDLE) thread->handle);
    if (((void*) thread->handle) == NULL)
        return (-1);
    return ((long) thread->handle);
}

/******************************************************************************/
/* Thread_Set_Signal: set signal function (e.g. ctrl C handling)              */
/*  0 on success                                                              */
/* -1 on failure                                                              */
/******************************************************************************/
int Thread_Set_Signal(ThreadStruct* thread, void* sig_fun) {
    // signal(SIGINT, sig_fun);
    return (0);
}

/******************************************************************************/
/* Pipe_Delete                                                                */
/*  0 on success                                                              */
/* -1 on failure                                                              */
/******************************************************************************/
int Pipe_Delete(void** pi) {
    HANDLE* the_pipes;

    the_pipes = *((HANDLE*) pi);

    /* check that the structure was allocated */
    if (the_pipes == 0)
        return (0);

    DBG_Pipe_Delete("Pipe_Delete: the_pipes=0x%x\n", the_pipes);

    /* close the first pipe if it exists */
    if (*the_pipes) {
        DBG_Pipe_Delete("Pipe_Delete: closing handle 0x%x\n", *the_pipes);
        if (CloseHandle(*the_pipes) == 0) {
            OS_AL_ERR("Pipe_Delete: CloseHandle: error %d\n", GetLastError());
            return (-1);
        }
    }

    /* close the second pipe if it exists */
    if (*(the_pipes + 1)) {
        DBG_Pipe_Delete("Pipe_Delete: closing handle 0x%x\n", *(the_pipes + 1));
        if (CloseHandle(*(the_pipes + 1)) == 0) {
            OS_AL_ERR("Pipe_Delete: CloseHandle: error %d\n", GetLastError());
            return (-1);
        }
    }

    DBG_Pipe_Delete("Pipe_Delete: free 0x%x\n", the_pipes);
    free(the_pipes);

    *pi = 0;

    return (0);
}

/******************************************************************************/
/* Pipe_Create                                                                */
/*  0 on success                                                              */
/* -1 on failure                                                              */
/******************************************************************************/
int Pipe_Create(void** pi, char* pipe_name) {
    HANDLE pipe_i, pipe_o, *the_pipes;
    char name[256];

    if ((*pi = (void*) malloc(2 * sizeof(HANDLE))) == 0) {
        OS_AL_ERR("Pipe_Create: malloc failed\n");
        Pipe_Delete(pi);
        return (-1);
    }
    the_pipes = *pi;

    /*****************************************************/
    /* Create the inbound pipe                           */
    /*****************************************************/
    sprintf(name, "\\\\.\\pipe\\%sin", pipe_name);
    if ((pipe_i = CreateNamedPipe(
                 name,
                 PIPE_ACCESS_INBOUND,
                 PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
                 1,
                 32768,
                 32768,
                 1000,
                 NULL)) == INVALID_HANDLE_VALUE) {
        OS_AL_ERR("Pipe_Create: CreateNamedPipe: error %d\n", GetLastError());
        Pipe_Delete(pi);
        return (-1);
    }
    DBG_Pipe_Create("Pipe_Create: pipe_i = 0x%x name:%s\n", pipe_i, name);

    /*****************************************************/
    /* Create the outbound pipe                          */
    /*****************************************************/
    sprintf(name, "\\\\.\\pipe\\%sout", pipe_name);
    if ((pipe_o = CreateNamedPipe(
                 name,
                 PIPE_ACCESS_OUTBOUND,
                 PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
                 1,
                 32768,
                 32768,
                 1000,
                 NULL)) == INVALID_HANDLE_VALUE) {
        OS_AL_ERR("Pipe_Create: CreateNamedPipe: error %d\n", GetLastError());
        Pipe_Delete(pi);
        return (-1);
    }
    DBG_Pipe_Create("Pipe_Create: pipe_o = 0x%x name:%s\n", pipe_o, name);

    *the_pipes = pipe_i;
    the_pipes++;
    *the_pipes = pipe_o;

    return (0);
}

/******************************************************************************/
/* Pipe_Open                                                                  */
/*  0 on success                                                              */
/* -1 on failure                                                              */
/******************************************************************************/
int Pipe_Open(void** pi, char* pipe_name) {
    HANDLE* the_pipes;
    char name[256];

    if ((the_pipes = (HANDLE*) malloc(2 * sizeof(HANDLE))) == 0) {
        OS_AL_ERR("Pipe_Create: malloc failed\n");
        Pipe_Close(pi);
        return (-1);
    }
    DBG_Pipe_Open("Pipe_Open: the_pipes=0x%x\n", the_pipes);

    /*****************************************************/
    /* Open the inbound pipe                             */
    /*****************************************************/
    sprintf(name, "\\\\.\\pipe\\%sin", pipe_name);
    DBG_Pipe_Open("Pipe_Open: name:%s\n", name);
    if ((*(the_pipes + 1) = CreateFile(
                 name,
                 GENERIC_WRITE,
                 FILE_SHARE_WRITE,
                 NULL,
                 OPEN_EXISTING,
                 FILE_ATTRIBUTE_NORMAL,
                 NULL)) == INVALID_HANDLE_VALUE) {
        OS_AL_ERR("Pipe_Open: CreateFile: error %d\n", GetLastError());
        Pipe_Close(pi);
        return (-1);
    }
    DBG_Pipe_Open("Pipe_Open: pipe_i = 0x%x name:%s\n", *(the_pipes + 1), name);

    /*****************************************************/
    /* must go to sleep for a while because the server   */
    /* has to execute the second connect before we open  */
    /* the second pipe                                   */
    /*****************************************************/
    Sleep(10);

    /*****************************************************/
    /* Open the outbound pipe                            */
    /*****************************************************/
    sprintf(name, "\\\\.\\pipe\\%sout", pipe_name);
    if ((*(the_pipes) = CreateFile(
                 name,
                 GENERIC_READ,
                 FILE_SHARE_READ,
                 NULL,
                 OPEN_EXISTING,
                 FILE_ATTRIBUTE_NORMAL,
                 NULL)) == INVALID_HANDLE_VALUE) {
        OS_AL_ERR("Pipe_Open: CreateFile: error %d\n", GetLastError());
        Pipe_Close(pi);
        return (-1);
    }
    DBG_Pipe_Open("Pipe_Open: pipe_o = 0x%x name:%s\n", *(the_pipes), name);
    *pi = the_pipes;
    return (0);
}

/******************************************************************************/
/* Pipe_Connect                                                               */
/*  0 on success                                                              */
/* -1 on failure                                                              */
/******************************************************************************/
int Pipe_Connect(void* pi) {
    HANDLE* the_pipes;

    the_pipes = (HANDLE*) pi;

    DBG_Pipe_Connect("Pipe_Connect: going to connect to pipe 0x%x\n", *the_pipes);
    if (ConnectNamedPipe(*the_pipes, NULL) == 0) {
        OS_AL_ERR("Pipe_Connect: ConnectNamedPipe: error %d\n", GetLastError());
        return (-1);
    }

    the_pipes++;
    DBG_Pipe_Connect("Pipe_Connect: going to connect to pipe 0x%x\n", *the_pipes);
    if (ConnectNamedPipe(*the_pipes, NULL) == 0) {
        OS_AL_ERR("Pipe_Connect: ConnectNamedPipe: error %d\n", GetLastError());
        return (-1);
    }

    return (0);
}

/******************************************************************************/
/* Pipe_Close                                                                 */
/*  0 on success                                                              */
/* -1 on failure                                                              */
/******************************************************************************/
int Pipe_Close(void** pi) {
    HANDLE* the_pipes;

    the_pipes = *((HANDLE*) pi);

    /* check that the structure was allocated */
    if (the_pipes == 0)
        return (0);

    DBG_Pipe_Close("Pipe_Close: the_pipes=0x%x\n", the_pipes);

    /* close the first pipe if it exists */
    if (*the_pipes) {
        DBG_Pipe_Close("Pipe_Close: closing handle 0x%x\n", *the_pipes);
        if (CloseHandle(*the_pipes) == 0) {
            OS_AL_ERR("Pipe_Close: CloseHandle: error %d\n", GetLastError());
            return (-1);
        }
    }

    /* close the second pipe if it exists */
    if (*(the_pipes + 1)) {
        DBG_Pipe_Close("Pipe_Close: closing handle 0x%x\n", *(the_pipes + 1));
        if (CloseHandle(*(the_pipes + 1)) == 0) {
            OS_AL_ERR("Pipe_Close: CloseHandle: error %d\n", GetLastError());
            return (-1);
        }
    }

    DBG_Pipe_Close("Pipe_Close: free 0x%x\n", the_pipes);
    free(the_pipes);

    *pi = 0;

    return (0);
}

/******************************************************************************/
/* Pipe_Read                                                                  */
/*  0 on success                                                              */
/* -1 on failure                                                              */
/******************************************************************************/
int Pipe_Read(void* pi, char* buf, int* sz) {
    HANDLE* pipe_i;

    pipe_i = *((HANDLE*) pi);

    if (ReadFile(
                pipe_i,
                buf,
                4096,
                sz,
                NULL) == 0) {
        OS_AL_ERR("Pipe_Read: ReadFile: error %d\n", GetLastError());
        return (-1);
    }
    return (0);
}

/******************************************************************************/
/* Pipe_Write                                                                 */
/*  0 on success                                                              */
/* -1 on failure                                                              */
/******************************************************************************/
int Pipe_Write(void* pi, char* buf, int sz) {
    int written;
    HANDLE* pipe_o;

    pipe_o = *((HANDLE*) pi + 1);
    if (WriteFile(
                pipe_o,
                buf,
                sz,
                &written,
                NULL) == 0) {
        OS_AL_ERR("Pipe_Write: WriteFile: error %d\n", GetLastError());
        return (-1);
    }
    return (0);
}
