/*	$OpenBSD: plwp.c,v 1.1 1999/04/30 01:59:13 art Exp $	*/
/* This version of the code is derived from the Coda version of the LWP
 * library, which can be compiled as a wrapper around Mach cthreads
 */
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* allocate externs here */
#define  LWP_KERNEL

#include <lwp.h>

RCSID("$KTH: plwp.c,v 1.2 1999/02/02 00:47:16 assar Exp $");

#ifdef	AFS_AIX32_ENV
#include <ulimit.h>
#include <sys/errno.h>
#include <sys/user.h>
#include <sys/pseg.h>
#include <sys/core.h>
#pragma alloca
#endif

#ifndef MAX
#define MAX(a,b) (((a)>(b))?(a):(b))
#endif

extern char PRE_Block;	/* from preempt.c */

#define  ON	    1
#define  OFF	    0
#define  TRUE	    1
#define  FALSE	    0
#define  READY		2
#define  WAITING		3
#define  DESTROYED	4
#define QWAITING		5
#define  MAXINT     (~(1<<((sizeof(int)*8)-1)))
#define  MINSTACK   44


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
static void Create_Process_Part2(PROCESS temp);
static void purge_dead_pcbs(void) ;
static void Dispose_of_Dead_PCB (PROCESS cur) ;
static void Free_PCB(PROCESS pid) ;
static void Exit_LWP();
static void Initialize_PCB(PROCESS temp, int priority, char *stack,
			   int stacksize, void (*ep)() , char *parm, 
			   char *name) ;
static int Internal_Signal(register char *event) ;
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

/* special user-tweakable option for AIX */
int lwp_MaxStackSize = 32768;
int lwp_MaxStackSeen = 0;

int lwp_nextindex = 0;

/* Iterator macro */
#define for_all_elts(var, q, body)\
	{\
	    register PROCESS var, _NEXT_;\
	    register int _I_;\
	    for (_I_=q.count, var = q.head; _I_>0; _I_--, var=_NEXT_) {\
		_NEXT_ = var -> next;\
		body\
	    }\
	}

static struct lwp_ctl *lwp_init = 0;

static void Dump_One_Process(PROCESS pid);
static void Dump_Processes();
static void Delete_PCB(register PROCESS pid);
static void Free_PCB(PROCESS pid);
static void Cal_Highest_runnable_priority();
static int InitializeProcessSupport(int, PROCESS *);

PROCESS	lwp_cpptr;

/* The global Highest_runnable_priority is only needed in NEW lwp.
    But it gets set within a for_all_elts() instance in
    InternalSignal(). */
int Highest_runnable_priority;	/* global variable for max priority */

int Proc_Running; /* indicates forked proc got control */

pthread_mutex_t run_sem, ct_mutex;

static void
lwpremove(register PROCESS p, register struct QUEUE *q)
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
    Debug(0, ("removing, count now %d", q->count));
    p -> next = p -> prev = NULL;
}

static void
lwpinsert(register PROCESS p, register struct QUEUE *q)
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
    Debug(0, ("inserting, count now %d", q->count));
}

static void
lwpmove(PROCESS p, struct QUEUE *from, struct QUEUE *to)
{
    lwpremove(p, from);
    lwpinsert(p, to);
}

int 
LWP_TerminateProcessSupport()       /* terminate all LWP support */
{
    register int i;

    Debug(0, ("Entered Terminate_Process_Support"));
    if (lwp_init == NULL) return LWP_EINIT;
    /* free all space allocated */
    for (i=0; i<MAX_PRIORITIES; i++)
        for_all_elts(cur, runnable[i], { Free_PCB(cur);})
    for_all_elts(cur, blocked, { Free_PCB(cur);})
    free((char *)lwp_init);
    lwp_init = NULL;
    return LWP_SUCCESS;
}

int 
LWP_GetRock(int Tag, char **Value)
{
    /* Obtains the pointer Value associated with the rock Tag of this LWP.
       Returns:
            LWP_SUCCESS    if specified rock exists and Value has been filled
            LWP_EBADROCK   rock specified does not exist
    */
    register int i;
    register struct rock *ra;

    ra = lwp_cpptr->rlist;

    for (i = 0; i < lwp_cpptr->rused; i++)
        if (ra[i].tag == Tag) {
#ifdef PTHREAD_GETSPECIFIC_TWOARG
	  pthread_getspecific(ra[i].val, Value);
#else
	  *Value = pthread_getspecific(ra[i].val);
#endif	  
	  /**Value =  ra[i].value;*/
	  return(LWP_SUCCESS);
	}
    return(LWP_EBADROCK);
}


int 
LWP_NewRock(int Tag, char *Value)
{
    /* Finds a free rock and sets its value to Value.
        Return codes:
                LWP_SUCCESS     Rock did not exist and a new one was used
                LWP_EBADROCK    Rock already exists.
                LWP_ENOROCKS    All rocks are in use.

        From the above semantics, you can only set a rock value once.
        This is specifically to prevent multiple users of the LWP
        package from accidentally using the same Tag value and
        clobbering others.  You can always use one level of
        indirection to obtain a rock whose contents can change.  */
    
    register int i;
    register struct rock *ra;   /* rock array */

    ra = lwp_cpptr->rlist;

    /* check if rock has been used before */
    for (i = 0; i < lwp_cpptr->rused; i++)
        if (ra[i].tag == Tag) return(LWP_EBADROCK);

    /* insert new rock in rock list and increment count of rocks */
    if (lwp_cpptr->rused < MAXROCKS) {
        ra[lwp_cpptr->rused].tag = Tag;
#ifdef HAVE_PTHREAD_KEYCREATE	
        if (pthread_keycreate(&ra[lwp_cpptr->rused].val, NULL))
#else
        if (pthread_key_create(&ra[lwp_cpptr->rused].val, NULL))
#endif
	  return(LWP_EBADROCK);
	if (pthread_setspecific(ra[lwp_cpptr->rused].val, Value))
	  return(LWP_EBADROCK);
        /* ra[lwp_cpptr->rused].value = Value; */
        lwp_cpptr->rused++;
        return(LWP_SUCCESS);
    }
    else return(LWP_ENOROCKS);
}

static void 
Dispose_of_Dead_PCB(PROCESS cur)
{

  Debug(0, ("Entered Dispose_of_Dead_PCB"));
  Delete_PCB(cur);
  Free_PCB(cur);
}

int 
LWP_CurrentProcess(PROCESS *pid)
{
    Debug(0, ("Entered Current_Process"));
    if (lwp_init) {
            *pid = lwp_cpptr;
            return LWP_SUCCESS;
    } else
        return LWP_EINIT;
}


int 
LWP_GetProcessPriority(PROCESS pid, int *priority)
{
    Debug(0, ("Entered Get_Process_Priority"));
    if (lwp_init) {
	*priority = pid -> priority;
	return 0;
    } else
	return LWP_EINIT;
}

int 
LWP_WaitProcess(char *event)
{
    char *tempev[2];

    Debug(0, ("Entered Wait_Process"));
    if (event == NULL) return LWP_EBADEVENT;
    tempev[0] = event;
    tempev[1] = NULL;
    return LWP_MwaitProcess(1, tempev);
}

static void 
Delete_PCB(register PROCESS pid)
{
    Debug(0, ("Entered Delete_PCB"));
    lwpremove(pid, (pid->blockflag || pid->status==WAITING || 
		    pid->status==DESTROYED ? &blocked
		    : &runnable[pid->priority]));
}

static void 
purge_dead_pcbs()
{
    for_all_elts(cur, blocked, { if (cur->status == DESTROYED) 
      Dispose_of_Dead_PCB(cur); })
}

static void 
Exit_LWP()
{
    exit (-1);
}

static void 
Dump_Processes()
{
    if (lwp_init) {
	register int i;
	for (i=0; i<MAX_PRIORITIES; i++)
	    for_all_elts(x, runnable[i], {
		printf("[Priority %d]\n", i);
		Dump_One_Process(x);
	    })
	for_all_elts(x, blocked, { Dump_One_Process(x); })
    } else
	printf("***LWP: LWP support not initialized\n");
}

char *
LWP_Name()
{
    return(lwp_cpptr->name);    
}

int 
LWP_Index()
{
    return(lwp_cpptr->index);
}

int 
LWP_HighestIndex()
{
    return(lwp_nextindex-1);
}

/* A process calls this routine to wait until somebody signals it.
 * LWP_QWait removes the calling process from the runnable queue
 * and makes the process sleep until some other process signals via LWP_QSignal
 */
int 
LWP_QWait()
{
    PROCESS old_cpptr;

    Debug(0, ("LWP_QWait: %s is going to QWait\n", lwp_cpptr->name));
    lwp_cpptr->status = QWAITING;
    if (runnable[lwp_cpptr->priority].count == 0)
	Cal_Highest_runnable_priority();

    old_cpptr = lwp_cpptr;

    /* wake up next lwp */
    lwp_cpptr = runnable[Highest_runnable_priority].head;
    lwpremove(lwp_cpptr, &runnable[Highest_runnable_priority]);
    timerclear(&lwp_cpptr->lastReady);
    pthread_mutex_lock(&ct_mutex);
    Debug(0, ("LWP_QWait:%s going to wake up %s \n", old_cpptr->name, 
	      lwp_cpptr->name));
    pthread_cond_signal(&lwp_cpptr->c);

    /* sleep on your own condition */
    Debug(0, ("LWP_QWait:%s going to wait on own condition \n", 
	      old_cpptr->name));
    pthread_cond_wait(&old_cpptr->c, &ct_mutex);
    pthread_mutex_unlock(&ct_mutex);

    lwp_cpptr = old_cpptr;

    /* return only if calling process' priority is the highest */
    if (lwp_cpptr->priority < Highest_runnable_priority)
	    Dispatcher();
    return LWP_SUCCESS;
}


/* signal the PROCESS pid - by adding it to the runnable queue */
int 
LWP_QSignal(PROCESS pid)
{
    if (pid->status == QWAITING) {
      Debug(0, ("LWP_Qsignal: %s is going to QSignal %s\n", lwp_cpptr->name,
		pid->name));
      pid->status = READY;
      lwpinsert(pid, &runnable[pid->priority]);
      Debug(0, ("LWP_QSignal: Just inserted %s into runnable queue\n", 
		pid->name));
      gettimeofday(&pid->lastReady, 0);
      Highest_runnable_priority = 
	MAX(Highest_runnable_priority, pid->priority);
      Debug(0, ("%s priority= %d; HRP = %d; Signalled process pri = %d", 
		lwp_cpptr->name, lwp_cpptr->priority,
		Highest_runnable_priority, pid->priority));
      return LWP_SUCCESS;	
    }
    else return LWP_ENOWAIT;
}


int
LWP_CreateProcess(void (*ep)(), int stacksize, int priority,
                  char *parm, char *name, PROCESS *pid)
{
    PROCESS temp;
    pthread_t	ct;
    pthread_attr_t cta;
    PROCESS old_cpptr;
    int retval;

    Debug(0, ("Entered LWP_CreateProcess to create %s at priority %d\n", 
	      name, priority));
    old_cpptr = lwp_cpptr;
    /* Throw away all dead process control blocks */
    purge_dead_pcbs();

    if (lwp_init) {
      /* allocate the memory for the pcb - check for malloc errors */
      temp = (PROCESS)malloc (sizeof (struct lwp_pcb));
      if (temp == NULL) {
	Dispatcher();
	return LWP_ENOMEM;
      }

      /* check priorities */
      if (priority < 0 || priority >= MAX_PRIORITIES) {
	Dispatcher();
	return LWP_EBADPRI;
      }
      
      Initialize_PCB(temp, priority, NULL, 0, ep, parm, name);
      
      /* make the process runnable by placing it in the runnable q */
      lwpinsert(temp, &runnable[priority]);
      gettimeofday(&temp->lastReady, 0);

      if (PRE_Block != 0) Abort_LWP("PRE_Block not 0");
      
      PRE_Block = 1;
      Proc_Running = FALSE;	    /* sem set true by forked process */
      
      pthread_attr_init(&cta);
#ifdef _POSIX_THREAD_ATTR_STACKSIZE
      pthread_attr_setstacksize(&cta, stacksize);
#endif
      retval = pthread_create(&ct, &cta, (void *)Create_Process_Part2, temp);
      if (!retval) {
	temp->a = cta;
	pthread_detach(ct);

	/* check if max priority has changed */
	Highest_runnable_priority = MAX(Highest_runnable_priority, priority);
	
	pthread_mutex_lock(&run_sem);
	Debug(0, ("Before creating process yields Proc_Running = %d\n", 
		  Proc_Running));
	while( !Proc_Running ){
	  pthread_mutex_unlock(&run_sem);
#if defined(HAVE_THR_YIELD)
	  thr_yield();
#elif defined(_POSIX_THREAD_PRIORITY_SCHEDULING)
	  sched_yield();
#else
	  pthread_yield();
#endif
	  pthread_mutex_lock(&run_sem);
	  Debug(0,("After creating proc yields and gets back control Proc_Running = %d\n", 
		    Proc_Running));
	}
	pthread_mutex_unlock(&run_sem);
	
	lwp_cpptr = old_cpptr;
	  
	Dispatcher();
	*pid = temp;
	return LWP_SUCCESS;
      }
    } else {
      pthread_attr_destroy(&cta);
    }
    return LWP_EINIT;
}

int 
LWP_DestroyProcess(PROCESS pid)
{
  void *t;

  Debug(0, ("Entered Destroy_Process"));
  if (lwp_init) {
    pthread_attr_destroy(&pid->a);
    if (lwp_cpptr == pid){
      /* kill myself */
      pid->status = DESTROYED;
      Free_PCB(pid);
      Cal_Highest_runnable_priority();
      
      /* Calculate next runnable lwp and signal it */
      lwp_cpptr = runnable[Highest_runnable_priority].head;
      lwpremove(lwp_cpptr, &runnable[Highest_runnable_priority]);
      
      pthread_mutex_lock(&ct_mutex);
      pthread_cond_signal(&lwp_cpptr->c);
      pthread_mutex_unlock(&ct_mutex);
      pthread_exit(t);
    } else {
      /* kill some other process - mark status destroyed - 
	 if process is blocked, it will be purged on next create proc;
	 if it is runnable the dispatcher will kill it */
      pid->status = DESTROYED ;
      Dispatcher();
    }
    return LWP_SUCCESS ;
  } else
    return LWP_EINIT;
}

int 
LWP_DispatchProcess()		/* explicit voluntary preemption */
{
    Debug(0, ("Entered Dispatch_Process"))
    if (lwp_init) {
	Dispatcher();
	return LWP_SUCCESS;
    } else
	return LWP_EINIT;
}

int 
LWP_Init(int version, int priority, PROCESS *pid)
{
  if (version != LWP_VERSION)
    {
      fprintf(stderr, "**** FATAL ERROR: LWP VERSION MISMATCH ****\n");
      exit(-1);
    }
  else return(InitializeProcessSupport(priority, pid));    
}

int 
LWP_InitializeProcessSupport(int priority, PROCESS *pid)
{
  return(InitializeProcessSupport(priority, pid));    
}

static int 
InitializeProcessSupport(int priority, PROCESS *pid)
{
  PROCESS temp;
  register int i;

  Debug(0, ("Entered InitializeProcessSupport"));
  if (lwp_init != NULL) return LWP_SUCCESS;
  
  /* check priorities and set up running and blocked queues */
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
  
  /* check parameters */
  Initialize_PCB(temp, priority, NULL, 0, NULL, NULL,"Main Process");
  gettimeofday(&temp->lastReady, 0);

  Highest_runnable_priority = priority;

  /* initialize mutex and semaphore */
  Proc_Running = TRUE;
  pthread_mutex_init(&run_sem, NULL);
  pthread_mutex_init(&ct_mutex, NULL);

  lwp_cpptr = temp;
  Dispatcher();
  *pid = temp;
  return LWP_SUCCESS;
}

int 
LWP_INTERNALSIGNAL(void *event, int yield)
{
  Debug(0, ("Entered LWP_SignalProcess, yield=%d", yield));
  if (lwp_init) {
    int rc;
    rc = Internal_Signal(event);
    if (yield) {
      Cal_Highest_runnable_priority();
      Debug(0, ("hipri=%d", Highest_runnable_priority));
      Dispatcher();
    }
    return rc;
  } else
    return LWP_EINIT;
}

int 
LWP_MwaitProcess(int wcount, char *evlist[]) /* wait on m of n events */
{
    register int ecount, i;
    PROCESS  old_cpptr;

    Debug(0, ("Entered Mwait_Process [waitcnt = %d]", wcount));
    if (evlist == NULL) {	
	Dispatcher();
	return LWP_EBADCOUNT;
    }
	
    /* count # of events in eventlist */
    for (ecount = 0; evlist[ecount] != NULL; ecount++) ;
    if (ecount == 0) {
	Dispatcher();
        return LWP_EBADCOUNT;
    }

    if (lwp_init) {
        /* check for illegal counts */
        if (wcount>ecount || wcount<0) {
            Dispatcher();
            return LWP_EBADCOUNT;
	}

        /* reallocate eventlist if new list has more elements than before */
        if (ecount > lwp_cpptr->eventlistsize) {
            lwp_cpptr->eventlist = 
	      (char **)realloc((char *)lwp_cpptr->eventlist, 
			       ecount*sizeof(char *));
            lwp_cpptr->eventlistsize = ecount;
	}

        /* place events in eventlist of the pcb */
        for (i=0; i<ecount; i++) lwp_cpptr -> eventlist[i] = evlist[i];

        /* if there are any events to wait on then set status to
           WAITING and place the pcb in blocked queue */
        if (wcount > 0) {
            lwp_cpptr -> status = WAITING;
            lwpinsert(lwp_cpptr, &blocked);
	}
        lwp_cpptr -> wakevent = 0;      /* index of eventid causing wakeup */
        lwp_cpptr -> waitcnt  = wcount;
        lwp_cpptr -> eventcnt = ecount;
	if (runnable[lwp_cpptr->priority].count == 0)
	  Cal_Highest_runnable_priority();
        old_cpptr = lwp_cpptr;

        /* wake up next lwp */
        lwp_cpptr = runnable[Highest_runnable_priority].head;
        lwpremove(lwp_cpptr, &runnable[Highest_runnable_priority]);
	timerclear(&lwp_cpptr->lastReady);
	Debug(0, ("WaitProcess: %s Going to signal %s \n", 
		  old_cpptr->name, lwp_cpptr->name))
        pthread_mutex_lock(&ct_mutex);
        pthread_cond_signal(&lwp_cpptr->c);

	/* sleep on your own condition */
	Debug(0, ("WaitProcess:%s going to wait \n", old_cpptr->name))
	pthread_cond_wait(&old_cpptr->c, &ct_mutex);
	pthread_mutex_unlock(&ct_mutex);

	/* update the global pointer */
	lwp_cpptr = old_cpptr;
	if (lwp_cpptr->priority < Highest_runnable_priority)
	    Dispatcher();
	return LWP_SUCCESS ;
    }
    return LWP_EINIT ;

}

int 
LWP_StackUsed(PROCESS pid, int *max, int *used)
{
    /* just here for compatibility */
    *max = -1;
    *used = -1;
    return LWP_SUCCESS;
}

static void 
Abort_LWP(char *msg)
{
    Debug(0, ("Entered Abort_LWP"))
    printf("***LWP Abort: %s\n", msg);
    Dump_Processes();
    Exit_LWP();
}

static void 
Create_Process_Part2(PROCESS temp)
{
    /* set the global Proc_Running to signal the parent */
    pthread_mutex_lock(&run_sem);
    Proc_Running = TRUE;
    pthread_cond_wait(&temp->c, &run_sem);
    pthread_mutex_unlock(&run_sem);
    lwp_cpptr = temp;

#ifdef _POSIX_THREAD_ATTR_STACKSIZE
    pthread_attr_setstacksize(&temp->a, temp->stacksize);
#endif
    
    (*temp->ep)(temp->parm);
    LWP_DestroyProcess(temp);
}

static void 
Dump_One_Process(PROCESS pid)
{
    int i;

    printf("***LWP: Process Control Block at 0x%x\n", (int)pid);
    printf("***LWP: Name: %s\n", pid->name);
    if (pid->ep != NULL)
	printf("***LWP: Initial entry point: 0x%x\n", (int)pid->ep);
    if (pid->blockflag) printf("BLOCKED and ");
    switch (pid->status) {
	case READY:	printf("READY");     break;
	case WAITING:	printf("WAITING");   break;
	case DESTROYED:	printf("DESTROYED"); break;
	default:	printf("unknown");
	}
    putchar('\n');
    printf("***LWP: Priority: %d \tInitial parameter: 0x%x\n",
	    pid->priority, (int)pid->parm);

    if (pid->eventcnt > 0) {
	printf("***LWP: Number of events outstanding: %d\n", pid->waitcnt);
	printf("***LWP: Event id list:");
	for (i=0;i<pid->eventcnt;i++)
	    printf(" 0x%x", (int)pid->eventlist[i]);
	putchar('\n');
    }
    if (pid->wakevent>0)
	printf("***LWP: Number of last wakeup event: %d\n", pid->wakevent);
}

static void 
Dispatcher()		/* Lightweight process dispatcher */
{
	void *t;
	int my_priority;
	PROCESS	old_cpptr;
#if 1
	int i = Highest_runnable_priority;
	
	Cal_Highest_runnable_priority();
	if (Highest_runnable_priority != i)
	  printf("hipri was %d actually %d\n", i, Highest_runnable_priority);
	Highest_runnable_priority = i;
#endif
	my_priority = lwp_cpptr->priority;
	Debug(0, ("Dispatcher: %d runnable at pri %d hi %d blk %d", 
		  runnable[my_priority].count, my_priority, 
		  Highest_runnable_priority, PRE_Block));
	PRE_Block = 1;
	if ((my_priority < Highest_runnable_priority) || 
	    (runnable[my_priority].count > 0))
	  {
	    Debug(0, ("Dispatcher: %s is now yielding", lwp_cpptr->name));
	    /* I have to quit */
	    old_cpptr = lwp_cpptr;
	    lwpinsert(old_cpptr, &runnable[my_priority]);
	    gettimeofday(&old_cpptr->lastReady, 0);
	    lwp_cpptr = runnable[Highest_runnable_priority].head;

	    /* remove next process from runnable queue and signal it */
	    lwpremove(lwp_cpptr, &runnable[Highest_runnable_priority]);
	    pthread_mutex_lock(&ct_mutex);
	    Debug(0, ("Dispatcher: %s going to signal %s condition\n", 
		      old_cpptr->name, lwp_cpptr->name))

	    pthread_cond_signal(&lwp_cpptr->c);

	    /* now sleep until somebody wakes me */
	    Debug(0, ("Dispatcher: %s going to wait on own condition\n", 
		      old_cpptr->name))
	    pthread_cond_wait(&old_cpptr->c, &ct_mutex);
	    pthread_mutex_unlock(&ct_mutex);

	    /* update global pointer */
	    lwp_cpptr = old_cpptr;
	} else {
	    Debug(0, ("Dispatcher: %s still running", lwp_cpptr->name))
	}
	/* make sure HRP is set correct */
	Highest_runnable_priority = lwp_cpptr->priority;
	if (lwp_cpptr->status == DESTROYED){
		/* the process was runnable but got destroyed by somebody */
		Free_PCB(lwp_cpptr);
		Cal_Highest_runnable_priority();
		lwp_cpptr = runnable[Highest_runnable_priority].head;
		lwpremove(lwp_cpptr, &runnable[Highest_runnable_priority]);

		pthread_mutex_lock(&ct_mutex);
		pthread_cond_signal(&lwp_cpptr->c);
		pthread_mutex_unlock(&ct_mutex);
		pthread_exit(t);
	}
#if 0
	if (PRE_Block != 1) Abort_LWP("PRE_Block not 1");
#endif
	PRE_Block = 0;
}


static void 
Free_PCB(PROCESS pid)
{
    Debug(0, ("Entered Free_PCB"))

    if (pid->eventlist != NULL)  free((char *)pid->eventlist);
    free((char *)pid);
}	

static void
Initialize_PCB(PROCESS temp, int priority, char *stack, int stacksize, 
               void (*ep)(), char *parm, char *name)
{
    register int i = 0;

    Debug(0, ("Entered Initialize_PCB"))
    if (name != NULL)
	while (((temp -> name[i] = name[i]) != '\0') && (i < 31)) i++;
    temp -> name[31] = '\0';
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
    temp -> ep = ep;
    temp -> parm = parm;
    temp -> misc = NULL;	/* currently unused */
    temp -> next = NULL;
    temp -> prev = NULL;
    temp -> rused = 0;
    temp -> level = 1;		/* non-preemptable */
    timerclear(&temp->lastReady);

    /* initialize the mutex and condition */
    pthread_mutex_init(&temp->m, NULL);	
    pthread_cond_init(&temp->c, NULL);

    Debug(0, ("Leaving Initialize_PCB\n"))
}

static int 
Internal_Signal(register char *event)
{
  int rc = LWP_ENOWAIT;
  register int i;

  Debug(0, ("Entered Internal_Signal [event id 0x%x]", (int)event));
  if (!lwp_init) return LWP_EINIT;
  if (event == NULL) return LWP_EBADEVENT;
  
  for_all_elts(temp, blocked, {     /* for all pcb's on the blocked q */
    if (temp->status == WAITING)
      for (i=0; i < temp->eventcnt; i++) { /* check each event in list */
	if (temp -> eventlist[i] == event) {
	  temp -> eventlist[i] = NULL;
	  rc = LWP_SUCCESS;
	  Debug(0, ("decrementing %s to %d", temp->name, (temp->waitcnt-1)));
	  /* reduce waitcnt by 1 for the signal */
	  /* if wcount reaches 0 then make the process runnable */
	  if (--temp->waitcnt == 0) {
	    temp -> status = READY;
	    temp -> wakevent = i+1;
	    lwpmove(temp, &blocked, &runnable[temp->priority]);
	    gettimeofday(&temp->lastReady, 0);
	    Highest_runnable_priority = 
	      MAX(Highest_runnable_priority, temp->priority);
	    Debug(0, ("marked runnable. hi_pri %d, %d at %d", Highest_runnable_priority, runnable[temp->priority].count, temp->priority));
	    break;
	  }
	}
      }
  })
    return rc;
}    

/* places the maximum of runnable task priorities in the global variable -
 * Highest_runnable_priority.  No runnable process is an error */
static void 
Cal_Highest_runnable_priority()
{
    int	i;
    for (i = LWP_MAX_PRIORITY; runnable[i].count == 0 && i >=0; i--);
#if 0
    if (i < 0)
	    Abort_LWP("No ready processes");
    else
	    Highest_runnable_priority = i;
#else
    if (i >= 0)
	    Highest_runnable_priority = i;
#endif
}
