/*
 * David Leonard, 1998. Public Domain. <david.leonard@csee.uq.edu.au>
 *
 * $OpenBSD: uthread_autoinit.c,v 1.10 2002/02/16 21:27:25 millert Exp $
 */

#include <stdio.h>
#include <pthread.h>
#include "pthread_private.h"

__BEGIN_DECLS
extern void _thread_init(void);
__END_DECLS

#ifdef DEBUG
#define init_debug(m)	stderr_debug( "[init method: " m "]\n")
#else
#define init_debug(m)	/* nothing */
#endif

/*
 * Use C++'s static instance constructor to initialise threads.
 */
#ifdef __cplusplus
class Init {
public:
	Init() { 
		init_debug("C++");
		_thread_init();
	}
};
Init _thread_initialiser;
#endif /* C++ */

/*
 * The a.out ld.so dynamic linker calls the function
 * at symbol ".init" if it exists, just after linkage.
 */
extern void _thread_dot_init(void) asm(".init");
void 
_thread_dot_init()
{ 
	init_debug("a.out .init");
	_thread_init();
}

/*
 * A GNU C installation may know how to automatically run
 * constructors for other architectures. (It doesn't matter if 
 * we initialise multiple times.)  This construct places
 * the function in the __CTOR_LIST__ entry in the object, and later
 * the collect2 stage of linkage will inform __main (from libgcc.a)
 * to call it.
 */
#if defined(__GNUC__) /* && defined(notyet) */ /* internal compiler error??? */
void _thread_init_constructor(void) __attribute__((constructor));
void
_thread_init_constructor()
{
	init_debug("GNU constructor");
	_thread_init();
}
#endif /* GNU C */

/*
 * Dummy symbol referenced by uthread_init.o so this compilation unit 
 * is always loaded from archives.
 */
int _thread_autoinit_dummy_decl = 0;

