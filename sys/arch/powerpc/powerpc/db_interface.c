/*	$OpenBSD: db_interface.c,v 1.2 1996/12/28 06:21:50 rahnds Exp $	*/

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
