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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

/* allocate externs here */
#define  LWP_KERNEL

#include <lwp.h>
#include "preempt.h"

RCSID("$arla: lwp_asm.c,v 1.27 2002/07/16 19:35:40 lha Exp $");

#ifdef	AFS_AIX32_ENV
#include <ulimit.h>
#include <sys/errno.h>
#include <sys/user.h>
#include <sys/pseg.h>
#include <sys/core.h>
#pragma alloca
#endif

#define  ON	    1
#define  OFF	    0
#define  TRUE	    1
#define  FALSE	    0
#define  READY		2
#define  WAITING		3
#define  DESTROYED	4
#define QWAITING		5
#define  MAXINT     (~(1<<((sizeof(int)*8)-1)))


/*
 * Make sure that alignment and saving of data is right
 */

#if defined(__alpha) || defined(__uxpv__) || defined(__sparcv9) || defined(__x86_64__) || defined(__m88k__)
#define REGSIZE 8
#else
#define REGSIZE 4
#endif

/*
 * Space before first stack frame expressed in registers. 
 * 
 * This should maybe be a ABI specific value defined somewhere else.
 */

#ifdef __hp9000s800
#define STACK_HEADROOM 16
#elif defined(__s390__)
#define STACK_HEADROOM 24
#elif defined(__m88k__)
#define STACK_HEADROOM (32 / REGSIZE)
#else
#define STACK_HEADROOM 5		
#endif

/* Debugging macro */
#ifdef DEBUG
#define Debug(level, msg)\
	 if (lwp_debug && lwp_debug >= level) {\
	     printf("***LWP (%p): ", lwp_cpptr);\
	     printf msg;\
	     putchar('\n');\
	 }

#else
#define Debug(level, msg)

#endif

/* Prototypes */
static void Abort_LWP(char *msg) ;
static void Dispatcher(void);
static void Create_Process_Part2(void);
static void purge_dead_pcbs(void) ;
static void Overflow_Complain (void) ;
static void Dispose_of_Dead_PCB (PROCESS cur) ;
static void Free_PCB(PROCESS pid) ;
static void Exit_LWP(void);
static void Initialize_PCB(PROCESS temp, int priority, char *stack,
			   int stacksize, void (*ep)() , char *parm, 
			   const char *name) ;
static long Initialize_Stack(char *stackptr,int stacksize) ;
static int Stack_Used(char *stackptr, int stacksize) ;
static int Internal_Signal(char *event) ;
char   (*RC_to_ASCII());

#define MAX_PRIORITIES	(LWP_MAX_PRIORITY+1)

struct QUEUE {
    PROCESS	head;
    int		count;
} runnable[MAX_PRIORITIES], blocked;
/* 
 * Invariant for runnable queues: The head of each queue points to the
 * currently running process if it is in that queue, or it points to the
 * next process in that queue that should run.
 */

/* Offset of stack field within pcb -- used by stack checking stuff */
int stack_offset;

/* special user-tweakable option for AIX */
int lwp_MaxStackSize = 32768;

/* biggest LWP stack created so far */
int lwp_MaxStackSeen = 0;

/* Stack checking action */
int lwp_overflowAction = LWP_SOABORT;

/* Controls stack size counting. */
int lwp_stackUseEnabled = TRUE;	/* pay the price */

int lwp_nextindex;

static void
lwp_remove(PROCESS p, struct QUEUE *q)
{
    /* Special test for only element on queue */
    if (q->count == 1)
	q -> head = NULL;
    else {
	/* Not only element, do normal remove */
	p -> next -> prev = p -> prev;
	p -> prev -> next = p -> next;
    }
    /* See if head pointing to this element */
    if (q->head == p) q -> head = p -> next;
    q->count--;
    p -> next = p -> prev = NULL;
}

static void
insert(PROCESS p, struct QUEUE *q)
{
    if (q->head == NULL) {	/* Queue is empty */
	q -> head = p;
	p -> next = p -> prev = p;
    } else {			/* Regular insert */
	p -> prev = q -> head -> prev;
	q -> head -> prev -> next = p;
	q -> head -> prev = p;
	p -> next = q -> head;
    }
    q->count++;
}

static void
move(PROCESS p, struct QUEUE *from, struct QUEUE *to)
{
    lwp_remove(p, from);

    insert(p, to);
}

/* Iterator macro */
#define for_all_elts(var, q, body)\
	{\
	    PROCESS var, _NEXT_;\
	    int _I_;\
	    for (_I_=q.count, var = q.head; _I_>0; _I_--, var=_NEXT_) {\
		_NEXT_ = var -> next;\
		body\
	    }\
	}

/*									    */
/*****************************************************************************\
* 									      *
*  Following section documents the Assembler interfaces used by LWP code      *
* 									      *
\*****************************************************************************/

/*
 *	savecontext(int (*ep)(), struct lwp_context *savearea, char *sp);
 *  XXX - the above prototype is a lie.
 * Stub for Assembler routine that will
 * save the current SP value in the passed
 * context savearea and call the function
 * whose entry point is in ep.  If the sp
 * parameter is NULL, the current stack is
 * used, otherwise sp becomes the new stack
 * pointer.
 *
 *      returnto(struct lwp_context *savearea);
 *
 * Stub for Assembler routine that will
 * restore context from a passed savearea
 * and return to the restored C frame.
 *
 */

void savecontext(void (*)(), struct lwp_context *, char *); 
void returnto(struct lwp_context *);

/* Macro to force a re-schedule.  Strange name is historical */
#define Set_LWP_RC() savecontext(Dispatcher, &lwp_cpptr->context, NULL)

static struct lwp_ctl *lwp_init = 0;

int 
LWP_QWait(void)
{
    PROCESS tp;
    (tp=lwp_cpptr) -> status = QWAITING;
    lwp_remove(tp, &runnable[tp->priority]);
    Set_LWP_RC();
    return LWP_SUCCESS;
}

int
LWP_QSignal(PROCESS pid)
{
    if (pid->status == QWAITING) {
	pid->status = READY;
	insert(pid, &runnable[pid->priority]);
	return LWP_SUCCESS;
    }
    else return LWP_ENOWAIT;
}

#ifdef	AFS_AIX32_ENV
char *
reserveFromStack(size) 
    long size;
{
    char *x;
    x = alloca(size);
    return x;
}
#endif

#if defined(LWP_REDZONE) && defined(HAVE_MMAP)

/*
 * Redzone protection of stack
 *
 * We protect one page before and one after the stack to make sure
 * none over/under runs the stack. The size of the stack is saved one
 * the first page together with a magic number to make sure we free
 * the right pages.
 *
 * If the operating system doesn't support mmap, turn redzone off in
 * the autoconf glue.
 */

#define P_SIZE_OFFSET	16
#define P_MAGIC		0x7442e938

static void *
lwp_stackmalloc(size_t size)
{
    char *p, *p_after, *p_before;
    size_t pagesize = getpagesize();
    int pages = (size - 1) / pagesize + 1;
    int fd = -1;

#ifndef MAP_ANON
#define MAP_ANON 0
#ifndef _PATH_DEV_ZERO
#define _PATH_DEV_ZERO "/dev/zero"
#endif
    fd = open(_PATH_DEV_ZERO, O_RDWR, 0644);
#endif

    p = mmap(0, (pages + 2) * pagesize, PROT_READ | PROT_WRITE,
	     MAP_PRIVATE | MAP_ANON, fd, 0);
    if (p == MAP_FAILED) {
	perror("mmap");
	exit(-1);
    }

    p_before = p;
    p += pagesize;
    p_after = p + pages * pagesize;

    /* store the magic and the length in the first page */

    *((unsigned long *)p_before) = P_MAGIC;
    *((unsigned long *)(p_before + P_SIZE_OFFSET)) = (pages + 2) * pagesize;

    /* protect pages */

    if (mprotect(p_before, pagesize, PROT_NONE) < 0) {
	perror("mprotect before");
	exit(-1);
    }
    if (mprotect(p_after, pagesize, PROT_NONE) < 0) {
	perror("mprotect after");
	exit(-1);
    }
    return p;
}

static void
lwp_stackfree(void *ptr, size_t len)
{
    size_t pagesize = getpagesize();
    char *realptr;
    unsigned long magic;
    size_t length;

    if (((size_t)ptr) % pagesize != 0)
	exit(-1);

    realptr = ((char *)ptr) - pagesize;

    if (mprotect(realptr, pagesize, PROT_READ) < 0) {
	perror("mprotect");
	exit(-1);
    }

    magic = *((unsigned long *)realptr);
    if (magic != P_MAGIC)
	exit(-1);
    length = *((unsigned long *)(realptr + P_SIZE_OFFSET));
    if (len != length - 2 * pagesize)
	exit(-1);

    if (munmap(realptr, length) < 0) {
	perror("munmap");
	exit(1);
    }
}

#else

static void *
lwp_stackmalloc(size_t size)
{
    return malloc(size);
}

static void
lwp_stackfree(void *ptr, size_t len)
{
    free(ptr);
}
#endif

int
LWP_CreateProcess(void (*ep)(), int stacksize, int priority,
		  char *parm, const char *name, PROCESS *pid)
{
    PROCESS temp, temp2;
#ifdef	AFS_AIX32_ENV
    static char *stackptr = 0;
#else
    char *stackptr;
#endif

    /*
     * on some systems (e.g. hpux), a minimum usable stack size has
     * been discovered
     */
    if (stacksize < AFS_LWP_MINSTACKSIZE)
	stacksize = AFS_LWP_MINSTACKSIZE;

    /* more stack size computations; keep track of for IOMGR */
    if (lwp_MaxStackSeen < stacksize)
	lwp_MaxStackSeen = stacksize;

    Debug(0, ("Entered LWP_CreateProcess"))
    /* Throw away all dead process control blocks */
    purge_dead_pcbs();
    if (!lwp_init)
	return LWP_EINIT;


    temp = (PROCESS) malloc(sizeof(struct lwp_pcb));
    if (temp == NULL) {
	Set_LWP_RC();
	return LWP_ENOMEM;
    }
    
    /* align stacksize */
    stacksize = REGSIZE * ((stacksize+REGSIZE-1) / REGSIZE);
    
#ifdef	AFS_AIX32_ENV
    if (!stackptr) {
	/*
	 * The following signal action for AIX is necessary so that in case of a 
	 * crash (i.e. core is generated) we can include the user's data section 
	 * in the core dump. Unfortunately, by default, only a partial core is
	 * generated which, in many cases, isn't too useful.
	 *
	 * We also do it here in case the main program forgets to do it.
	 */
	struct sigaction nsa;
	extern int geteuid();
	
	sigemptyset(&nsa.sa_mask);
	nsa.sa_handler = SIG_DFL;
	nsa.sa_flags = SA_FULLDUMP;
	sigaction(SIGSEGV, &nsa, NULL);
	
	/*
	 * First we need to increase the default resource limits,
	 * if necessary, so that we can guarantee that we have the
	 * resources to create the core file, but we can't always 
	 * do it as an ordinary user.
	 */
	if (!geteuid()) {
	    setlim(RLIMIT_FSIZE, 0, 1048575);	/* 1 Gig */
	    setlim(RLIMIT_STACK, 0, 65536);	/* 65 Meg */
	    setlim(RLIMIT_CORE, 0, 131072);	/* 131 Meg */
	}
	/*
	 * Now reserve in one scoop all the stack space that will be used
	 * by the particular application's main (i.e. non-lwp) body. This
	 * is plenty space for any of our applications.
	 */
	stackptr = reserveFromStack(lwp_MaxStackSize);
    }
    stackptr -= stacksize;
#else /* !AFS_AIX32_ENV */
    if ((stackptr = (char *) lwp_stackmalloc(stacksize)) == NULL) {
	Set_LWP_RC();
	return LWP_ENOMEM;
    }
#endif /* AFS_AIX32_ENV */
    if (priority < 0 || priority >= MAX_PRIORITIES) {
	Set_LWP_RC();
	return LWP_EBADPRI;
    }
    Initialize_Stack(stackptr, stacksize);
    Initialize_PCB(temp, priority, stackptr, stacksize, ep, parm, name);
    insert(temp, &runnable[priority]);
    temp2 = lwp_cpptr;
    
    if (PRE_Block != 0)
	Abort_LWP("PRE_Block not 0");
    
    /* Gross hack: beware! */
    PRE_Block = 1;
    lwp_cpptr = temp;
#ifdef __hp9000s800
    savecontext(Create_Process_Part2, &temp2->context,
		stackptr + (REGSIZE * STACK_HEADROOM));
#else
    savecontext(Create_Process_Part2, &temp2->context,
		stackptr + stacksize - (REGSIZE * STACK_HEADROOM));
#endif
    /* End of gross hack */
    
    Set_LWP_RC();
    *pid = temp;
    return 0;
}

/* returns pid of current process */
int
LWP_CurrentProcess(PROCESS *pid)
{
    Debug(0, ("Entered Current_Process"))
    if (lwp_init) {
	    *pid = lwp_cpptr;
	    return LWP_SUCCESS;
    } else
	return LWP_EINIT;
}

#define LWPANCHOR (*lwp_init)

/* destroy a lightweight process */
int
LWP_DestroyProcess(PROCESS pid)
{
    PROCESS temp;

    Debug(0, ("Entered Destroy_Process"))
    if (lwp_init) {
	if (lwp_cpptr != pid) {
	    Dispose_of_Dead_PCB(pid);
	    Set_LWP_RC();
	} else {
	    pid -> status = DESTROYED;
	    move(pid, &runnable[pid->priority], &blocked);
	    temp = lwp_cpptr;
#ifdef __hp9000s800
	    savecontext(Dispatcher, &(temp -> context),
			&(LWPANCHOR.dsptchstack[(REGSIZE * STACK_HEADROOM)]));
#else
	    savecontext(Dispatcher, &(temp -> context),
			&(LWPANCHOR.dsptchstack[(sizeof LWPANCHOR.dsptchstack)
					       - (REGSIZE * STACK_HEADROOM)]));
#endif
	}
	return LWP_SUCCESS;
    } else
	return LWP_EINIT;
}

/* explicit voluntary preemption */
int 
LWP_DispatchProcess(void)
{
    Debug(2, ("Entered Dispatch_Process"))
    if (lwp_init) {
	Set_LWP_RC();
	return LWP_SUCCESS;
    } else
	return LWP_EINIT;
}

#ifdef DEBUG
static void Dump_One_Process(PROCESS pid);

static void
Dump_Processes(void)
{
    if (lwp_init) {
	int i;
	for (i=0; i<MAX_PRIORITIES; i++)
	    for_all_elts(x, runnable[i], {
		printf("[Priority %d]\n", i);
		Dump_One_Process(x);
	    })
	for_all_elts(x, blocked, { Dump_One_Process(x); })
    } else
	printf("***LWP: LWP support not initialized\n");
}
#endif

/* returns process priority */
int
LWP_GetProcessPriority(PROCESS pid, int *priority)
{
    Debug(0, ("Entered Get_Process_Priority"))
    if (lwp_init) {
	*priority = pid -> priority;
	return 0;
    } else
	return LWP_EINIT;
}

int
LWP_InitializeProcessSupport(int priority, PROCESS *pid)
{
    PROCESS temp;
    struct lwp_pcb dummy;
    int i;

    Debug(0, ("Entered LWP_InitializeProcessSupport"))
    if (lwp_init != NULL) return LWP_SUCCESS;

    /* Set up offset for stack checking -- do this as soon as possible */
    stack_offset = (char *) &dummy.stack - (char *) &dummy;

    if (priority >= MAX_PRIORITIES) return LWP_EBADPRI;
    for (i=0; i<MAX_PRIORITIES; i++) {
	runnable[i].head = NULL;
	runnable[i].count = 0;
    }
    blocked.head = NULL;
    blocked.count = 0;
    lwp_init = (struct lwp_ctl *) malloc(sizeof(struct lwp_ctl));
    temp = (PROCESS) malloc(sizeof(struct lwp_pcb));
    if (lwp_init == NULL || temp == NULL)
	Abort_LWP("Insufficient Storage to Initialize LWP Support");
    LWPANCHOR.processcnt = 1;
    LWPANCHOR.outerpid = temp;
    LWPANCHOR.outersp = NULL;
    Initialize_PCB(temp, priority, NULL, 0, NULL, NULL,
		   "Main Process [created by LWP]");
    insert(temp, &runnable[priority]);
    savecontext(Dispatcher, &temp->context, NULL);
    LWPANCHOR.outersp = temp -> context.topstack;
    Set_LWP_RC();
    *pid = temp;
    return LWP_SUCCESS;
}

/* signal the occurence of an event */
int
LWP_INTERNALSIGNAL(void *event, int yield)
{
    Debug(2, ("Entered LWP_SignalProcess"))
    if (lwp_init) {
	int rc;
	rc = Internal_Signal(event);
	if (yield) Set_LWP_RC();
	return rc;
    } else
	return LWP_EINIT;
}

/* terminate all LWP support */
int
LWP_TerminateProcessSupport(void)
{
    int i;

    Debug(0, ("Entered Terminate_Process_Support"))
    if (lwp_init == NULL) return LWP_EINIT;
    if (lwp_cpptr != LWPANCHOR.outerpid)
	Abort_LWP("Terminate_Process_Support invoked from wrong process!");
    for (i=0; i<MAX_PRIORITIES; i++)
	for_all_elts(cur, runnable[i], { Free_PCB(cur); })
    for_all_elts(cur, blocked, { Free_PCB(cur); })
    free(lwp_init);
    lwp_init = NULL;
    return LWP_SUCCESS;
}

/* wait on m of n events */
int
LWP_MwaitProcess(int wcount, char *evlist[])
{
    int ecount, i;


    Debug(0, ("Entered Mwait_Process [waitcnt = %d]", wcount))

    if (evlist == NULL) {
	Set_LWP_RC();
	return LWP_EBADCOUNT;
    }

    for (ecount = 0; evlist[ecount] != NULL; ecount++) ;

    if (ecount == 0) {
	Set_LWP_RC();
	return LWP_EBADCOUNT;
    }

    if (lwp_init) {

	if (wcount>ecount || wcount<0) {
	    Set_LWP_RC();
	    return LWP_EBADCOUNT;
	}
	if (ecount > lwp_cpptr->eventlistsize) {

	    lwp_cpptr->eventlist = (char **)realloc(lwp_cpptr->eventlist,
						    ecount*sizeof(char *));
	    lwp_cpptr->eventlistsize = ecount;
	}
	for (i=0; i<ecount; i++) lwp_cpptr -> eventlist[i] = evlist[i];
	if (wcount > 0) {
	    lwp_cpptr -> status = WAITING;

	    move(lwp_cpptr, &runnable[lwp_cpptr->priority], &blocked);
	}
	lwp_cpptr -> wakevent = 0;
	lwp_cpptr -> waitcnt = wcount;
	lwp_cpptr -> eventcnt = ecount;

	Set_LWP_RC();

	return LWP_SUCCESS;
    }

    return LWP_EINIT;
}

/* wait on a single event */
int
LWP_WaitProcess(void *event)
{
    char *tempev[2];

    Debug(2, ("Entered Wait_Process"))
    if (event == NULL) return LWP_EBADEVENT;
    tempev[0] = event;
    tempev[1] = NULL;
    return LWP_MwaitProcess(1, tempev);
}

int
LWP_StackUsed(PROCESS pid, int *max, int *used)
{
    *max = pid -> stacksize;
    *used = Stack_Used(pid->stack, *max);
    if (*used == 0)
	return LWP_NO_STACK;
    return LWP_SUCCESS;
}

/*
 *  The following functions are strictly
 *  INTERNAL to the LWP support package.
 */

static void
Abort_LWP(char *msg)
{
    struct lwp_context tempcontext;

    Debug(0, ("Entered Abort_LWP"))
    printf("***LWP: %s\n",msg);
    printf("***LWP: Abort --- dumping PCBs ...\n");
#ifdef DEBUG
    Dump_Processes();
#endif
    if (LWPANCHOR.outersp == NULL)
	Exit_LWP();
    else
	savecontext(Exit_LWP, &tempcontext, LWPANCHOR.outersp);
}

/* creates a context for the new process */
static void
Create_Process_Part2(void)
{
    PROCESS temp;

    Debug(2, ("Entered Create_Process_Part2"))
    temp = lwp_cpptr;		/* Get current process id */
    savecontext(Dispatcher, &temp->context, NULL);
    (*temp->ep)(temp->parm);
    LWP_DestroyProcess(temp);
}

/* remove a PCB from the process list */
static void
Delete_PCB(PROCESS pid)
{
    Debug(4, ("Entered Delete_PCB"))
    lwp_remove(pid, (pid->blockflag || 
		     pid->status==WAITING ||
		     pid->status==DESTROYED
		     ? &blocked
		     : &runnable[pid->priority]));
    LWPANCHOR.processcnt--;
}

#ifdef DEBUG
static void
Dump_One_Process(PROCESS pid)
{
    int i;

    printf("***LWP: Process Control Block at %p\n", pid);
    printf("***LWP: Name: %s\n", pid->name);
    if (pid->ep != NULL)
	printf("***LWP: Initial entry point: %p\n", pid->ep);
    if (pid->blockflag) printf("BLOCKED and ");
    switch (pid->status) {
	case READY:	printf("READY");     break;
	case WAITING:	printf("WAITING");   break;
	case DESTROYED:	printf("DESTROYED"); break;
	default:	printf("unknown");
    }
    putchar('\n');
    printf("***LWP: Priority: %d \tInitial parameter: %p\n",
	    pid->priority, pid->parm);
    if (pid->stacksize != 0) {
	printf("***LWP:  Stacksize: %d \tStack base address: %p\n",
		pid->stacksize, pid->stack);
	printf("***LWP: HWM stack usage: ");
	printf("%d\n", Stack_Used(pid->stack,pid->stacksize));
    }
    printf("***LWP: Current Stack Pointer: %p\n", pid->context.topstack);
    if (pid->eventcnt > 0) {
	printf("***LWP: Number of events outstanding: %d\n", pid->waitcnt);
	printf("***LWP: Event id list:");
	for (i=0;i<pid->eventcnt;i++)
	    printf(" %p", pid->eventlist[i]);
	putchar('\n');
    }
    if (pid->wakevent>0)
	printf("***LWP: Number of last wakeup event: %d\n", pid->wakevent);
}
#endif

static void
purge_dead_pcbs(void)
{
    for_all_elts(cur, blocked, { 
	if (cur->status == DESTROYED) Dispose_of_Dead_PCB(cur);
    })
}

int LWP_TraceProcesses = 0;

/* Lightweight process dispatcher */
static void
Dispatcher(void)
{
    int i;
#ifdef DEBUG
    static int dispatch_count = 0;

    if (LWP_TraceProcesses > 0) {
	for (i=0; i<MAX_PRIORITIES; i++) {
	    printf("[Priority %d, runnable (%d):", i, runnable[i].count);
	    for_all_elts(p, runnable[i], {
		printf(" \"%s\"", p->name);
	    })
	    puts("]");
	}
	printf("[Blocked (%d):", blocked.count);
	for_all_elts(p, blocked, {
	    printf(" \"%s\"", p->name);
	})
	puts("]");
    }
#endif

    /* 
     * Check for stack overflow if this lwp has a stack.  Check for
     * the guard word at the front of the stack being damaged and
     * for the stack pointer being below the front of the stack.
     * WARNING!  This code assumes that stacks grow downward.
     */
#ifdef __hp9000s800
    /* Fix this (stackcheck at other end of stack???) */
    if (lwp_cpptr != NULL && lwp_cpptr->stack != NULL
	    && (lwp_cpptr->stackcheck != 
	       *(long *)((lwp_cpptr->stack) + lwp_cpptr->stacksize - 4)
		|| lwp_cpptr->context.topstack > 
		   lwp_cpptr->stack + lwp_cpptr->stacksize - 4)) {
#else
    if (lwp_cpptr != NULL && lwp_cpptr->stack != NULL
	    && (lwp_cpptr->stackcheck != *(long *)(lwp_cpptr->stack)
		|| lwp_cpptr->context.topstack < lwp_cpptr->stack)) {
#endif

        printf("stackcheck = %lul: stack = %lul\n",
	       lwp_cpptr->stackcheck, 
	       *(long *)lwp_cpptr->stack);
	printf("topstack = %lul\n", *(long *)lwp_cpptr->context.topstack);

	switch (lwp_overflowAction) {
	    case LWP_SOQUIET:
		break;
	    case LWP_SOABORT:
		Overflow_Complain();
		abort ();
	    case LWP_SOMESSAGE:
	    default:
		Overflow_Complain();
		lwp_overflowAction = LWP_SOQUIET;
		break;
	}
    }


    /* 
     * Move head of current runnable queue forward if current LWP 
     * is still in it. 
     */
    if (lwp_cpptr != NULL && lwp_cpptr == runnable[lwp_cpptr->priority].head)
	runnable[lwp_cpptr->priority].head = runnable[lwp_cpptr->priority].head->next;

    /* Find highest priority with runnable processes. */
    for (i = MAX_PRIORITIES - 1; i >= 0; i--)
	if (runnable[i].head != NULL)
	    break;

    if (i < 0)
	Abort_LWP("No READY processes");

#ifdef DEBUG
    if (LWP_TraceProcesses > 0)
	printf("Dispatch %d [PCB at %p] \"%s\"\n", 
	       ++dispatch_count,
	       runnable[i].head,
	       runnable[i].head->name);
#endif
    if (PRE_Block != 1) Abort_LWP("PRE_Block not 1");
    lwp_cpptr = runnable[i].head;

    returnto(&lwp_cpptr->context);
}

/* Complain of a stack overflow to stderr without using stdio. */
static void
Overflow_Complain (void)
{
    static char msg1[] = "LWP: stack overflow in process ";
    static char msg2[] = "!\n";

    write (2, msg1, sizeof(msg1) - 1);
    write (2, lwp_cpptr->name, strlen(lwp_cpptr->name));
    write (2, msg2, sizeof(msg2) - 1);
}

static void
Dispose_of_Dead_PCB (PROCESS cur)
{
    Debug(4, ("Entered Dispose_of_Dead_PCB"))
    Delete_PCB(cur);
    Free_PCB(cur);
/*
    Internal_Signal(cur);
*/
}

static void
Exit_LWP(void)
{
    exit(-1);
}

static void
Free_PCB(PROCESS pid)
{
    Debug(4, ("Entered Free_PCB"))
    if (pid -> stack != NULL) {
	Debug(0, ("HWM stack usage: %d, [PCB at %p]",
		   Stack_Used(pid->stack,pid->stacksize), pid))
	lwp_stackfree(pid -> stack, pid->stacksize);
    }
    if (pid->eventlist != NULL)  free(pid->eventlist);
    free(pid);
}

static void
Initialize_PCB(PROCESS temp, int priority, char *stack, int stacksize, 
	       void (*ep)(), char *parm, const char *name)
{
    Debug(4, ("Entered Initialize_PCB"))
    if (name != NULL) {
	strncpy(temp -> name, name, sizeof(temp -> name));
	temp -> name[sizeof(temp -> name) - 1] = '\0';
    } else
	temp -> name[0] = '\0';
    temp -> status = READY;
    temp -> eventlist = (char **)malloc(EVINITSIZE*sizeof(char *));
    temp -> eventlistsize = EVINITSIZE;
    temp -> eventcnt = 0;
    temp -> wakevent = 0;
    temp -> waitcnt = 0;
    temp -> blockflag = 0;
    temp -> iomgrRequest = 0;
    temp -> priority = priority;
    temp -> index = lwp_nextindex++;
    temp -> stack = stack;
    temp -> stacksize = stacksize;
#ifdef __hp9000s800
    if (temp -> stack != NULL)
	temp -> stackcheck = *(long *) ((temp -> stack) + stacksize - REGSIZE);
#else
    if (temp -> stack != NULL)
	temp -> stackcheck = *(long *) (temp -> stack);
#endif
    temp -> ep = ep;
    temp -> parm = parm;
    temp -> misc = NULL;	/* currently unused */
    temp -> next = NULL;
    temp -> prev = NULL;
    temp -> rused = 0;
    temp -> level = 1;		/* non-preemptable */
}

static int
Internal_Signal(char *event)
{
    int rc = LWP_ENOWAIT;
    int i;

    Debug(0, ("Entered Internal_Signal [event id %p]", event))
    if (!lwp_init) return LWP_EINIT;
    if (event == NULL) return LWP_EBADEVENT;
    for_all_elts(temp, blocked, {
	if (temp->status == WAITING)
	    for (i=0; i < temp->eventcnt; i++) {
		if (temp -> eventlist[i] == event) {
		    temp -> eventlist[i] = NULL;
		    rc = LWP_SUCCESS;
		    Debug(0, ("Signal satisfied for PCB %p", temp))
		    if (--temp->waitcnt == 0) {
			temp -> status = READY;
			temp -> wakevent = i+1;
			move(temp, &blocked, &runnable[temp->priority]);
			break;
		    }
		}
	    }
    })
    return rc;
}

/* This can be any unlikely pattern except 0x00010203 or the reverse. */
#define STACKMAGIC	0xBADBADBA
static long
Initialize_Stack(char *stackptr, int stacksize)
{
    int i;

    Debug(4, ("Entered Initialize_Stack"))
    if (lwp_stackUseEnabled)
	for (i=0; i<stacksize; i++)
	    stackptr[i] = i &0xff;
    else
#ifdef __hp9000s800
	*(long *)(stackptr + stacksize - 4) = STACKMAGIC;
#else
	*(long *)stackptr = STACKMAGIC;
#endif
    return 0; /* XXX - added. No clue what it should be */
}

static int
Stack_Used(char *stackptr, int stacksize)
{
    int    i;

#ifdef __hp9000s800
    if (*(long *) (stackptr + stacksize - 4) == STACKMAGIC)
	return 0;
    else {
	for (i = stacksize - 1; i >= 0 ; i--)
	    if ((unsigned char) stackptr[i] != (i & 0xff))
		return (i);
	return 0;
    }
#else
    if (*(long *) stackptr == STACKMAGIC)
	return 0;
    else {
	for (i = 0; i < stacksize; i++)
	    if ((unsigned char) stackptr[i] != (i & 0xff))
		return (stacksize - i);
	return 0;
    }
#endif
}


/* 
 * Finds a free rock and sets its value to Value.
 * Return codes:
 *	LWP_SUCCESS	Rock did not exist and a new one was used
 *	LWP_EBADROCK	Rock already exists.
 *	LWP_ENOROCKS	All rocks are in use.

 * From the above semantics, you can only set a rock value once. 
 * This is specificallY to prevent multiple users of the LWP package from
 * accidentally using the same Tag value and clobbering others. You can always
 *  use one level of indirection to obtain a rock whose contents can change.
 */

int
LWP_NewRock(int Tag, char *Value)
{
    int i;
    struct rock *ra;	/* rock array */
    
    ra = lwp_cpptr->rlist;

    for (i = 0; i < lwp_cpptr->rused; i++)
	if (ra[i].tag == Tag) return(LWP_EBADROCK);

    if (lwp_cpptr->rused < MAXROCKS)
	{
	ra[lwp_cpptr->rused].tag = Tag;
	ra[lwp_cpptr->rused].value = Value;
	lwp_cpptr->rused++;
	return(LWP_SUCCESS);
	}
    else return(LWP_ENOROCKS);
}

/* 
 * Obtains the pointer Value associated with the rock Tag of this LWP.
 * Returns:
 *    LWP_SUCCESS	if specified rock exists and Value has been filled
 *    LWP_EBADROCK	rock specified does not exist
 */
int
LWP_GetRock(int Tag, char **Value)
{
    int i;
    struct rock *ra;
    
    ra = lwp_cpptr->rlist;
    
    for (i = 0; i < lwp_cpptr->rused; i++) {
	if (ra[i].tag == Tag) {
	    *Value =  ra[i].value;
	    return(LWP_SUCCESS);
	}
    }
    return(LWP_EBADROCK);
}


#ifdef	AFS_AIX32_ENV
setlim(limcon, hard, limit)
    int limcon;
    uchar_t hard;
{
    struct rlimit rlim;

    (void) getrlimit(limcon, &rlim);
         
    limit = limit * 1024;
    if (hard)
	rlim.rlim_max = limit;
    else if (limit == RLIM_INFINITY && geteuid() != 0)
	rlim.rlim_cur = rlim.rlim_max;
    else
	rlim.rlim_cur = limit;

    /* Must use ulimit() due to Posix constraints */
    if (limcon == RLIMIT_FSIZE) {				 
	if (ulimit(UL_SETFSIZE, ((hard ? rlim.rlim_max : rlim.rlim_cur) / 512)) < 0) {
	    printf("Can't %s%s limit\n",
		   limit == RLIM_INFINITY ? "remove" : "set", hard ? " hard" : "");
	    return (-1);
	}
    } else {
	if (setrlimit(limcon, &rlim) < 0) {
	    perror("");
	    printf("Can't %s%s limit\n",
		   limit == RLIM_INFINITY ? "remove" : "set", hard ? " hard" : "");
	    return (-1);
	}
    }
    return (0);
}


#ifdef	notdef
/*
 * Print the specific limit out
 */
plim(name, lc, hard)
    char *name;
    long lc;
    uchar_t hard;
{
    struct rlimit rlim;
    int lim;

    printf("%s \t", name);
    (void) getrlimit(lc, &rlim);
    lim = hard ? rlim.rlim_max : rlim.rlim_cur;
    if (lim == RLIM_INFINITY)
	printf("unlimited");
    printf("%d %s", lim / 1024, "kbytes");
    printf("\n");
}
#endif
#endif
