/*
 * David Leonard, 1998. Public Domain. <david.leonard@csee.uq.edu.au>
 *
 * $OpenBSD: uthread_autoinit.c,v 1.5 1999/02/02 01:36:00 imp Exp $
 */

#include <stdio.h>
#include <pthread.h>
#include "pthread_private.h"

__BEGIN_DECLS
extern void _thread_init __P((void));
__END_DECLS

#ifdef DEBUG
#define init_debug(m)	stderr_debug(m)
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
		init_debug("init: C++\n");
		_thread_init();
	}
};
Init _thread_initialiser;
#endif /* C++ */

/*
 * The a.out ld.so dynamic linker calls the function
 * at symbol ".init" if it exists, just after linkage.
 */
extern void _thread_dot_init __P((void)) asm(".init");
void 
_thread_dot_init()
{ 
	init_debug("init: a.out .init\n");
	_thread_init();
}

/*
 * The GNU ELF loader will place a function called _init
 * found in the .dynamic section into the _INIT field. This then gets
 * automatically run by GNU ELF's ld.so.
 */
#ifdef pmax
extern int _init() __attribute__((constructor,section (".dynamic")));
int 
_init()
{ 
	init_debug("init: elf _init\n");
	_thread_init();
	return 0; 
}
#endif /* mips */

/*
 * A GNU C installation may know how to automatically run
 * constructors for other architectures. (It doesn't matter if 
 * we initialise multiple times.)  This construct places
 * the function in the __CTOR_LIST__ entry in the object, and later
 * the collect2 stage of linkage will inform __main (from libgcc.a)
 * to call it.
 */
#ifdef _GNUC_
void _thread_init_constructor __P((void)) __attribute__((constructor));
void
_thread_init_constructor()
{
	init_debug("init: GNU constructor");
	_thread_init();
}
#endif /* GNU C */

/*
 * Dummy symbol referenced by uthread_init.o so this compilation unit 
 * is always loaded.
 */
int _thread_autoinit_dummy_decl = 0;
