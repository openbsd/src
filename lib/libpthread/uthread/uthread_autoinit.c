/*
 * David Leonard, 1998. Public Domain. <david.leonard@csee.uq.edu.au>
 *
 * $OpenBSD: uthread_autoinit.c,v 1.11 2003/01/31 04:46:17 marc Exp $
 */


#include <stdio.h>
#include <pthread.h>
#include "pthread_private.h"

/*
 * Use C++'s static instance constructor to initialise threads.
 */
#ifdef __cplusplus
class Init {
public:
	Init() { 
		_thread_init();
	}
};
Init _thread_initialiser;
#endif /* C++ */

/*
 * This construct places the function in the __CTOR_LIST__ entry in the
 * object, and later the collect2 stage of linkage will inform __main (from
 * libgcc.a) to call it.
 */
#if defined(__GNUC__)
extern void _thread_init_constructor(void) __attribute__((constructor));
extern void _GLOBAL_$I$_thread_init_constructor(void);

void
_thread_init_constructor()
{
	_thread_init();
}
#endif /* GNU C */

/*
 * Dummy symbol referenced by uthread_init.o so this compilation unit 
 * is always loaded from archives.
 */
int _thread_autoinit_dummy_decl = 0;

