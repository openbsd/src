/*	$OpenBSD: uthread_vfork.c,v 1.2 1999/11/25 07:01:47 d Exp $	*/
#include <unistd.h>
#ifdef _THREAD_SAFE

int
vfork(void)
{
	return (fork());
}
#endif
