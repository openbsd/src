/*	$OpenBSD: netbsd_misc.c,v 1.1 1999/09/15 18:36:38 kstailey Exp $	*/

#include <sys/param.h>
/* #include <sys/systm.h> */
#include <sys/proc.h>

#include <compat/netbsd/netbsd_types.h>
#include <compat/netbsd/netbsd_signal.h>
#include <compat/netbsd/netbsd_syscallargs.h>

/* XXX doesn't do shared address space */
int
netbsd_sys___vfork14(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	return (fork1(p, ISVFORK, 0, NULL, 0, retval));
}
