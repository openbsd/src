/*	$OpenBSD: uthread_vfork.c,v 1.3 2007/11/20 19:35:37 deraadt Exp $	*/
#include <unistd.h>
#ifdef _THREAD_SAFE

pid_t	_dofork(int vfork);

pid_t
vfork(void)
{
	return (_dofork(1));
}
#endif
