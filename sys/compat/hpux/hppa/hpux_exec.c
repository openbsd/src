/*	$OpenBSD: hpux_exec.c,v 1.5 2010/06/26 23:24:44 guenther Exp $	*/

/*
 * Copyright (c) 2004 Michael Shalayeff.  All rights reserved.
 * Copyright (c) 1995, 1997 Jason R. Thorpe.  All rights reserved.
 * Copyright (c) 1993, 1994 Christopher G. Demetriou
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
 *	This product includes software developed by Christopher G. Demetriou.
 * 4. The name of the author may not be used to endorse or promote products
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
 */

/*
 * Glue for exec'ing HP-UX executables and the HP-UX execv() system call.
 * Based on sys/kern/exec_aout.c
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/core.h>

#include <uvm/uvm_extern.h>

#include <machine/cpu.h>
#include <machine/reg.h>

#include <sys/syscallargs.h>    

#include <compat/hpux/hpux.h>
#include <compat/hpux/hpux_util.h>
#include <compat/hpux/hppa/hpux_syscall.h>
#include <compat/hpux/hppa/hpux_syscallargs.h>

#include <machine/hpux_machdep.h>

const char hpux_emul_path[] = "/emul/hpux";
extern char hpux_sigcode[], hpux_esigcode[];
extern struct sysent hpux_sysent[];
#ifdef SYSCALL_DEBUG
extern char *hpux_syscallnames[];
#endif
extern int bsdtohpuxerrnomap[];

int exec_hpux_som_nmagic(struct proc *, struct exec_package *);
int exec_hpux_som_zmagic(struct proc *, struct exec_package *);
int exec_hpux_som_omagic(struct proc *, struct exec_package *);

struct emul emul_hpux = {
	"hpux",
	bsdtohpuxerrnomap,
	hpux_sendsig,
	HPUX_SYS_syscall,
	HPUX_SYS_MAXSYSCALL,
	hpux_sysent,
#ifdef SYSCALL_DEBUG
	hpux_syscallnames,
#else
	NULL,
#endif
	0,
	copyargs,
	hpux_setregs,
	NULL,
	coredump_trad,
	hpux_sigcode,
	hpux_esigcode,
};

int
exec_hpux_makecmds(p, epp)
	struct proc *p;
	struct exec_package *epp;
{
	struct som_exec *som_ep = epp->ep_hdr;
	short sysid, magic;
	int error = ENOEXEC;

	sysid = HPUX_SYSID(som_ep);
	if (sysid != MID_HPUX800 && sysid != MID_HPPA11 && sysid != MID_HPPA20)
		return (error);

	/* XXX read in the aux header if it was not following the som header */
	if (sysid != MID_HPUX && (!(som_ep->som_version == HPUX_SOM_V0 ||
	    som_ep->som_version == HPUX_SOM_V1) ||
	    som_ep->som_auxhdr + sizeof(struct som_aux) > epp->ep_hdrvalid)) {
		return (error);
	}

	/*
	 * HP-UX is a 4k page size system, and executables assume
	 * this.
	 */
	if (PAGE_SIZE != HPUX_LDPGSZ)
		return (error);

	magic = HPUX_MAGIC(som_ep);
	switch (magic) {
	case OMAGIC:
		error = exec_hpux_som_omagic(p, epp);
		break;

	case NMAGIC:
		error = exec_hpux_som_nmagic(p, epp);
		break;

	case ZMAGIC:
		error = exec_hpux_som_zmagic(p, epp);
		break;
	}

	if (error == 0) {
		/* set up our emulation information */
		epp->ep_emul = &emul_hpux;
	} else
		kill_vmcmds(&epp->ep_vmcmds);

	return (error);
}

int
exec_hpux_som_nmagic(struct proc *p, struct exec_package *epp)
{
	struct som_exec *execp = epp->ep_hdr;
	struct som_aux *auxp = epp->ep_hdr + execp->som_auxhdr;

	epp->ep_taddr = auxp->som_tmem;
	epp->ep_tsize = auxp->som_tsize;
	epp->ep_daddr = auxp->som_dmem;
	epp->ep_dsize = auxp->som_dsize + auxp->som_bsize;
	epp->ep_entry = auxp->som_entry;

	/* set up command for text segment */
	NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_readvn, epp->ep_tsize,
	    epp->ep_taddr, epp->ep_vp, auxp->som_tfile,
	    VM_PROT_READ|VM_PROT_EXECUTE);

	/* set up command for data segment */
	NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_readvn, auxp->som_dsize,
	    epp->ep_daddr, epp->ep_vp, auxp->som_dfile,
	    VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE);

	/* set up command for bss segment */
	if (auxp->som_bsize > 0)
		NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_zero, auxp->som_bsize,
		    epp->ep_daddr + auxp->som_dsize,
		    NULLVP, 0, VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE);

	return (exec_setup_stack(p, epp));
}

int
exec_hpux_som_zmagic(struct proc *p, struct exec_package *epp)
{

	return (exec_setup_stack(p, epp));
}

int
exec_hpux_som_omagic(struct proc *p, struct exec_package *epp)
{

	return (exec_setup_stack(p, epp));
}

/*
 * The HP-UX execv(2) system call.
 *
 * Just check the alternate emulation path, and pass it on to the NetBSD
 * execve().
 */
int
hpux_sys_execv(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct hpux_sys_execv_args /* {
		syscallarg(char *) path;
		syscallarg(char **) argv;
	} */ *uap = v;
	struct sys_execve_args ap;
	caddr_t sg;

	sg = stackgap_init(p->p_emul);
	HPUX_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));

	SCARG(&ap, path) = SCARG(uap, path);
	SCARG(&ap, argp) = SCARG(uap, argp);
	SCARG(&ap, envp) = NULL;

	return sys_execve(p, &ap, retval);
}

int
hpux_sys_execve(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct hpux_sys_execve_args /* {
		syscallarg(char *) path;
		syscallarg(char **) argv;
		syscallarg(char **) envp;
        } */ *uap = v;
	struct sys_execve_args ap;
	caddr_t sg;

	sg = stackgap_init(p->p_emul);
	HPUX_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));

	SCARG(&ap, path) = SCARG(uap, path);
	SCARG(&ap, argp) = SCARG(uap, argp);
	SCARG(&ap, envp) = SCARG(uap, envp);

	return (sys_execve(p, &ap, retval));
}
