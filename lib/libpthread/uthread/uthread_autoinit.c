
#include <stdio.h>
#include <pthread.h>
#include "pthread_private.h"

extern void _thread_init __P((void));

#ifdef __cplusplus
/*
 * Use C++ static initialiser
 */
class Init {
public:
	Init() { _thread_init(); }
};
Init _thread_initialiser;
#endif /* C++ */

/*
 * a.out ld.so initialisation
 */
extern void _thread_dot_init __P((void)) asm(".init");
void 
_thread_dot_init()
{ 
	_thread_init();
}

#ifdef mips
/*
 * elf ld.so initialisation
 */
extern int _init() __attribute__((constructor,section (".dynamic")));
int 
_init()
{ 
	_thread_init();
	return 0; 
}
#endif /* mips */

#ifdef _GNUC_
/*
 * GNU CTOR_LIST constructor
 */
void _thread_init_constructor __P((void)) __attribute__((constructor));
void
_thread_init_constructor()
{
	_thread_init();
}
#endif /* GNU C */

