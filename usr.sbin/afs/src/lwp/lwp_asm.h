
/*
****************************************************************************
*        Copyright IBM Corporation 1988, 1989 - All Rights Reserved        *
*                                                                          *
* Permission to use, copy, modify, and distribute this software and its    *
* documentation for any purpose and without fee is hereby granted,         *
* provided that the above copyright notice appear in all copies and        *
* that both that copyright notice and this permission notice appear in     *
* supporting documentation, and that the name of IBM not be used in        *
* advertising or publicity pertaining to distribution of the software      *
* without specific, written prior permission.                              *
*                                                                          *
* IBM DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL *
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL IBM *
* BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY      *
* DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER  *
* IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING   *
* OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.    *
****************************************************************************
*/

/*******************************************************************\
* 								    *
* 	Information Technology Center				    *
* 	Carnegie-Mellon University				    *
* 								    *
* 								    *
\*******************************************************************/

/* $arla: lwp_asm.h,v 1.19 2002/06/01 17:47:48 lha Exp $ */ 

#ifndef __LWP_INCLUDE_
#define	__LWP_INCLUDE_	1
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#ifdef HAVE_POSIX_SIGNALS
#define AFS_POSIX_SIGNALS 1
#endif

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
#define LWP_MAX_PRIORITY 4	/* changed from 1 by Satya, 22 Nov. 85 */

/* Usual priority used by user LWPs */
#define LWP_NORMAL_PRIORITY (LWP_MAX_PRIORITY-2)

/* Initial size of eventlist in a PCB; grows dynamically  */ 
#define EVINITSIZE  5

typedef struct lwp_pcb *PROCESS;

struct lwp_context {	/* saved context for dispatcher */
    char *topstack;	/* ptr to top of process stack */
#ifdef sparc
#ifdef	save_allregs
#ifdef __sparcv9
#define nregs (7+1+62+2)
#else
#define nregs (7+1+32+2+32+2)     /* g1-g7, y reg, f0-f31, fsr, fq, c0-c31, csr, cq. */
#endif
    long globals[nregs];
#undef nregs
#else
    long globals[8];    /* g1-g7 and y registers. */
#endif
#endif
#if defined(powerpc) || defined(ppc) || defined(powerc)
    char *linkRegister;		/* the contents of the link register */
    long conditionRegister;	/* the contents of the condition register */
#endif /* defined(powerpc) || defined(ppc) || defined(powerc) */
};

struct rock
    {/* to hide things associated with this LWP under */
    int  tag;		/* unique identifier for this rock */
    char *value;	/* pointer to some arbitrary data structure */
    };

#define MAXROCKS	4	/* max no. of rocks per LWP */

struct lwp_pcb {			/* process control block */
  char		name[32];		/* ASCII name */
  int		rc;			/* most recent return code */
  char		status;			/* status flags */
  char		blockflag;		/* if (blockflag), process blocked */
  char		eventlistsize;		/* size of eventlist array */
  char          padding;                /* force 32-bit alignment */
  char		**eventlist;		/* ptr to array of eventids */
  int		eventcnt;		/* no. of events currently in eventlist array*/
  int		wakevent;		/* index of eventid causing wakeup */
  int		waitcnt;		/* min number of events awaited */
  int		priority;		/* dispatching priority */
  struct lwp_pcb *misc;			/* for LWP internal use only */
  char		*stack;			/* ptr to process stack */
  int		stacksize;		/* size of stack */
  long		stackcheck;		/* first word of stack for overflow checking */
  void		(*ep)();		/* initial entry point */
  char		*parm;			/* initial parm for process */
  struct lwp_context
		context;		/* saved context for next dispatch */
  int		rused;			/* no of rocks presently in use */
  struct rock	rlist[MAXROCKS];	/* set of rocks to hide things under */
  struct lwp_pcb *next, *prev;		/* ptrs to next and previous pcb */
  int		level;			/* nesting level of critical sections */
  struct IoRequest	*iomgrRequest;	/* request we're waiting for */
  int           index;                  /* LWP index: should be small index; actually is
                                           incremented on each lwp_create_process */ 
  };

extern int lwp_nextindex;                      /* Next lwp index to assign */


#ifndef LWP_KERNEL
#define LWP_INVALIDTHREADID		(-1)
#define LWP_ActiveProcess	(lwp_cpptr+0)
#define LWP_Index() (LWP_ActiveProcess->index)
#define LWP_HighestIndex() (lwp_nextindex - 1)
#define LWP_SignalProcess(event)	LWP_INTERNALSIGNAL(event, 1)
#define LWP_NoYieldSignal(event)	LWP_INTERNALSIGNAL(event, 0)

extern
#endif
  PROCESS lwp_cpptr;	/* pointer to current process pcb */

struct	 lwp_ctl {			/* LWP control structure */
    int		processcnt;		/* number of lightweight processes */
    char	*outersp;		/* outermost stack pointer */
    struct lwp_pcb *outerpid;		/* process carved by Initialize */
    struct lwp_pcb *first, last;	/* ptrs to first and last pcbs */
#ifdef __hp9000s800
    double	dsptchstack[100];	/* stack for dispatcher use only */
					/* force 8 byte alignment        */
#else
    char	dsptchstack[800];	/* stack for dispatcher use only */
#endif
};

#ifndef LWP_KERNEL
extern
#endif
	int lwp_debug;		/* ON = show LWP debugging trace */

#if defined(AFS_SUN5_ENV) || defined(AFS_LINUX_ENV)
#define AFS_POSIX_SIGNALS
#endif

/* 
 * Under some unices any stack size smaller than 16K seems prone to
 * overflow problems. Set it to a somewhat larger value.
 */

#define AFS_LWP_MINSTACKSIZE	(100 * 1024)

/* Action to take on stack overflow. */
#define LWP_SOQUIET	1		/* do nothing */
#define LWP_SOABORT	2		/* abort the program */
#define LWP_SOMESSAGE	3		/* print a message and be quiet */
extern int lwp_overflowAction;

/* Tells if stack size counting is enabled. */
extern int lwp_stackUseEnabled;
extern int lwp_MaxStackSeen;

struct timeval;

int LWP_QSignal(PROCESS);
int LWP_DispatchProcess(void);
int LWP_InitializeProcessSupport(int, PROCESS *);
int LWP_DestroyProcess(PROCESS);
int LWP_QWait(void);

/* exported interface */
int LWP_CreateProcess(void (*)(), int, int, char *, const char *, PROCESS *);
int LWP_CurrentProcess(PROCESS *);
int LWP_WaitProcess(void *);
int LWP_INTERNALSIGNAL(void *, int);
int LWP_DispatchProcess(void);
int LWP_GetProcessPriority(PROCESS pid, int *priority);
int LWP_TerminateProcessSupport(void);
int LWP_MwaitProcess(int wcount, char *evlist[]);
int LWP_StackUsed(PROCESS pid, int *max, int *used);

void IOMGR_Sleep(unsigned int);
int  IOMGR_Select(int, fd_set *, fd_set *, fd_set *, struct timeval *);
long IOMGR_Poll(void);
int  IOMGR_Cancel(PROCESS);
int  IOMGR_Initialize(void);
int  IOMGR_SoftSig(void (*aproc)(), char *arock);
int  IOMGR_Finalize(void);
int  IOMGR_Signal (int signo, char *event);
int  IOMGR_CancelSignal (int signo);

int LWP_NewRock(int Tag, char *Value);
int LWP_GetRock(int Tag, char **Value);


#endif /* __LWP_INCLUDE_ */
