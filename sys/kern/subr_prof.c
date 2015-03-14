/*	$OpenBSD: subr_prof.c,v 1.28 2015/03/14 03:38:50 jsg Exp $	*/
/*	$NetBSD: subr_prof.c,v 1.12 1996/04/22 01:38:50 christos Exp $	*/

/*-
 * Copyright (c) 1982, 1986, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)subr_prof.c	8.3 (Berkeley) 9/23/93
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/mount.h>
#include <sys/sysctl.h>
#include <sys/syscallargs.h>


#ifdef GPROF
#include <sys/malloc.h>
#include <sys/gmon.h>

#include <uvm/uvm_extern.h>

/*
 * Flag to prevent CPUs from executing the mcount() monitor function
 * until we're sure they are in a sane state.
 */
int gmoninit = 0;

extern char etext[];

void
kmstartup(void)
{
	CPU_INFO_ITERATOR cii;
	struct cpu_info *ci;
	struct gmonparam *p;
	u_long lowpc, highpc, textsize;
	u_long kcountsize, fromssize, tossize;
	long tolimit;
	char *cp;
	int size;

	/*
	 * Round lowpc and highpc to multiples of the density we're using
	 * so the rest of the scaling (here and in gprof) stays in ints.
	 */
	lowpc = ROUNDDOWN(KERNBASE, HISTFRACTION * sizeof(HISTCOUNTER));
	highpc = ROUNDUP((u_long)etext, HISTFRACTION * sizeof(HISTCOUNTER));
	textsize = highpc - lowpc;
	printf("Profiling kernel, textsize=%ld [%lx..%lx]\n",
	    textsize, lowpc, highpc);
	kcountsize = textsize / HISTFRACTION;
	fromssize = textsize / HASHFRACTION;
	tolimit = textsize * ARCDENSITY / 100;
	if (tolimit < MINARCS)
		tolimit = MINARCS;
	else if (tolimit > MAXARCS)
		tolimit = MAXARCS;
	tossize = tolimit * sizeof(struct tostruct);
	size = sizeof(*p) + kcountsize + fromssize + tossize;

	/* Allocate and initialize one profiling buffer per CPU. */
	CPU_INFO_FOREACH(cii, ci) {
		cp = km_alloc(round_page(size), &kv_any, &kp_zero, &kd_nowait);
		if (cp == NULL) {
			printf("No memory for profiling.\n");
			return;
		}

		p = (struct gmonparam *)cp;
		cp += sizeof(*p);
		p->tos = (struct tostruct *)cp;
		cp += tossize;
		p->kcount = (u_short *)cp;
		cp += kcountsize;
		p->froms = (u_short *)cp;

		p->state = GMON_PROF_OFF;
		p->lowpc = lowpc;
		p->highpc = highpc;
		p->textsize = textsize;
		p->hashfraction = HASHFRACTION;
		p->kcountsize = kcountsize;
		p->fromssize = fromssize;
		p->tolimit = tolimit;
		p->tossize = tossize;

		ci->ci_gmon = p;
	}
}

/*
 * Return kernel profiling information.
 */
int
sysctl_doprof(int *name, u_int namelen, void *oldp, size_t *oldlenp, void *newp,
    size_t newlen)
{
	CPU_INFO_ITERATOR cii;
	struct cpu_info *ci;
	struct gmonparam *gp = NULL;
	int error, cpuid, op;

	/* all sysctl names at this level are name and field */
	if (namelen != 2)
		return (ENOTDIR);		/* overloaded */

	op = name[0];
	cpuid = name[1];

	CPU_INFO_FOREACH(cii, ci) {
		if (cpuid == CPU_INFO_UNIT(ci)) {
			gp = ci->ci_gmon;
			break;
		}
	}

	if (gp == NULL)
		return (EOPNOTSUPP);

	/* Assume that if we're here it is safe to execute profiling. */
	gmoninit = 1;

	switch (op) {
	case GPROF_STATE:
		error = sysctl_int(oldp, oldlenp, newp, newlen, &gp->state);
		if (error)
			return (error);
		if (gp->state == GMON_PROF_OFF)
			stopprofclock(&process0);
		else
			startprofclock(&process0);
		return (0);
	case GPROF_COUNT:
		return (sysctl_struct(oldp, oldlenp, newp, newlen,
		    gp->kcount, gp->kcountsize));
	case GPROF_FROMS:
		return (sysctl_struct(oldp, oldlenp, newp, newlen,
		    gp->froms, gp->fromssize));
	case GPROF_TOS:
		return (sysctl_struct(oldp, oldlenp, newp, newlen,
		    gp->tos, gp->tossize));
	case GPROF_GMONPARAM:
		return (sysctl_rdstruct(oldp, oldlenp, newp, gp, sizeof *gp));
	default:
		return (EOPNOTSUPP);
	}
	/* NOTREACHED */
}
#endif /* GPROF */

/*
 * Profiling system call.
 *
 * The scale factor is a fixed point number with 16 bits of fraction, so that
 * 1.0 is represented as 0x10000.  A scale factor of 0 turns off profiling.
 */
/* ARGSUSED */
int
sys_profil(struct proc *p, void *v, register_t *retval)
{
	struct sys_profil_args /* {
		syscallarg(caddr_t) samples;
		syscallarg(size_t) size;
		syscallarg(u_long) offset;
		syscallarg(u_int) scale;
	} */ *uap = v;
	struct process *pr = p->p_p;
	struct uprof *upp;
	int s;

	if (SCARG(uap, scale) > (1 << 16))
		return (EINVAL);
	if (SCARG(uap, scale) == 0) {
		stopprofclock(pr);
		return (0);
	}
	upp = &pr->ps_prof;

	/* Block profile interrupts while changing state. */
	s = splstatclock();
	upp->pr_off = SCARG(uap, offset);
	upp->pr_scale = SCARG(uap, scale);
	upp->pr_base = (caddr_t)SCARG(uap, samples);
	upp->pr_size = SCARG(uap, size);
	startprofclock(pr);
	splx(s);

	return (0);
}

/*
 * Scale is a fixed-point number with the binary point 16 bits
 * into the value, and is <= 1.0.  pc is at most 32 bits, so the
 * intermediate result is at most 48 bits.
 */
#define	PC_TO_INDEX(pc, prof) \
	((int)(((u_quad_t)((pc) - (prof)->pr_off) * \
	    (u_quad_t)((prof)->pr_scale)) >> 16) & ~1)

/*
 * Collect user-level profiling statistics; called on a profiling tick,
 * when a process is running in user-mode.  This routine may be called
 * from an interrupt context. Schedule an AST that will vector us to
 * trap() with a context in which copyin and copyout will work.
 * Trap will then call addupc_task().
 */
void
addupc_intr(struct proc *p, u_long pc)
{
	struct uprof *prof;

	prof = &p->p_p->ps_prof;
	if (pc < prof->pr_off || PC_TO_INDEX(pc, prof) >= prof->pr_size)
		return;			/* out of range; ignore */

	p->p_prof_addr = pc;
	p->p_prof_ticks++;
	atomic_setbits_int(&p->p_flag, P_OWEUPC);
	need_proftick(p);
}


/*
 * Much like before, but we can afford to take faults here.  If the
 * update fails, we simply turn off profiling.
 */
void
addupc_task(struct proc *p, u_long pc, u_int nticks)
{
	struct process *pr = p->p_p;
	struct uprof *prof;
	caddr_t addr;
	u_int i;
	u_short v;

	/* Testing PS_PROFIL may be unnecessary, but is certainly safe. */
	if ((pr->ps_flags & PS_PROFIL) == 0 || nticks == 0)
		return;

	prof = &pr->ps_prof;
	if (pc < prof->pr_off ||
	    (i = PC_TO_INDEX(pc, prof)) >= prof->pr_size)
		return;

	addr = prof->pr_base + i;
	if (copyin(addr, (caddr_t)&v, sizeof(v)) == 0) {
		v += nticks;
		if (copyout((caddr_t)&v, addr, sizeof(v)) == 0)
			return;
	}
	stopprofclock(pr);
}
