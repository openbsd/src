#include <machine/asm.h>

/*
 * Switch stacks
 */

/* void _thread_machdep_switch(new, oldsave); */
ENTRY(_thread_machdep_switch)
	/* dummy */
	ret
