/*	$OpenBSD: linux_exec.c,v 1.10 1999/02/10 08:07:20 deraadt Exp $	*/
/*	$NetBSD: linux_exec.c,v 1.13 1996/04/05 00:01:10 christos Exp $	*/

/*
 * Copyright (c) 1995 Frank van der Linden
 * Copyright (c) 1994 Christos Zoulas
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * based on exec_aout.c, sunos_exec.c and svr4_exec.c
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/exec_elf.h>
#include <sys/exec_olf.h>

#include <sys/mman.h>
#include <sys/syscallargs.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_map.h>

#include <machine/cpu.h>
#include <machine/reg.h>
#include <machine/exec.h>
#include <machine/linux_machdep.h>

#include <compat/linux/linux_types.h>
#include <compat/linux/linux_syscall.h>
#include <compat/linux/linux_signal.h>
#include <compat/linux/linux_syscallargs.h>
#include <compat/linux/linux_util.h>
#include <compat/linux/linux_exec.h>

static void *linux_aout_copyargs __P((struct exec_package *,
	struct ps_strings *, void *, void *));

#define	LINUX_AOUT_AUX_ARGSIZ	2
#define LINUX_ELF_AUX_ARGSIZ (sizeof(AuxInfo) * 8 / sizeof(char *))


const char linux_emul_path[] = "/emul/linux";
extern int linux_error[];
extern char linux_sigcode[], linux_esigcode[];
extern struct sysent linux_sysent[];
extern char *linux_syscallnames[];

int exec_linux_aout_prep_zmagic __P((struct proc *, struct exec_package *));
int exec_linux_aout_prep_nmagic __P((struct proc *, struct exec_package *));
int exec_linux_aout_prep_omagic __P((struct proc *, struct exec_package *));
int exec_linux_aout_prep_qmagic __P((struct proc *, struct exec_package *));

struct emul emul_linux_aout = {
	"linux",
	linux_error,
	linux_sendsig,
	LINUX_SYS_syscall,
	LINUX_SYS_MAXSYSCALL,
	linux_sysent,
	linux_syscallnames,
	LINUX_AOUT_AUX_ARGSIZ,
	linux_aout_copyargs,
	setregs,
	NULL,
	linux_sigcode,
	linux_esigcode,
};

struct emul emul_linux_elf = {
	"linux",
	linux_error,
	linux_sendsig,
	LINUX_SYS_syscall,
	LINUX_SYS_MAXSYSCALL,
	linux_sysent,
	linux_syscallnames,
	LINUX_ELF_AUX_ARGSIZ,
	elf_copyargs,
	setregs,
	exec_elf_fixup,
	linux_sigcode,
	linux_esigcode,
};


static void *
linux_aout_copyargs(pack, arginfo, stack, argp)
	struct exec_package *pack;
	struct ps_strings *arginfo;
	void *stack;
	void *argp;
{
	char **cpp = stack;
	char **stk = stack;
	char *dp, *sp;
	size_t len;
	void *nullp = NULL;
	int argc = arginfo->ps_nargvstr;
	int envc = arginfo->ps_nenvstr;

	if (copyout(&argc, cpp++, sizeof(argc)))
		return (NULL);

	/* leave room for envp and argv */
	cpp += 2;
	if (copyout(&cpp, &stk[1], sizeof (cpp)))
		return (NULL);

	dp = (char *)(cpp + argc + envc + 2);
	sp = argp;

	/* XXX don't copy them out, remap them! */
	arginfo->ps_argvstr = cpp; /* remember location of argv for later */

	for (; --argc >= 0; sp += len, dp += len)
		if (copyout(&dp, cpp++, sizeof(dp)) ||
		    copyoutstr(sp, dp, ARG_MAX, &len))
			return (NULL);

	if (copyout(&nullp, cpp++, sizeof(nullp)))
		return (NULL);

	if (copyout(&cpp, &stk[2], sizeof (cpp)))
		return (NULL);

	arginfo->ps_envstr = cpp; /* remember location of envp for later */

	for (; --envc >= 0; sp += len, dp += len)
		if (copyout(&dp, cpp++, sizeof(dp)) ||
		    copyoutstr(sp, dp, ARG_MAX, &len))
			return (NULL);

	if (copyout(&nullp, cpp++, sizeof(nullp)))
		return (NULL);

	return (cpp);
}

int
exec_linux_aout_makecmds(p, epp)
	struct proc *p;
	struct exec_package *epp;
{
	struct exec *linux_ep = epp->ep_hdr;
	int machtype, magic;
	int error = ENOEXEC;

	magic = LINUX_N_MAGIC(linux_ep);
	machtype = LINUX_N_MACHTYPE(linux_ep);


	if (machtype != LINUX_MID_MACHINE)
		return (ENOEXEC);

	switch (magic) {
	case QMAGIC:
		error = exec_linux_aout_prep_qmagic(p, epp);
		break;
	case ZMAGIC:
		error = exec_linux_aout_prep_zmagic(p, epp);
		break;
	case NMAGIC:
		error = exec_linux_aout_prep_nmagic(p, epp);
		break;
	case OMAGIC:
		error = exec_linux_aout_prep_omagic(p, epp);
		break;
	}
	if (error == 0)
		epp->ep_emul = &emul_linux_aout;
	return (error);
}

/*
 * Since text starts at 0x400 in Linux ZMAGIC executables, and 0x400
 * is very likely not page aligned on most architectures, it is treated
 * as an NMAGIC here. XXX
 */

int
exec_linux_aout_prep_zmagic(p, epp)
	struct proc *p;
	struct exec_package *epp;
{
	struct exec *execp = epp->ep_hdr;

	epp->ep_taddr = LINUX_N_TXTADDR(*execp, ZMAGIC);
	epp->ep_tsize = execp->a_text;
	epp->ep_daddr = LINUX_N_DATADDR(*execp, ZMAGIC);
	epp->ep_dsize = execp->a_data + execp->a_bss;
	epp->ep_entry = execp->a_entry;

	/* set up command for text segment */
	NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_readvn, execp->a_text,
	    epp->ep_taddr, epp->ep_vp, LINUX_N_TXTOFF(*execp, ZMAGIC),
	    VM_PROT_READ|VM_PROT_EXECUTE);

	/* set up command for data segment */
	NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_readvn, execp->a_data,
	    epp->ep_daddr, epp->ep_vp, LINUX_N_DATOFF(*execp, ZMAGIC),
	    VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE);

	/* set up command for bss segment */
	NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_zero, execp->a_bss,
	    epp->ep_daddr + execp->a_data, NULLVP, 0,
	    VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE);

	return (exec_setup_stack(p, epp));
}

/*
 * exec_aout_prep_nmagic(): Prepare Linux NMAGIC package.
 * Not different from the normal stuff.
 */

int
exec_linux_aout_prep_nmagic(p, epp)
	struct proc *p;
	struct exec_package *epp;
{
	struct exec *execp = epp->ep_hdr;
	long bsize, baddr;

	epp->ep_taddr = LINUX_N_TXTADDR(*execp, NMAGIC);
	epp->ep_tsize = execp->a_text;
	epp->ep_daddr = LINUX_N_DATADDR(*execp, NMAGIC);
	epp->ep_dsize = execp->a_data + execp->a_bss;
	epp->ep_entry = execp->a_entry;

	/* set up command for text segment */
	NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_readvn, execp->a_text,
	    epp->ep_taddr, epp->ep_vp, LINUX_N_TXTOFF(*execp, NMAGIC),
	    VM_PROT_READ|VM_PROT_EXECUTE);

	/* set up command for data segment */
	NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_readvn, execp->a_data,
	    epp->ep_daddr, epp->ep_vp, LINUX_N_DATOFF(*execp, NMAGIC),
	    VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE);

	/* set up command for bss segment */
	baddr = roundup(epp->ep_daddr + execp->a_data, NBPG);
	bsize = epp->ep_daddr + epp->ep_dsize - baddr;
	if (bsize > 0)
		NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_zero, bsize, baddr,
		    NULLVP, 0, VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE);

	return (exec_setup_stack(p, epp));
}

/*
 * exec_aout_prep_omagic(): Prepare Linux OMAGIC package.
 * Business as usual.
 */

int
exec_linux_aout_prep_omagic(p, epp)
	struct proc *p;
	struct exec_package *epp;
{
	struct exec *execp = epp->ep_hdr;
	long dsize, bsize, baddr;

	epp->ep_taddr = LINUX_N_TXTADDR(*execp, OMAGIC);
	epp->ep_tsize = execp->a_text;
	epp->ep_daddr = LINUX_N_DATADDR(*execp, OMAGIC);
	epp->ep_dsize = execp->a_data + execp->a_bss;
	epp->ep_entry = execp->a_entry;

	/* set up command for text and data segments */
	NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_readvn,
	    execp->a_text + execp->a_data, epp->ep_taddr, epp->ep_vp,
	    LINUX_N_TXTOFF(*execp, OMAGIC), VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE);

	/* set up command for bss segment */
	baddr = roundup(epp->ep_daddr + execp->a_data, NBPG);
	bsize = epp->ep_daddr + epp->ep_dsize - baddr;
	if (bsize > 0)
		NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_zero, bsize, baddr,
		    NULLVP, 0, VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE);

	/*
	 * Make sure (# of pages) mapped above equals (vm_tsize + vm_dsize);
	 * obreak(2) relies on this fact. Both `vm_tsize' and `vm_dsize' are
	 * computed (in execve(2)) by rounding *up* `ep_tsize' and `ep_dsize'
	 * respectively to page boundaries.
	 * Compensate `ep_dsize' for the amount of data covered by the last
	 * text page. 
	 */
	dsize = epp->ep_dsize + execp->a_text - roundup(execp->a_text, NBPG);
	epp->ep_dsize = (dsize > 0) ? dsize : 0;
	return (exec_setup_stack(p, epp));
}

int
exec_linux_aout_prep_qmagic(p, epp)
	struct proc *p;
	struct exec_package *epp;
{
	struct exec *execp = epp->ep_hdr;

	epp->ep_taddr = LINUX_N_TXTADDR(*execp, QMAGIC);
	epp->ep_tsize = execp->a_text;
	epp->ep_daddr = LINUX_N_DATADDR(*execp, QMAGIC);
	epp->ep_dsize = execp->a_data + execp->a_bss;
	epp->ep_entry = execp->a_entry;

	/*
	 * check if vnode is in open for writing, because we want to
	 * demand-page out of it.  if it is, don't do it, for various
	 * reasons
	 */
	if ((execp->a_text != 0 || execp->a_data != 0) &&
	    epp->ep_vp->v_writecount != 0) {
#ifdef DIAGNOSTIC
		if (epp->ep_vp->v_flag & VTEXT)
			panic("exec: a VTEXT vnode has writecount != 0");
#endif
		return (ETXTBSY);
	}
	epp->ep_vp->v_flag |= VTEXT;

	/* set up command for text segment */
	NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_pagedvn, execp->a_text,
	    epp->ep_taddr, epp->ep_vp, LINUX_N_TXTOFF(*execp, QMAGIC),
	    VM_PROT_READ|VM_PROT_EXECUTE);

	/* set up command for data segment */
	NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_pagedvn, execp->a_data,
	    epp->ep_daddr, epp->ep_vp, LINUX_N_DATOFF(*execp, QMAGIC),
	    VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE);

	/* set up command for bss segment */
	NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_zero, execp->a_bss,
	    epp->ep_daddr + execp->a_data, NULLVP, 0,
	    VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE);

	return (exec_setup_stack(p, epp));
}

int
linux_elf_probe(p, epp, itp, pos, os)
	struct proc *p;
	struct exec_package *epp;
	char *itp;
	u_long *pos;
	u_int8_t *os;
{
	Elf32_Ehdr *eh = epp->ep_hdr;
	char *bp, *brand;
	int error;
	size_t len;

	brand = elf_check_brand(eh);
	if (brand && strcmp(brand, "Linux"))
		return (EINVAL);
	if (itp[0]) {
		if ((error = emul_find(p, NULL, linux_emul_path, itp, &bp, 0)))
			return (error);
		if ((error = copystr(bp, itp, MAXPATHLEN, &len)))
			return (error);
		free(bp, M_TEMP);
	}
	epp->ep_emul = &emul_linux_elf;
	*pos = ELF32_NO_ADDR;
	if (*os == OOS_NULL)
		*os = OOS_LINUX;
	return (0);
}

/*
 * The Linux system call to load shared libraries, a.out version. The
 * a.out shared libs are just files that are mapped onto a fixed
 * address in the process' address space. The address is given in
 * a_entry. Read in the header, set up some VM commands and run them.
 *
 * Yes, both text and data are mapped at once, so we're left with
 * writeable text for the shared libs. The Linux crt0 seemed to break
 * sometimes when data was mapped seperately. It munmapped a uselib()
 * of ld.so by hand, which failed with shared text and data for ld.so
 * Yuck.
 *
 * Because of the problem with ZMAGIC executables (text starts
 * at 0x400 in the file, but needs to be mapped at 0), ZMAGIC
 * shared libs are not handled very efficiently :-(
 */

int
linux_sys_uselib(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_uselib_args /* {
		syscallarg(char *) path;
	} */ *uap = v;
	caddr_t sg;
	long bsize, dsize, tsize, taddr, baddr, daddr;
	struct nameidata ni;
	struct vnode *vp;
	struct exec hdr;
	struct exec_vmcmd_set vcset;
	int i, magic, error;
	size_t rem;

	sg = stackgap_init(p->p_emul);
	LINUX_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));

	NDINIT(&ni, LOOKUP, FOLLOW, UIO_USERSPACE, SCARG(uap, path), p);

	if ((error = namei(&ni)))
		return (error);

	vp = ni.ni_vp;

	if ((error = vn_rdwr(UIO_READ, vp, (caddr_t) &hdr, LINUX_AOUT_HDR_SIZE,
			     0, UIO_SYSSPACE, IO_NODELOCKED, p->p_ucred,
			     &rem, p))) {
		vrele(vp);
		return (error);
	}

	if (rem != 0) {
		vrele(vp);
		return (ENOEXEC);
	}

	if (LINUX_N_MACHTYPE(&hdr) != LINUX_MID_MACHINE)
		return (ENOEXEC);

	magic = LINUX_N_MAGIC(&hdr);
	taddr = hdr.a_entry & (~(NBPG - 1));
	tsize = hdr.a_text;
	daddr = taddr + tsize;
	dsize = hdr.a_data + hdr.a_bss;

	if ((hdr.a_text != 0 || hdr.a_data != 0) && vp->v_writecount != 0) {
		vrele(vp);
                return (ETXTBSY);
        }
	vp->v_flag |= VTEXT;

	vcset.evs_cnt = 0;
	vcset.evs_used = 0;

	NEW_VMCMD(
	    &vcset, magic == ZMAGIC ? vmcmd_map_readvn : vmcmd_map_pagedvn,
	    hdr.a_text + hdr.a_data, taddr, vp, LINUX_N_TXTOFF(hdr, magic),
	    VM_PROT_READ|VM_PROT_EXECUTE|VM_PROT_WRITE);

	baddr = roundup(daddr + hdr.a_data, NBPG);
	bsize = daddr + dsize - baddr;
        if (bsize > 0) {
                NEW_VMCMD(&vcset, vmcmd_map_zero, bsize, baddr,
                    NULLVP, 0, VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE);
	}

	for (i = 0; i < vcset.evs_used && !error; i++) {
		struct exec_vmcmd *vcp;

		vcp = &vcset.evs_cmds[i];
		error = (*vcp->ev_proc)(p, vcp);
	}

	kill_vmcmds(&vcset);

	vrele(vp);

	return (error);
}

/*
 * Execve(2). Just check the alternate emulation path, and pass it on
 * to the regular execve().
 */
int
linux_sys_execve(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_execve_args /* {
		syscallarg(char *) path;
		syscallarg(char **) argv;
		syscallarg(char **) envp;
        } */ *uap = v;
	struct sys_execve_args ap;
	caddr_t sg;

	sg = stackgap_init(p->p_emul);
	LINUX_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));

	SCARG(&ap, path) = SCARG(uap, path);
	SCARG(&ap, argp) = SCARG(uap, argp);
	SCARG(&ap, envp) = SCARG(uap, envp);

	return (sys_execve(p, &ap, retval));
}
