
#include <sys/param.h>
#include <sys/proc.h>

#include <machine/db_machdep.h>
void
Debugger()
{
	db_trap(T_BREAKPOINT);
/*
	__asm volatile ("tw 4,2,2");
*/
}
