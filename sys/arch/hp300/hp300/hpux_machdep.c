/*	$NetBSD: hpux_machdep.c,v 1.2 1996/02/14 02:56:45 thorpej Exp $	*/

/*
 * Copyright (c) 1995, 1996 Jason R. Thorpe.  All rights reserved.
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 */

/*
 * Machinde-dependent bits for HP-UX binary compatibility.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/signalvar.h>
#include <sys/kernel.h>
#include <sys/filedesc.h>
#include <sys/proc.h> 
#include <sys/buf.h>
#include <sys/wait.h> 
#include <sys/file.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/ioctl.h>
#include <sys/ptrace.h>
#include <sys/stat.h>
#include <sys/syslog.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/ipc.h>
#include <sys/namei.h>
#include <sys/user.h>
#include <sys/mman.h>
#include <sys/conf.h>

#include <machine/cpu.h>
#include <machine/reg.h>
#include <machine/psl.h>
#include <machine/vmparam.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_map.h> 

#include <machine/cpu.h> 
#include <machine/reg.h>

#include <sys/syscallargs.h>

#include <compat/hpux/hpux.h>
#include <compat/hpux/hpux_util.h>
#include <compat/hpux/hpux_syscall.h>
#include <compat/hpux/hpux_syscallargs.h>

#include <machine/hpux_machdep.h>

#define NHIL	1	/* XXX */
#include "grf.h"

#if NGRF > 0
extern	int grfopen __P((dev_t dev, int oflags, int devtype, struct proc *p));
#endif

#if NHIL > 0
extern	int hilopen __P((dev_t dev, int oflags, int devtype, struct proc *p));
#endif

static	struct {
	int	machine_id;
	char	*machine_str;
} machine_table[] = {
	{ HP_320,	"320" },
	{ HP_330,	"330" },	/* includes 318 and 319 */
	{ HP_340,	"340" },
	{ HP_350,	"350" },
	{ HP_360,	"360" },
	{ HP_370,	"370" },
	{ HP_375,	"375" },	/* includes 345 and 400 */
	{ HP_380,	"380" },	/* includes 425 */
	{ HP_433,	"433" },
	{     -1,	"3?0" },	/* unknown system (???) */
};

/* 6.0 and later style context */
#ifdef M68040
static char hpux_040context[] =
    "standalone HP-MC68040 HP-MC68881 HP-MC68020 HP-MC68010 localroot default";
#endif
#ifdef FPCOPROC
static char hpux_context[] =
    "standalone HP-MC68881 HP-MC68020 HP-MC68010 localroot default";
#else
static char hpux_context[] =
    "standalone HP-MC68020 HP-MC68010 localroot default";
#endif

#define UOFF(f)		((int)&((struct user *)0)->f)
#define HPUOFF(f)	((int)&((struct hpux_user *)0)->f)

/* simplified FP structure */
struct bsdfp {
	int save[54];
	int reg[24];
	int ctrl[3];
};

/*
 * m68k-specific setup for HP-UX executables.
 * XXX m68k/m68k/hpux_machdep.c?
 */
int
hpux_cpu_makecmds(p, epp)
	struct proc *p;
	struct exec_package *epp;
{
	struct hpux_exec *hpux_ep = epp->ep_hdr;

	/* set up command for exec header */
	NEW_VMCMD(&epp->ep_vmcmds, hpux_cpu_vmcmd,
	    sizeof(struct hpux_exec), (long)epp->ep_hdr, NULLVP, 0, 0);
}

/*
 * We need to stash the exec header in the pcb, so we define
 * this vmcmd to do it for us, since vmcmds are executed once
 * we're committed to the exec (i.e. the old program has been unmapped).
 *
 * The address of the header is in ev->ev_addr and the length is
 * in ev->ev_len.
 */
int
hpux_cpu_vmcmd(p, ev)
	struct proc *p;
	struct exec_vmcmd *ev;
{
	struct hpux_exec *execp = (struct hpux_exec *)ev->ev_addr;

	/* Make sure we have room. */
	if (ev->ev_len <= sizeof(p->p_addr->u_md.md_exec))
		bcopy((caddr_t)ev->ev_addr, p->p_addr->u_md.md_exec,
		    ev->ev_len);

	/* Deal with misc. HP-UX process attributes. */
	if (execp->ha_trsize & HPUXM_VALID) {
		if (execp->ha_trsize & HPUXM_DATAWT)
			p->p_md.md_flags &= ~MDP_CCBDATA;

		if (execp->ha_trsize & HPUXM_STKWT)
			p->p_md.md_flags & ~MDP_CCBSTACK;
	}

	return (0);
}

/*
 * Machine-dependent stat structure conversion.
 */
void
hpux_cpu_bsd_to_hpux_stat(sb, hsb)
	struct stat *sb;
	struct hpux_stat *hsb;
{

	/* XXX I don't want to talk about it... */
	if ((sb->st_mode & S_IFMT) == S_IFCHR) {
#if NGRF > 0
		if (cdevsw[major(sb->st_rdev)].d_open == grfopen)
			hsb->hst_rdev = grfdevno(sb->st_rdev);
#endif
#if NHIL > 0
		if (cdevsw[major(sb->st_rdev)].d_open == hilopen)
			hsb->hst_rdev = hildevno(sb->st_rdev);
#endif
	}
}

/*
 * Machine-dependent uname information.
 */
void
hpux_cpu_uname(ut)
	struct hpux_utsname *ut;
{
	int i;

	bzero(ut->machine, sizeof(ut->machine));

	/*
	 * Find the current machine-ID in the table and
	 * copy the string into the uname.
	 */
	for (i = 0; machine_table[i].machine_id != -1; ++i)
		if (machine_table[i].machine_id == machineid)
			break;

	sprintf(ut->machine, "9000/%s", machine_table[i].machine_str);
}

/*
 * Return arch-type for hpux_sys_sysconf()
 */
int
hpux_cpu_sysconf_arch()
{

	switch (machineid) {
	case HP_320:
	case HP_330:
	case HP_350:
		return (HPUX_SYSCONF_CPUM020);

	case HP_340:
	case HP_360:
	case HP_370:
	case HP_375:
		return (HPUX_SYSCONF_CPUM030);

	case HP_380:
	case HP_433:
		return (HPUX_SYSCONF_CPUM040);

	default:
		return (HPUX_SYSCONF_CPUM020);	/* ??? */
	}
	/* NOTREACHED */
}

/*
 * HP-UX advise(2) system call.
 */
int
hpux_sys_advise(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct hpux_sys_advise_args *uap = v;
	int error = 0;

	switch (SCARG(uap, arg)) {
	case 0:
		p->p_md.md_flags |= MDP_HPUXMMAP; 
		break;

	case 1:
		ICIA();
		break;

	case 2:
		DCIA();
		break;

	default:
		error = EINVAL;
		break;
	}

	return (error);
}

/*
 * HP-UX getcontext(2) system call.
 * Man page lies, behaviour here is based on observed behaviour.
 */
int
hpux_sys_getcontext(p, v, retval)
	struct proc *p; 
	void *v;
	register_t *retval; 
{
	struct hpux_sys_getcontext_args *uap = v;
	int error = 0;
	register int len; 

#ifdef M68040
	if ((machineid == HP_380) || (machineid == HP_433)) {
		len = min(SCARG(uap, len), sizeof(hpux_040context));
		if (len)
			error = copyout(hpux_040context, SCARG(uap, buf), len);
		if (error == 0)
			*retval = sizeof(hpux_040context);
		return (error);
	}
#endif
	len = min(SCARG(uap, len), sizeof(hpux_context));
	if (len)
		error = copyout(hpux_context, SCARG(uap, buf), (u_int)len);
	if (error == 0)
		*retval = sizeof(hpux_context);
	return (error);
}

/*
 * Brutal hack!  Map HP-UX u-area offsets into BSD k-stack offsets.
 * XXX This probably doesn't work anymore, BTW.  --thorpej
 */
int
hpux_to_bsd_uoff(off, isps, p)
	int *off, *isps; 
	struct proc *p;
{
	register int *ar0 = p->p_md.md_regs;
	struct hpux_fp *hp; 
	struct bsdfp *bp;
	register u_int raddr;

	*isps = 0;

	/* u_ar0 field; procxmt puts in U_ar0 */
	if ((int)off == HPUOFF(hpuxu_ar0))
		return(UOFF(U_ar0)); 


#ifdef FPCOPROC
	/* FP registers from PCB */
	hp = (struct hpux_fp *)HPUOFF(hpuxu_fp);
	bp = (struct bsdfp *)UOFF(u_pcb.pcb_fpregs);

	if (off >= hp->hpfp_ctrl && off < &hp->hpfp_ctrl[3])
		return((int)&bp->ctrl[off - hp->hpfp_ctrl]);

	if (off >= hp->hpfp_reg && off < &hp->hpfp_reg[24])
		return((int)&bp->reg[off - hp->hpfp_reg]);
#endif

	/*
	 * Everything else we recognize comes from the kernel stack,
	 * so we convert off to an absolute address (if not already)
	 * for simplicity.
	 */
	if (off < (int *)ctob(UPAGES))
		off = (int *)((u_int)off + (u_int)p->p_addr);	/* XXX */

	/*
	 * General registers.
	 * We know that the HP-UX registers are in the same order as ours.
	 * The only difference is that their PS is 2 bytes instead of a
	 * padded 4 like ours throwing the alignment off.
	 */
	if (off >= ar0 && off < &ar0[18]) {
		/*
		 * PS: return low word and high word of PC as HP-UX would
		 * (e.g. &u.u_ar0[16.5]).
		 *
		 * XXX we don't do this since HP-UX adb doesn't rely on
		 * it and passing such an offset to procxmt will cause
		 * it to fail anyway.  Instead, we just set the offset
		 * to PS and let hpux_ptrace() shift up the value returned.
		 */
		if (off == &ar0[PS]) {
#if 0
			raddr = (u_int) &((short *)ar0)[PS*2+1];
#else
			raddr = (u_int) &ar0[(int)(off - ar0)];
#endif
			*isps = 1;
		}
		/*
		 * PC: off will be &u.u_ar0[16.5] since HP-UX saved PS
		 * is only 16 bits.
		 */
		else if (off == (int *)&(((short *)ar0)[PS*2+1]))
			raddr = (u_int) &ar0[PC];
		/*
		 * D0-D7, A0-A7: easy
		 */
		else
			raddr = (u_int) &ar0[(int)(off - ar0)];
		return((int)(raddr - (u_int)p->p_addr));	/* XXX */
	}

	/* everything else */
	return (-1);
}

/*
 * Kludge up a uarea dump so that HP-UX debuggers can find out
 * what they need.  IMPORTANT NOTE: we do not EVEN attempt to
 * convert the entire user struct.
 */
int
hpux_dumpu(vp, cred)
	struct vnode *vp;
	struct ucred *cred;
{
	int error = 0;
	struct proc *p = curproc;
	struct hpux_user *faku;
	struct bsdfp *bp;
	short *foop;

	/*
	 * Make sure there is no mistake about this being a real
	 * user structure.
	 */
	faku = (struct hpux_user *)malloc((u_long)ctob(1), M_TEMP, M_WAITOK);
	bzero((caddr_t)faku, ctob(1));

	/* Fill in the process sizes. */
	faku->hpuxu_tsize = p->p_vmspace->vm_tsize;
	faku->hpuxu_dsize = p->p_vmspace->vm_dsize;
	faku->hpuxu_ssize = p->p_vmspace->vm_ssize;

	/*
	 * Fill in the exec header for CDB.
	 * This was saved back in exec().  As far as I can tell CDB
	 * only uses this information to verify that a particular
	 * core file goes with a particular binary.
	 */
	bcopy((caddr_t)p->p_addr->u_md.md_exec,
	    (caddr_t)&faku->hpuxu_exdata, sizeof (struct hpux_exec));

	/*
	 * Adjust user's saved registers (on kernel stack) to reflect
	 * HP-UX order.  Note that HP-UX saves the SR as 2 bytes not 4
	 * so we have to move it up.
	 */
	faku->hpuxu_ar0 = p->p_md.md_regs;
	foop = (short *) p->p_md.md_regs;
	foop[32] = foop[33];
	foop[33] = foop[34];
	foop[34] = foop[35];

#ifdef FPCOPROC
	/*
	 * Copy 68881 registers from our PCB format to HP-UX format
	 */
	bp = (struct bsdfp *) &p->p_addr->u_pcb.pcb_fpregs;
	bcopy((caddr_t)bp->save, (caddr_t)faku->hpuxu_fp.hpfp_save,
	    sizeof(bp->save));
	bcopy((caddr_t)bp->ctrl, (caddr_t)faku->hpuxu_fp.hpfp_ctrl,
	    sizeof(bp->ctrl));
	bcopy((caddr_t)bp->reg, (caddr_t)faku->hpuxu_fp.hpfp_reg,
	    sizeof(bp->reg));
#endif

	/*
	 * Slay the dragon
	 */
	faku->hpuxu_dragon = -1;

	/*
	 * Dump this artfully constructed page in place of the
	 * user struct page.
	 */
	error = vn_rdwr(UIO_WRITE, vp, (caddr_t)faku, ctob(1), (off_t)0,
	    UIO_SYSSPACE, IO_NODELOCKED|IO_UNIT, cred, (int *)NULL, p);

	/*
	 * Dump the remaining UPAGES-1 pages normally
	 * XXX Spot the wild guess.
	 */
	if (error == 0)
		error = vn_rdwr(UIO_WRITE, vp, (caddr_t)p->p_addr + ctob(1),
		    ctob(UPAGES-1), (off_t)ctob(1), UIO_SYSSPACE,
		    IO_NODELOCKED|IO_UNIT, cred, (int *)NULL, p);

	free((caddr_t)faku, M_TEMP);

	return (error);
}
