/*
 * $arla: plwp.h,v 1.9 2003/01/03 16:26:08 tol Exp $
 */

#ifndef LWP_INCLUDED
#define LWP_INCLUDED

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_POSIX_SIGNALS
#define AFS_POSIX_SIGNALS 1
#endif
#define AFS_LWP_MINSTACKSIZE    (100 * 1024)
#include <sys/time.h>
#include <signal.h>

#ifdef PTHREADS_LWP
#include <pthread.h>
#endif

#include <roken.h>

#ifdef WINDOWS_THREADS_LWP
#include <windows.h>
#endif

#ifndef LWP_KERNEL
extern
#endif
	int lwp_debug;          /* ON = show LWP debugging trace */

extern int lwp_stackUseEnabled;
extern int lwp_MaxStackSeen;
#define LWP_ActiveProcess       (lwp_cpptr+0)

#define LWP_VERSION  210888001

#define LWP_SUCCESS	0
#define LWP_EBADPID	-1
#define LWP_EBLOCKED	-2
#define LWP_EINIT	-3
#define LWP_EMAXPROC	-4
#define LWP_ENOBLOCK	-5
#define LWP_ENOMEM	-6
#define LWP_ENOPROCESS	-7
#define LWP_ENOWAIT	-8
#define LWP_EBADCOUNT	-9
#define LWP_EBADEVENT	-10
#define LWP_EBADPRI	-11
#define LWP_NO_STACK	-12
/* These two are for the signal mechanism. */
#define LWP_EBADSIG	-13		/* bad signal number */
#define LWP_ESYSTEM	-14		/* system call failed */
/* These are for the rock mechanism */
#define LWP_ENOROCKS	-15	/* all rocks are in use */
#define LWP_EBADROCK	-16	/* the specified rock does not exist */


/* Maximum priority permissible (minimum is always 0) */
#define LWP_MAX_PRIORITY 4

/* Usual priority used by user LWPs */
#define LWP_NORMAL_PRIORITY (LWP_MAX_PRIORITY-1)

#define LWP_SignalProcess(event)	LWP_INTERNALSIGNAL(event, 1)
#define LWP_NoYieldSignal(event)	LWP_INTERNALSIGNAL(event, 0)

/* Users aren't really supposed to know what a pcb is, but .....*/
typedef struct lwp_pcb *PROCESS;

/* Action to take on stack overflow. */
#define LWP_SOQUIET	1		/* do nothing */
#define LWP_SOABORT	2		/* abort the program */
#define LWP_SOMESSAGE	3		/* print a message and be quiet */
extern int lwp_overflowAction;

/* Invalid LWP Index */
#define LWP_INVALIDTHREADID		(-1)

/* Tells if stack size counting is enabled. */
extern int lwp_stackUseEnabled;

int LWP_QWait(void);
int LWP_QSignal(PROCESS);
int LWP_Init(int, int, PROCESS *);
int LWP_InitializeProcessSupport(int, PROCESS *);
int LWP_TerminateProcessSupport();
int LWP_CreateProcess(void (*)(), int, int, char *, char *, PROCESS *);
int LWP_CurrentProcess(PROCESS *);
int LWP_DestroyProcess(PROCESS);
int LWP_DispatchProcess();
int LWP_GetProcessPriority(PROCESS, int *);
int LWP_INTERNALSIGNAL(void *, int);
int LWP_WaitProcess(void *);
int LWP_MwaitProcess(int, char **);
int LWP_StackUsed(PROCESS, int *, int *); 
int LWP_NewRock(int, char *);
int LWP_GetRock(int,  char **);
char *LWP_Name();
int LWP_Index();
int LWP_HighestIndex();

int IOMGR_SoftSig(void (*)(), char *);
int IOMGR_Initialize(void);
int IOMGR_Finalize(void);
long IOMGR_Poll(void);
int IOMGR_Select(int, fd_set *, fd_set *, fd_set *, struct timeval *);
int IOMGR_Cancel(PROCESS);
int IOMGR_Signal(int, char *);
int IOMGR_CancelSignal(int);
void IOMGR_Sleep(unsigned int);

int FT_Init(int, int);
#if	__GNUC__ >= 2
struct timezone;
#endif
int FT_GetTimeOfDay(struct timeval *, struct timezone *);
int TM_GetTimeOfDay(struct timeval *, struct timezone *);
int FT_AGetTimeOfDay(struct timeval *, struct timezone *);
unsigned int FT_ApproxTime(void);

/* Initial size of eventlist in a PCB; grows dynamically  */ 
#define EVINITSIZE  5

struct rock 
 {/* to hide things associated with this LWP under */
   int  tag;		/* unique identifier for this rock */
   /* pointer to some arbitrary data structure */
#if defined(PTHREADS_LWP)
   pthread_key_t val;
#elif defined(WINDOWS_THREADS_LWP)
   LPVOID val;
#endif
 };

#define MAXROCKS	4	/* max no. of rocks per LWP */

struct lwp_pcb {			/* process control block */
  char name[32];
  int rc;
  char status;      
  char **eventlist; 
  char eventlistsize; 
  int eventcnt;	   
  int wakevent;
  int waitcnt;	
  char blockflag;
  int priority;		
  PROCESS misc;		
  char *stack;		
  int stacksize;
  long stackcheck;
  void (*ep)(char *);
  char *parm;		
  int rused;		
  struct rock rlist[MAXROCKS];
  PROCESS next, prev;	
  int level;          
  struct IoRequest *iomgrRequest;
  int index;        
  struct timeval lastReady;
#if defined(PTHREADS_LWP)
  pthread_mutex_t m;       
  pthread_cond_t c;        
  pthread_attr_t a;        
#elif defined(WINDOWS_THREADS_LWP)
  HANDLE m;
  HANDLE c;
  HANDLE t;
#endif
};

extern int lwp_nextindex;                      /* Next lwp index to assign */

extern PROCESS	lwp_cpptr;		/* pointer to current process pcb */
struct	 lwp_ctl {			/* LWP control structure */
    int		processcnt;		/* number of lightweight processes */
    char	*outersp;		/* outermost stack pointer */
    PROCESS	outerpid;		/* process carved by Initialize */
    PROCESS	first, last;		/* ptrs to first and last pcbs */
    char	dsptchstack[800];	/* stack for dispatcher use only */
};

/* Debugging macro */
#ifdef LWPDEBUG
#define lwpdebug(level, msg)\
	 if (lwp_debug > level) {\
	     printf("***LWP (0x%x): ", lwp_cpptr);\
	     printf msg;\
	     putchar('\n');\
	     fflush(stdout);\
	 }
#else /* LWPDEBUG */
#define lwpdebug(level, msg)
#endif /* LWPDEBUG */

#define MAXTHREADS	100

#endif /* LWP_INCLUDED */



