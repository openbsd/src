/*	$OpenBSD: process_machdep.c,v 1.1 1998/12/29 18:06:48 mickey Exp $	*/

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/ptrace.h>

int
process_read_regs(p, regs)
	struct proc *p;
	struct reg *regs;
{
	return EINVAL;
}

int
process_write_regs(p, regs)
	struct proc *p;
	struct reg *regs;
{
	return EINVAL;
}

int
process_read_fpregs(p, fpregs)
	struct proc *p;
	struct fpreg *fpregs;
{
	return EINVAL;
}

int
process_write_fpregs(p, fpregs)
	struct proc *p;
	struct fpreg *fpregs;
{
	return EINVAL;
}

int
process_sstep(p, sstep)
	struct proc *p;
	int sstep;
{
	return EINVAL;
}

int
process_set_pc(p, addr)
	struct proc *p;
	caddr_t addr;
{
	return EINVAL;
}
