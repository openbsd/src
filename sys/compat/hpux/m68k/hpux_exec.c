/*	$OpenBSD: hpux_exec.c,v 1.2 2005/12/30 19:46:55 miod Exp $	*/
/*	$NetBSD: hpux_exec.c,v 1.8 1997/03/16 10:14:44 thorpej Exp $	*/

/*
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
#include <sys/user.h>
#include <sys/vnode.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <uvm/uvm_extern.h>

#include <machine/cpu.h>
#include <machine/reg.h>

#include <sys/syscallargs.h>    

#include <compat/hpux/hpux.h>
#include <compat/hpux/hpux_util.h>
#include <compat/hpux/m68k/hpux_syscall.h>
#include <compat/hpux/m68k/hpux_syscallargs.h>

#include <machine/hpux_machdep.h>

const char hpux_emul_path[] = "/emul/hpux";
extern char sigcode[], esigcode[];
extern struct sysent hpux_sysent[];
#ifdef SYSCALL_DEBUG
extern char *hpux_syscallnames[];
#endif
extern int bsdtohpuxerrnomap[];

static	int exec_hpux_prep_nmagic(struct proc *, struct exec_package *);
static	int exec_hpux_prep_zmagic(struct proc *, struct exec_package *);
static	int exec_hpux_prep_omagic(struct proc *, struct exec_package *);

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
	sigcode,
	esigcode,
};

int
exec_hpux_makecmds(p, epp)
	struct proc *p;
	struct exec_package *epp;
{
	struct hpux_exec *hpux_ep = epp->ep_hdr;
	short sysid, magic;
	int error = ENOEXEC;

	if (epp->ep_hdrvalid < sizeof(struct hpux_exec))
		return (ENOEXEC);

	magic = HPUX_MAGIC(hpux_ep);
	sysid = HPUX_SYSID(hpux_ep);

	if (sysid != MID_HPUX)
		return (ENOEXEC);

	/*
	 * HP-UX is a 4k page size system, and executables assume
	 * this.
	 */
	if (PAGE_SIZE != HPUX_LDPGSZ)
		return (ENOEXEC);

	switch (magic) {
	case OMAGIC:
		error = exec_hpux_prep_omagic(p, epp);
		break;

	case NMAGIC:
		error = exec_hpux_prep_nmagic(p, epp);
		break;

	case ZMAGIC:
		error = exec_hpux_prep_zmagic(p, epp);
		break;
	}

	if (error == 0) {
		/* set up our emulation information */
		epp->ep_emul = &emul_hpux;
	} else
		kill_vmcmds(&epp->ep_vmcmds);

	return (error);
}

static int
exec_hpux_prep_nmagic(p, epp)
	struct proc *p;
	struct exec_package *epp;
{
	struct hpux_exec *execp = epp->ep_hdr;
	long bsize, baddr;

	epp->ep_taddr = 0;
	epp->ep_tsize = execp->ha_text;
	epp->ep_daddr = epp->ep_taddr + roundup(execp->ha_text, HPUX_LDPGSZ);
	epp->ep_dsize = execp->ha_data + execp->ha_bss;
	epp->ep_entry = execp->ha_entry;

	/* set up command for text segment */
	NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_readvn, execp->ha_text,
	    epp->ep_taddr, epp->ep_vp, HPUX_TXTOFF(*execp, NMAGIC),
	    VM_PROT_READ|VM_PROT_EXECUTE);

	/* set up command for data segment */
	NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_readvn, execp->ha_data,
	    epp->ep_daddr, epp->ep_vp, HPUX_DATAOFF(*execp, NMAGIC),
	    VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE);

	/* set up command for bss segment */
	baddr = round_page(epp->ep_daddr + execp->ha_data);
	bsize = epp->ep_daddr + epp->ep_dsize - baddr;
	if (bsize > 0)
		NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_zero, bsize, baddr,
		    NULLVP, 0, VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE);

	return (exec_setup_stack(p, epp));
}

static int
exec_hpux_prep_zmagic(p, epp)
	struct proc *p;
	struct exec_package *epp;
{
	struct hpux_exec *execp = epp->ep_hdr;
	long bsize, baddr;
	long nontext;

	/*
	 * Check if vnode is in open for writing, because we want to
	 * demand-page out of it.  If it is, don't do it, for various
	 * reasons.
	 */
	if ((execp->ha_text != 0 || execp->ha_data != 0) &&
	    epp->ep_vp->v_writecount != 0)
		return (ETXTBSY);
	vn_marktext(epp->ep_vp);

	/*
	 * HP-UX ZMAGIC executables need to have their segment
	 * sizes frobbed.
	 */
	nontext = execp->ha_data + execp->ha_bss;
	execp->ha_text = ctob(btoc(execp->ha_text));
	execp->ha_data = ctob(btoc(execp->ha_data));
	execp->ha_bss = nontext - execp->ha_data;
	if (execp->ha_bss < 0)
		execp->ha_bss = 0;

	epp->ep_taddr = 0;
	epp->ep_tsize = execp->ha_text;
	epp->ep_daddr = epp->ep_taddr + roundup(execp->ha_text, HPUX_LDPGSZ);
	epp->ep_dsize = execp->ha_data + execp->ha_bss;
	epp->ep_entry = execp->ha_entry;

	/* set up command for text segment */
	NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_pagedvn, execp->ha_text,
	    epp->ep_taddr, epp->ep_vp, HPUX_TXTOFF(*execp, ZMAGIC),
	    VM_PROT_READ|VM_PROT_EXECUTE);

	/* set up command for data segment */
	NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_pagedvn, execp->ha_data,
	    epp->ep_daddr, epp->ep_vp, HPUX_DATAOFF(*execp, ZMAGIC),
	    VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE);

	/* set up command for bss segment */
	baddr = round_page(epp->ep_daddr + execp->ha_data);
	bsize = epp->ep_daddr + epp->ep_dsize - baddr;
	if (bsize > 0)
		NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_zero, bsize, baddr,
		    NULLVP, 0, VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE);

	return (exec_setup_stack(p, epp));
}

/*
 * HP-UX's version of OMAGIC.
 */
static int
exec_hpux_prep_omagic(p, epp)
	struct proc *p;
	struct exec_package *epp;
{
	struct hpux_exec *execp = epp->ep_hdr;
	long dsize, bsize, baddr;

	epp->ep_taddr = 0;
	epp->ep_tsize = execp->ha_text;
	epp->ep_daddr = epp->ep_taddr + roundup(execp->ha_text, HPUX_LDPGSZ);
	epp->ep_dsize = execp->ha_data + execp->ha_bss;
	epp->ep_entry = execp->ha_entry;

	/* set up command for text and data segments */
	NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_readvn,
	    execp->ha_text + execp->ha_data, epp->ep_taddr, epp->ep_vp,
	    HPUX_TXTOFF(*execp, OMAGIC),
	    VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE);

	/* set up command for bss segment */
	baddr = round_page(epp->ep_daddr + execp->ha_data);
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
	dsize = epp->ep_dsize + execp->ha_text - round_page(execp->ha_text);
	epp->ep_dsize = (dsize > 0) ? dsize : 0;
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
