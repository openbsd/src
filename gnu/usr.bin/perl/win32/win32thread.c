#include "EXTERN.h"
#include "perl.h"

#if defined(PERL_OBJECT)
#define NO_XSLOCKS
extern CPerlObj* pPerl;
#include "XSUB.h"
#endif

#ifdef USE_DECLSPEC_THREAD
__declspec(thread) struct perl_thread *Perl_current_thread = NULL;
#endif

void
Perl_setTHR(struct perl_thread *t)
{
#ifdef USE_THREADS
#ifdef USE_DECLSPEC_THREAD
 Perl_current_thread = t;
#else
 TlsSetValue(PL_thr_key,t);
#endif
#endif
}

struct perl_thread *
Perl_getTHR(void)
{
#ifdef USE_THREADS
#ifdef USE_DECLSPEC_THREAD
 return Perl_current_thread;
#else
 return (struct perl_thread *) TlsGetValue(PL_thr_key);
#endif
#else
 return NULL;
#endif
}

void
Perl_alloc_thread_key(void)
{
#ifdef USE_THREADS
    static int key_allocated = 0;
    if (!key_allocated) {
	if ((PL_thr_key = TlsAlloc()) == TLS_OUT_OF_INDEXES)
	    croak("panic: TlsAlloc");
	key_allocated = 1;
    }
#endif
}

void
Perl_init_thread_intern(struct perl_thread *athr)
{
#ifdef USE_THREADS
#ifndef USE_DECLSPEC_THREAD

 /* 
  * Initialize port-specific per-thread data in thr->i
  * as only things we have there are just static areas for
  * return values we don't _need_ to do anything but 
  * this is good practice:
  */
 memset(&athr->i,0,sizeof(athr->i));

#endif
#endif
}

void
Perl_set_thread_self(struct perl_thread *thr)
{
#ifdef USE_THREADS
    /* Set thr->self.  GetCurrentThread() retrurns a pseudo handle, need
       this to convert it into a handle another thread can use.
     */
    DuplicateHandle(GetCurrentProcess(),
		    GetCurrentThread(),
		    GetCurrentProcess(),
		    &thr->self,
		    0,
		    FALSE,
		    DUPLICATE_SAME_ACCESS);
#endif
}

#ifdef USE_THREADS
int
Perl_thread_create(struct perl_thread *thr, thread_func_t *fn)
{
    DWORD junk;
    unsigned long th;

    DEBUG_S(PerlIO_printf(PerlIO_stderr(),
			  "%p: create OS thread\n", thr));
#ifdef USE_RTL_THREAD_API
    /* See comment about USE_RTL_THREAD_API in win32thread.h */
#if defined(__BORLANDC__)
    th = _beginthreadNT(fn,				/* start address */
			0,				/* stack size */
			(void *)thr,			/* parameters */
			(void *)NULL,			/* security attrib */
			0,				/* creation flags */
			(unsigned long *)&junk);	/* tid */
    if (th == (unsigned long)-1)
	th = 0;
#elif defined(_MSC_VER_)
    th = _beginthreadex((void *)NULL,			/* security attrib */
			0,				/* stack size */
			fn,				/* start address */
			(void*)thr,			/* parameters */
			0,				/* creation flags */
			(unsigned *)&junk);		/* tid */
#else /* compilers using CRTDLL.DLL only have _beginthread() */
    th = _beginthread(fn,				/* start address */
		      0,				/* stack size */
		      (void*)thr);			/* parameters */
    if (th == (unsigned long)-1)
	th = 0;
#endif
    thr->self = (HANDLE)th;
#else	/* !USE_RTL_THREAD_API */
    thr->self = CreateThread(NULL, 0, fn, (void*)thr, 0, &junk);
#endif	/* !USE_RTL_THREAD_API */
    DEBUG_S(PerlIO_printf(PerlIO_stderr(),
			  "%p: OS thread = %p, id=%ld\n", thr, thr->self, junk));
    return thr->self ? 0 : -1;
}
#endif

