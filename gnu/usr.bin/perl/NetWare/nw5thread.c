
/*
 * Copyright © 2001 Novell, Inc. All Rights Reserved.
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Artistic License, as specified in the README file.
 *
 */

/*
 * FILENAME		:	nw5thread.c
 * DESCRIPTION	:	Thread related functions.
 * Author		:	SGP
 * Date			:	January 2001.
 *
 */



#include "EXTERN.h"
#include "perl.h"

//For Thread Local Storage
#include "win32ish.h"		// For "BOOL", "TRUE" and "FALSE"
#include "nwtinfo.h"

#ifdef USE_DECLSPEC_THREAD
__declspec(thread) void *PL_current_context = NULL;
#endif


void
Perl_set_context(void *t)
{
#if defined(USE_ITHREADS)
#  ifdef USE_DECLSPEC_THREAD
    Perl_current_context = t;
#  else
	fnAddThreadCtx(PL_thr_key, t);
#  endif
#endif
}


void *
Perl_get_context(void)
{
#if defined(USE_ITHREADS)
#  ifdef USE_DECLSPEC_THREAD
    return Perl_current_context;
#  else
	return(fnGetThreadCtx(PL_thr_key));
#  endif
#else
    return NULL;
#endif
}


//To Remove the Thread Context stored during Perl_set_context
BOOL
Remove_Thread_Ctx(void)
{
#if defined(USE_ITHREADS)
#  ifdef USE_DECLSPEC_THREAD
	return TRUE;
#  else
	return(fnRemoveThreadCtx(PL_thr_key));
#  endif
#  else
	return TRUE;
#endif
}


//PL_thr_key - Not very sure if this is global or per thread.  When multiple scripts
//run simultaneously on NetWare, this will give problems.  Hence in nwtinfo.c, the 
//current thread id is used as the TLS index & PL_thr_key is not used.  
//This has to be checked???? - sgp

