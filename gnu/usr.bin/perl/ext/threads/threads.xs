#define PERL_NO_GET_CONTEXT
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#ifdef USE_ITHREADS

#ifdef WIN32
#include <windows.h>
#include <win32thread.h>
#define PERL_THREAD_SETSPECIFIC(k,v) TlsSetValue(k,v)
#define PERL_THREAD_GETSPECIFIC(k,v) v = TlsGetValue(k)
#define PERL_THREAD_ALLOC_SPECIFIC(k) \
STMT_START {\
  if((k = TlsAlloc()) == TLS_OUT_OF_INDEXES) {\
    PerlIO_printf(PerlIO_stderr(),"panic threads.h: TlsAlloc");\
    exit(1);\
  }\
} STMT_END
#else
#include <pthread.h>
#include <thread.h>

#define PERL_THREAD_SETSPECIFIC(k,v) pthread_setspecific(k,v)
#ifdef OLD_PTHREADS_API
#define PERL_THREAD_DETACH(t) pthread_detach(&(t))
#define PERL_THREAD_GETSPECIFIC(k,v) pthread_getspecific(k,&v)
#define PERL_THREAD_ALLOC_SPECIFIC(k) STMT_START {\
  if(pthread_keycreate(&(k),0)) {\
    PerlIO_printf(PerlIO_stderr(), "panic threads.h: pthread_key_create");\
    exit(1);\
  }\
} STMT_END
#else
#define PERL_THREAD_DETACH(t) pthread_detach((t))
#define PERL_THREAD_GETSPECIFIC(k,v) v = pthread_getspecific(k)
#define PERL_THREAD_ALLOC_SPECIFIC(k) STMT_START {\
  if(pthread_key_create(&(k),0)) {\
    PerlIO_printf(PerlIO_stderr(), "panic threads.h: pthread_key_create");\
    exit(1);\
  }\
} STMT_END
#endif
#endif

/* Values for 'state' member */
#define PERL_ITHR_JOINABLE		0
#define PERL_ITHR_DETACHED		1
#define PERL_ITHR_FINISHED		4
#define PERL_ITHR_JOINED		2

typedef struct ithread_s {
    struct ithread_s *next;	/* next thread in the list */
    struct ithread_s *prev;	/* prev thread in the list */
    PerlInterpreter *interp;	/* The threads interpreter */
    I32 tid;              	/* threads module's thread id */
    perl_mutex mutex; 		/* mutex for updating things in this struct */
    I32 count;			/* how many SVs have a reference to us */
    signed char state;		/* are we detached ? */
    int gimme;			/* Context of create */
    SV* init_function;          /* Code to run */
    SV* params;                 /* args to pass function */
#ifdef WIN32
	DWORD	thr;            /* OS's idea if thread id */
	HANDLE handle;          /* OS's waitable handle */
#else
  	pthread_t thr;          /* OS's handle for the thread */
#endif
} ithread;

ithread *threads;

/* Macros to supply the aTHX_ in an embed.h like manner */
#define ithread_join(thread)		Perl_ithread_join(aTHX_ thread)
#define ithread_DESTROY(thread)		Perl_ithread_DESTROY(aTHX_ thread)
#define ithread_CLONE(thread)		Perl_ithread_CLONE(aTHX_ thread)
#define ithread_detach(thread)		Perl_ithread_detach(aTHX_ thread)
#define ithread_tid(thread)		((thread)->tid)
#define ithread_yield(thread)		(YIELD);

static perl_mutex create_destruct_mutex;  /* protects the creation and destruction of threads*/

I32 tid_counter = 0;
I32 known_threads = 0;
I32 active_threads = 0;
perl_key self_key;

/*
 *  Clear up after thread is done with
 */
void
Perl_ithread_destruct (pTHX_ ithread* thread, const char *why)
{
        PerlInterpreter* destroyperl = NULL;        
	MUTEX_LOCK(&thread->mutex);
	if (!thread->next) {
	    Perl_croak(aTHX_ "panic: destruct destroyed thread %p (%s)",thread, why);
	}
	if (thread->count != 0) {
		MUTEX_UNLOCK(&thread->mutex);
		return;
	}
	MUTEX_LOCK(&create_destruct_mutex);
	/* Remove from circular list of threads */
	if (thread->next == thread) {
	    /* last one should never get here ? */
	    threads = NULL;
        }
	else {
	    thread->next->prev = thread->prev;
	    thread->prev->next = thread->next;
	    if (threads == thread) {
		threads = thread->next;
	    }
	    thread->next = NULL;
	    thread->prev = NULL;
	}
	known_threads--;
	assert( known_threads >= 0 );
#if 0
        Perl_warn(aTHX_ "destruct %d @ %p by %p now %d",
	          thread->tid,thread->interp,aTHX, known_threads);
#endif
	MUTEX_UNLOCK(&create_destruct_mutex);
	/* Thread is now disowned */
	if (thread->interp) {
	    dTHXa(thread->interp);
	    PERL_SET_CONTEXT(thread->interp);
	    SvREFCNT_dec(thread->params);
	    thread->params = Nullsv;
	    destroyperl = thread->interp;
	    thread->interp = NULL;
	}
	MUTEX_UNLOCK(&thread->mutex);
	MUTEX_DESTROY(&thread->mutex);
        PerlMemShared_free(thread);
	if(destroyperl) {
	    perl_destruct(destroyperl);
            perl_free(destroyperl);
	}
	PERL_SET_CONTEXT(aTHX);
}

int
Perl_ithread_hook(pTHX)
{
    int veto_cleanup = 0;
    MUTEX_LOCK(&create_destruct_mutex);
    if (aTHX == PL_curinterp && active_threads != 1) {
	Perl_warn(aTHX_ "A thread exited while %" IVdf " other threads were still running",
						(IV)active_threads);
	veto_cleanup = 1;
    }
    MUTEX_UNLOCK(&create_destruct_mutex);
    return veto_cleanup;
}

void
Perl_ithread_detach(pTHX_ ithread *thread)
{
    MUTEX_LOCK(&thread->mutex);
    if (!(thread->state & (PERL_ITHR_DETACHED|PERL_ITHR_JOINED))) {
	thread->state |= PERL_ITHR_DETACHED;
#ifdef WIN32
	CloseHandle(thread->handle);
	thread->handle = 0;
#else
	PERL_THREAD_DETACH(thread->thr);
#endif
    }
    if ((thread->state & PERL_ITHR_FINISHED) &&
        (thread->state & PERL_ITHR_DETACHED)) {
	MUTEX_UNLOCK(&thread->mutex);
	Perl_ithread_destruct(aTHX_ thread, "detach");
    }
    else {
	MUTEX_UNLOCK(&thread->mutex);
    }
}

/* MAGIC (in mg.h sense) hooks */

int
ithread_mg_get(pTHX_ SV *sv, MAGIC *mg)
{
    ithread *thread = (ithread *) mg->mg_ptr;
    SvIVX(sv) = PTR2IV(thread);
    SvIOK_on(sv);
    return 0;
}

int
ithread_mg_free(pTHX_ SV *sv, MAGIC *mg)
{
    ithread *thread = (ithread *) mg->mg_ptr;
    MUTEX_LOCK(&thread->mutex);
    thread->count--;
    if (thread->count == 0) {
       if(thread->state & PERL_ITHR_FINISHED &&
          (thread->state & PERL_ITHR_DETACHED ||
           thread->state & PERL_ITHR_JOINED))
       {
            MUTEX_UNLOCK(&thread->mutex);
            Perl_ithread_destruct(aTHX_ thread, "no reference");
       }
       else {
	    MUTEX_UNLOCK(&thread->mutex);
       }    
    }
    else {
	MUTEX_UNLOCK(&thread->mutex);
    }
    return 0;
}

int
ithread_mg_dup(pTHX_ MAGIC *mg, CLONE_PARAMS *param)
{
    ithread *thread = (ithread *) mg->mg_ptr;
    MUTEX_LOCK(&thread->mutex);
    thread->count++;
    MUTEX_UNLOCK(&thread->mutex);
    return 0;
}

MGVTBL ithread_vtbl = {
 ithread_mg_get,	/* get */
 0,			/* set */
 0,			/* len */
 0,			/* clear */
 ithread_mg_free,	/* free */
 0,			/* copy */
 ithread_mg_dup		/* dup */
};


/*
 *	Starts executing the thread. Needs to clean up memory a tad better.
 *      Passed as the C level function to run in the new thread
 */

#ifdef WIN32
THREAD_RET_TYPE
Perl_ithread_run(LPVOID arg) {
#else
void*
Perl_ithread_run(void * arg) {
#endif
	ithread* thread = (ithread*) arg;
	dTHXa(thread->interp);
	PERL_SET_CONTEXT(thread->interp);
	PERL_THREAD_SETSPECIFIC(self_key,thread);

#if 0
	/* Far from clear messing with ->thr child-side is a good idea */
	MUTEX_LOCK(&thread->mutex);
#ifdef WIN32
	thread->thr = GetCurrentThreadId();
#else
	thread->thr = pthread_self();
#endif
 	MUTEX_UNLOCK(&thread->mutex);
#endif

	PL_perl_destruct_level = 2;

	{
		AV* params = (AV*) SvRV(thread->params);
		I32 len = av_len(params)+1;
		int i;
		dSP;
		ENTER;
		SAVETMPS;
		PUSHMARK(SP);
		for(i = 0; i < len; i++) {
		    XPUSHs(av_shift(params));
		}
		PUTBACK;
		len = call_sv(thread->init_function, thread->gimme|G_EVAL);
		SPAGAIN;
		for (i=len-1; i >= 0; i--) {
		  SV *sv = POPs;
		  av_store(params, i, SvREFCNT_inc(sv));
		}
		PUTBACK;
		if (SvTRUE(ERRSV)) {
		    Perl_warn(aTHX_ "thread failed to start: %" SVf, ERRSV);
		}
		FREETMPS;
		LEAVE;
		SvREFCNT_dec(thread->init_function);
	}

	PerlIO_flush((PerlIO*)NULL);
	MUTEX_LOCK(&thread->mutex);
	thread->state |= PERL_ITHR_FINISHED;

	if (thread->state & PERL_ITHR_DETACHED) {
		MUTEX_UNLOCK(&thread->mutex);
		Perl_ithread_destruct(aTHX_ thread, "detached finish");
	} else {
		MUTEX_UNLOCK(&thread->mutex);
	}
	MUTEX_LOCK(&create_destruct_mutex);
	active_threads--;
	assert( active_threads >= 0 );
	MUTEX_UNLOCK(&create_destruct_mutex);

#ifdef WIN32
	return (DWORD)0;
#else
	return 0;
#endif
}

SV *
ithread_to_SV(pTHX_ SV *obj, ithread *thread, char *classname, bool inc)
{
    SV *sv;
    MAGIC *mg;
    if (inc) {
	MUTEX_LOCK(&thread->mutex);
	thread->count++;
	MUTEX_UNLOCK(&thread->mutex);
    }
    if (!obj)
     obj = newSV(0);
    sv = newSVrv(obj,classname);
    sv_setiv(sv,PTR2IV(thread));
    mg = sv_magicext(sv,Nullsv,PERL_MAGIC_shared_scalar,&ithread_vtbl,(char *)thread,0);
    mg->mg_flags |= MGf_DUP;
    SvREADONLY_on(sv);
    return obj;
}

ithread *
SV_to_ithread(pTHX_ SV *sv)
{
    ithread *thread;
    if (SvROK(sv))
     {
      thread = INT2PTR(ithread*, SvIV(SvRV(sv)));
     }
    else
     {
      PERL_THREAD_GETSPECIFIC(self_key,thread);
     }
    return thread;
}

/*
 * iThread->create(); ( aka iThread->new() )
 * Called in context of parent thread
 */

SV *
Perl_ithread_create(pTHX_ SV *obj, char* classname, SV* init_function, SV* params)
{
	ithread*	thread;
	CLONE_PARAMS	clone_param;

	MUTEX_LOCK(&create_destruct_mutex);
	thread = PerlMemShared_malloc(sizeof(ithread));
	Zero(thread,1,ithread);
	thread->next = threads;
	thread->prev = threads->prev;
	threads->prev = thread;
	thread->prev->next = thread;
	/* Set count to 1 immediately in case thread exits before
	 * we return to caller !
	 */
	thread->count = 1;
	MUTEX_INIT(&thread->mutex);
	thread->tid = tid_counter++;
	thread->gimme = GIMME_V;

	/* "Clone" our interpreter into the thread's interpreter
	 * This gives thread access to "static data" and code.
	 */

	PerlIO_flush((PerlIO*)NULL);

#ifdef WIN32
	thread->interp = perl_clone(aTHX, CLONEf_KEEP_PTR_TABLE | CLONEf_CLONE_HOST);
#else
	thread->interp = perl_clone(aTHX, CLONEf_KEEP_PTR_TABLE);
#endif
	/* perl_clone leaves us in new interpreter's context.
	   As it is tricky to spot implcit aTHX create a new scope
	   with aTHX matching the context for the duration of
	   our work for new interpreter.
	 */
	{
	    dTHXa(thread->interp);
            /* Here we remove END blocks since they should only run
	       in the thread they are created
            */
            SvREFCNT_dec(PL_endav);
            PL_endav = newAV();
            clone_param.flags = 0;
	    thread->init_function = sv_dup(init_function, &clone_param);
	    if (SvREFCNT(thread->init_function) == 0) {
		SvREFCNT_inc(thread->init_function);
	    }

	    thread->params = sv_dup(params, &clone_param);
	    SvREFCNT_inc(thread->params);
	    SvTEMP_off(thread->init_function);
	    ptr_table_free(PL_ptr_table);
	    PL_ptr_table = NULL;
	    PL_exit_flags |= PERL_EXIT_DESTRUCT_END;
	}

	PERL_SET_CONTEXT(aTHX);

	/* Start the thread */

#ifdef WIN32

	thread->handle = CreateThread(NULL, 0, Perl_ithread_run,
			(LPVOID)thread, 0, &thread->thr);

#else
	{
	  static pthread_attr_t attr;
	  static int attr_inited = 0;
	  static int attr_joinable = PTHREAD_CREATE_JOINABLE;
	  if (!attr_inited) {
	    attr_inited = 1;
	    pthread_attr_init(&attr);
	  }
#  ifdef PTHREAD_ATTR_SETDETACHSTATE
            PTHREAD_ATTR_SETDETACHSTATE(&attr, attr_joinable);
#  endif
#  ifdef THREAD_CREATE_NEEDS_STACK
	    if(pthread_attr_setstacksize(&attr, THREAD_CREATE_NEEDS_STACK))
	      croak("panic: pthread_attr_setstacksize failed");
#  endif

#ifdef OLD_PTHREADS_API
	  pthread_create( &thread->thr, attr, Perl_ithread_run, (void *)thread);
#else
	  pthread_create( &thread->thr, &attr, Perl_ithread_run, (void *)thread);
#endif
	}
#endif
	known_threads++;
	active_threads++;
	MUTEX_UNLOCK(&create_destruct_mutex);
	sv_2mortal(params);
	return ithread_to_SV(aTHX_ obj, thread, classname, FALSE);
}

SV*
Perl_ithread_self (pTHX_ SV *obj, char* Class)
{
    ithread *thread;
    PERL_THREAD_GETSPECIFIC(self_key,thread);
    return ithread_to_SV(aTHX_ obj, thread, Class, TRUE);
}

/*
 * Joins the thread this code needs to take the returnvalue from the
 * call_sv and send it back
 */

void
Perl_ithread_CLONE(pTHX_ SV *obj)
{
 if (SvROK(obj))
  {
   ithread *thread = SV_to_ithread(aTHX_ obj);
  }
 else
  {
   Perl_warn(aTHX_ "CLONE %" SVf,obj);
  }
}

AV*
Perl_ithread_join(pTHX_ SV *obj)
{
    ithread *thread = SV_to_ithread(aTHX_ obj);
    MUTEX_LOCK(&thread->mutex);
    if (thread->state & PERL_ITHR_DETACHED) {
	MUTEX_UNLOCK(&thread->mutex);
	Perl_croak(aTHX_ "Cannot join a detached thread");
    }
    else if (thread->state & PERL_ITHR_JOINED) {
	MUTEX_UNLOCK(&thread->mutex);
	Perl_croak(aTHX_ "Thread already joined");
    }
    else {
        AV* retparam;
#ifdef WIN32
	DWORD waitcode;
#else
	void *retval;
#endif
	MUTEX_UNLOCK(&thread->mutex);
#ifdef WIN32
	waitcode = WaitForSingleObject(thread->handle, INFINITE);
#else
	pthread_join(thread->thr,&retval);
#endif
	MUTEX_LOCK(&thread->mutex);
	
	/* sv_dup over the args */
	{
	  AV* params = (AV*) SvRV(thread->params);	
	  CLONE_PARAMS clone_params;
	  clone_params.stashes = newAV();
	  PL_ptr_table = ptr_table_new();
	  retparam = (AV*) sv_dup((SV*)params, &clone_params);
	  SvREFCNT_dec(clone_params.stashes);
	  SvREFCNT_inc(retparam);
	  ptr_table_free(PL_ptr_table);
	  PL_ptr_table = NULL;

	}
	/* We have finished with it */
	thread->state |= PERL_ITHR_JOINED;
	MUTEX_UNLOCK(&thread->mutex);
    	sv_unmagic(SvRV(obj),PERL_MAGIC_shared_scalar);
	return retparam;
    }
    return (AV*)NULL;
}

void
Perl_ithread_DESTROY(pTHX_ SV *sv)
{
    ithread *thread = SV_to_ithread(aTHX_ sv);
    sv_unmagic(SvRV(sv),PERL_MAGIC_shared_scalar);
}

#endif /* USE_ITHREADS */

MODULE = threads		PACKAGE = threads	PREFIX = ithread_
PROTOTYPES: DISABLE

#ifdef USE_ITHREADS

void
ithread_new (classname, function_to_call, ...)
char *	classname
SV *	function_to_call
CODE:
{
    AV* params = newAV();
    if (items > 2) {
	int i;
	for(i = 2; i < items ; i++) {
	    av_push(params, SvREFCNT_inc(ST(i)));
	}
    }
    ST(0) = sv_2mortal(Perl_ithread_create(aTHX_ Nullsv, classname, function_to_call, newRV_noinc((SV*) params)));
    XSRETURN(1);
}

void
ithread_list(char *classname)
PPCODE:
{
  ithread *curr_thread;
  MUTEX_LOCK(&create_destruct_mutex);
  curr_thread = threads;
  if(curr_thread->tid != 0)	
    PUSHs( sv_2mortal(ithread_to_SV(aTHX_ NULL, curr_thread, classname, TRUE)));
  while(curr_thread) {
    curr_thread = curr_thread->next;
    if(curr_thread == threads)
      break;
    if(curr_thread->state & PERL_ITHR_DETACHED ||
       curr_thread->state & PERL_ITHR_JOINED)
         continue;
     PUSHs( sv_2mortal(ithread_to_SV(aTHX_ NULL, curr_thread, classname, TRUE)));
  }	
  MUTEX_UNLOCK(&create_destruct_mutex);
}


void
ithread_self(char *classname)
CODE:
{
	ST(0) = sv_2mortal(Perl_ithread_self(aTHX_ Nullsv,classname));
	XSRETURN(1);
}

int
ithread_tid(ithread *thread)

void
ithread_join(SV *obj)
PPCODE:
{
  AV* params = Perl_ithread_join(aTHX_ obj);
  int i;
  I32 len = AvFILL(params);
  for (i = 0; i <= len; i++) {
    SV* tmp = av_shift(params);
    XPUSHs(tmp);
    sv_2mortal(tmp);
  }
  SvREFCNT_dec(params);
}

void
yield(...)
CODE:
{
    YIELD;
}
	

void
ithread_detach(ithread *thread)

void
ithread_DESTROY(SV *thread)

#endif /* USE_ITHREADS */

BOOT:
{
#ifdef USE_ITHREADS
	ithread* thread;
	PL_perl_destruct_level = 2;
	PERL_THREAD_ALLOC_SPECIFIC(self_key);
	MUTEX_INIT(&create_destruct_mutex);
	MUTEX_LOCK(&create_destruct_mutex);
	PL_threadhook = &Perl_ithread_hook;
	thread  = PerlMemShared_malloc(sizeof(ithread));
	Zero(thread,1,ithread);
	PL_perl_destruct_level = 2;
	MUTEX_INIT(&thread->mutex);
	threads = thread;
	thread->next = thread;
        thread->prev = thread;
	thread->interp = aTHX;
	thread->count  = 1;  /* imortal */
	thread->tid = tid_counter++;
	known_threads++;
	active_threads++;
	thread->state = 1;
#ifdef WIN32
	thread->thr = GetCurrentThreadId();
#else
	thread->thr = pthread_self();
#endif

	PERL_THREAD_SETSPECIFIC(self_key,thread);
	MUTEX_UNLOCK(&create_destruct_mutex);
#endif /* USE_ITHREADS */
}

