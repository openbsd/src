/*
 *	$OpenBSD: uthread_vfork.c,v 1.1 1998/11/09 03:13:21 d Exp $
 */
#include <unistd.h>
#ifdef _THREAD_SAFE

int
vfork(void)
{
	return (fork());
}
#endif /* _THREAD_SAFE */
