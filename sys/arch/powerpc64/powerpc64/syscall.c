#include <sys/param.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/syscall.h>
#include <sys/syscall_mi.h>

void
child_return(void *arg)
{
	struct proc *p = (struct proc *)arg;

	mi_child_return(p);
}
