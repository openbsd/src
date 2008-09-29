
/*
 * Copyright © 2001 Novell, Inc. All Rights Reserved.
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Artistic License, as specified in the README file.
 *
 */

/*
 * FILENAME		:	nw5thread.h
 * DESCRIPTION	:	Thread related functions.
 * Author		:	SGP
 * Date			:	January 2001.
 *
 */



#ifndef _NW5THREAD_H
#define _NW5THREAD_H


#include <nwthread.h>

#include "netware.h"

typedef long perl_key;

// The line below is just a definition to avoid compilation error.
// It is not being used anywhere.
// Ananth, 3 Sept 2001
typedef struct nw_cond { long waiters; unsigned int sem; } perl_cond;

#if defined (USE_ITHREADS) && defined(MPK_ON)
#ifdef __cplusplus
extern "C"
{
#endif
	#include <mpktypes.h>
	#include <mpkapis.h>
	#define kSUCCESS	(0)
	#define ERROR_INVALID_MUTEX  (0x1010)

#ifdef __cplusplus
}
#endif
#undef WORD
//On NetWare, since the NLM will be resident, only once the MUTEX_INIT gets called and
//this will be freed when the script terminates.  But when a new script is executed,
//then MUTEX_LOCK will fail since it is already freed.  Even if this problem is fixed
//by not freeing the mutex when script terminates but when the NLM unloads, there will
//still be problems when multiple scripts are running simultaneously in a multi-processor
//machine - sgp
typedef MUTEX perl_mutex;
#  define MUTEX_INIT(m) \
    STMT_START {						\
	/*if ((*(m) = kMutexAlloc("NetWarePerlMutex")) == NULL)	*/\
	    /*Perl_croak_nocontext("panic: MUTEX_ALLOC");		*/\
	/*ConsolePrintf("Mutex Init %d\n",*(m));	*/\
    } STMT_END

#  define MUTEX_LOCK(m) \
    STMT_START {						\
	/*ConsolePrintf("Mutex lock %d\n",*(m));	*/\
	/*if (kMutexLock(*(m)) == ERROR_INVALID_MUTEX)	*/\
	    /*Perl_croak_nocontext("panic: MUTEX_LOCK");		*/\
    } STMT_END

#  define MUTEX_UNLOCK(m) \
    STMT_START {						\
	/*ConsolePrintf("Mutex unlock %d\n",*(m));	*/\
	/*if (kMutexUnlock(*(m)) != kSUCCESS)				\
	    Perl_croak_nocontext("panic: MUTEX_UNLOCK");	*/\
    } STMT_END

#  define MUTEX_DESTROY(m) \
    STMT_START {						\
	/*ConsolePrintf("Mutex Destroy %d\n",*(m));	*/\
	/*if (kMutexWaitCount(*(m)) == 0 )	*/\
	/*{	*/\
		/*PERL_SET_INTERP(NULL); *//*newly added CHKSGP???*/	\
		/*if (kMutexFree(*(m)) != kSUCCESS)			*/	\
			/*Perl_croak_nocontext("panic: MUTEX_FREE");	*/\
	/*}	*/\
    } STMT_END

#else
typedef unsigned long perl_mutex;
#  define MUTEX_INIT(m)
#  define MUTEX_LOCK(m)
#  define MUTEX_UNLOCK(m)
#  define MUTEX_DESTROY(m)
#endif

/* These macros assume that the mutex associated with the condition
 * will always be held before COND_{SIGNAL,BROADCAST,WAIT,DESTROY},
 * so there's no separate mutex protecting access to (c)->waiters
 */
//For now let us just see when this happens -sgp.
#define COND_INIT(c) \
    STMT_START {						\
	/*ConsolePrintf("In COND_INIT\n");	*/\
    } STMT_END

/*	(c)->waiters = 0;					\
	(c)->sem = OpenLocalSemaphore (0);	\
	if ((c)->sem == NULL)					\
	    Perl_croak_nocontext("panic: COND_INIT (%ld)",errno);	\*/

#define COND_SIGNAL(c) \
    STMT_START {						\
	/*ConsolePrintf("In COND_SIGNAL\n");	*/\
    } STMT_END
/*if ((c)->waiters > 0 &&					\
	    SignalLocalSemaphore((c)->sem) != 0)		\
	    Perl_croak_nocontext("panic: COND_SIGNAL (%ld)",errno);	\*/

#define COND_BROADCAST(c) \
    STMT_START {						\
	/*ConsolePrintf("In COND_BROADCAST\n");	*/\
    } STMT_END

	/*if ((c)->waiters > 0 ) {					\
		int count;	\
		for(count=0; count<(c)->waiters; count++) {	\
			if(SignalLocalSemaphore((c)->sem) != 0)	\
				Perl_croak_nocontext("panic: COND_BROADCAST (%ld)",GetLastError());\
		}	\
	}	\*/
#define COND_WAIT(c, m) \
    STMT_START {						\
	/*ConsolePrintf("In COND_WAIT\n");	*/\
    } STMT_END


#define COND_DESTROY(c) \
    STMT_START {						\
	/*ConsolePrintf("In COND_DESTROY\n");	*/\
    } STMT_END

/*		(c)->waiters = 0;					\
	if (CloseLocalSemaphore((c)->sem) != 0)				\
	    Perl_croak_nocontext("panic: COND_DESTROY (%ld)",errno);	\*/

#if 0
#define DETACH(t) \
    STMT_START {						\
	if (CloseHandle((t)->self) == 0) {			\
	    MUTEX_UNLOCK(&(t)->mutex);				\
	    Perl_croak_nocontext("panic: DETACH");		\
	}							\
    } STMT_END
#endif	//#if 0

//Following has to be defined CHKSGP
#if defined(PERLDLL) && defined(USE_DECLSPEC_THREAD) && (!defined(__BORLANDC__) || defined(_DLL))
extern __declspec(thread) void *PL_current_context;
#define PERL_SET_CONTEXT(t)   		(PL_current_context = t)
#define PERL_GET_CONTEXT		PL_current_context
#else
#define PERL_GET_CONTEXT		Perl_get_context()
#define PERL_SET_CONTEXT(t)		Perl_set_context(t)
#endif

//Check the following, will be used in Thread extension - CHKSGP
#define THREAD_RET_TYPE	unsigned __stdcall
#define THREAD_RET_CAST(p)	((unsigned)(p))

#define INIT_THREADS		NOOP

//Ideally this should have been PL_thr_key = fnInitializeThreadCtx();
//See the comment at the end of file nw5thread.c as to why PL_thr_key is not assigned - sgp
#define ALLOC_THREAD_KEY \
    STMT_START {							\
	fnInitializeThreadCtx();			\
    } STMT_END


#endif /* _NW5THREAD_H */

