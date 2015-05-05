/*	$OpenBSD: linux_exec.c,v 1.43 2015/05/05 02:13:47 guenther Exp $	*/
/*	$NetBSD: linux_exec.c,v 1.13 1996/04/05 00:01:10 christos Exp $	*/

/*-
 * Copyright (c) 1994, 1995, 1998, 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas, Frank van der Linden, Eric Haszlakiewicz and
 * Thor Lancelot Simon.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/exec.h>
#include <sys/exec_elf.h>

#include <sys/mman.h>
#include <sys/syscallargs.h>
#include <sys/signalvar.h>

#include <uvm/uvm_extern.h>

#include <machine/cpu.h>
#include <machine/reg.h>
#include <machine/linux_machdep.h>

#include <compat/linux/linux_types.h>
#include <compat/linux/linux_syscall.h>
#include <compat/linux/linux_signal.h>
#include <compat/linux/linux_syscallargs.h>
#include <compat/linux/linux_util.h>
#include <compat/linux/linux_exec.h>
#include <compat/linux/linux_emuldata.h>

#define LINUX_ELF_AUX_ARGSIZ (sizeof(AuxInfo) * 8 / sizeof(char *))


const char linux_emul_path[] = "/emul/linux";
extern int linux_error[];
extern char linux_sigcode[], linux_esigcode[];
extern struct sysent linux_sysent[];
#ifdef SYSCALL_DEBUG
extern char *linux_syscallnames[];
#endif

extern struct mutex futex_lock;
extern void futex_pool_init(void);

void linux_e_proc_exec(struct proc *, struct exec_package *);
void linux_e_proc_fork(struct proc *, struct proc *);
void linux_e_proc_exit(struct proc *);
void linux_e_proc_init(struct proc *, struct vmspace *);

struct emul emul_linux_elf = {
	"linux",
	linux_error,
	linux_sendsig,
	LINUX_SYS_syscall,
	LINUX_SYS_MAXSYSCALL,
	linux_sysent,
#ifdef SYSCALL_DEBUG
	linux_syscallnames,
#else
	NULL,
#endif
	LINUX_ELF_AUX_ARGSIZ,
	elf32_copyargs,
	setregs,
	exec_elf32_fixup,
	NULL,			/* coredump */
	linux_sigcode,
	linux_esigcode,
	0,
	NULL,
	linux_e_proc_exec,
	linux_e_proc_fork,
	linux_e_proc_exit,
};

/*
 * Allocate per-process structures. Called when executing Linux
 * process. We can reuse the old emuldata - if it's not null,
 * the executed process is of same emulation as original forked one.
 */
void
linux_e_proc_init(struct proc *p, struct vmspace *vmspace)
{
	if (!p->p_emuldata) {
		/* allocate new Linux emuldata */
		p->p_emuldata = malloc(sizeof(struct linux_emuldata),
		    M_EMULDATA, M_WAITOK|M_ZERO);
	}
	else {
		memset(p->p_emuldata, '\0', sizeof(struct linux_emuldata));
	}

	/* Set the process idea of the break to the real value */
	((struct linux_emuldata *)(p->p_emuldata))->p_break = 
	    vmspace->vm_daddr + ptoa(vmspace->vm_dsize);
}

void
linux_e_proc_exec(struct proc *p, struct exec_package *epp)
{
	/* exec, use our vmspace */
	linux_e_proc_init(p, p->p_vmspace);
}

/*
 * Emulation per-process exit hook.
 */
void
linux_e_proc_exit(struct proc *p)
{
	struct linux_emuldata *emul = p->p_emuldata;

	if (emul->my_clear_tid) {
		pid_t zero = 0;

		if (copyout(&zero, emul->my_clear_tid, sizeof(zero)))
			psignal(p, SIGSEGV);
		/* 
		 * not yet: futex(my_clear_tid, FUTEX_WAKE, 1, NULL, NULL, 0)
		 */
	}

	/* free Linux emuldata and set the pointer to null */
	free(p->p_emuldata, M_EMULDATA, 0);
	p->p_emuldata = NULL;
}

/*
 * Emulation fork hook.
 */
void
linux_e_proc_fork(struct proc *p, struct proc *parent)
{
	struct linux_emuldata *emul;
	struct linux_emuldata *parent_emul;

	/* Allocate new emuldata for the new process. */
	p->p_emuldata = NULL;

	/* fork, use parent's vmspace (our vmspace may not be setup yet) */
	linux_e_proc_init(p, parent->p_vmspace);

	emul = p->p_emuldata;
	parent_emul = parent->p_emuldata;

	emul->my_set_tid = parent_emul->child_set_tid;
	emul->my_clear_tid = parent_emul->child_clear_tid;
	emul->my_tls_base = parent_emul->child_tls_base;
	emul->set_tls_base = parent_emul->set_tls_base;
}

int
exec_linux_elf32_makecmds(struct proc *p, struct exec_package *epp)
{
	if (!(emul_linux_elf.e_flags & EMUL_ENABLED))
		return (ENOEXEC);

	return exec_elf32_makecmds(p, epp);
}

int
linux_elf_probe(struct proc *p, struct exec_package *epp, char *itp,
    u_long *pos)
{
	Elf32_Ehdr *eh = epp->ep_hdr;
	char *bp, *brand;
	int error;
	size_t len;

	if (!(emul_linux_elf.e_flags & EMUL_ENABLED))
		return (ENOEXEC);

	/*
	 * Modern Linux binaries carry an identification note.
	 */
	if (ELFNAME(os_pt_note)(p, epp, epp->ep_hdr, "GNU", 4, 0x10) == 0) {
		goto recognized;
	}

	brand = elf32_check_brand(eh);
	if (brand != NULL && strcmp(brand, "Linux") != 0)
		return (EINVAL);

	/*
	 * If this is a static binary, do not allow it to run, as it
	 * has not been identified. We'll give non-static binaries a
	 * chance to run, as the Linux ld.so name is usually unique
	 * enough to clear any ambiguity.
	 */
	if (itp == NULL)
		return (EINVAL);

recognized:
	if (itp) {
		if ((error = emul_find(p, NULL, linux_emul_path, itp, &bp, 0)))
			return (error);
		error = copystr(bp, itp, MAXPATHLEN, &len);
		free(bp, M_TEMP, 0);
		if (error)
			return (error);
	}
	epp->ep_emul = &emul_linux_elf;
	*pos = ELF32_NO_ADDR;

	mtx_init(&futex_lock, IPL_NONE);
	futex_pool_init();

	return (0);
}

/*
 * Execve(2). Just check the alternate emulation path, and pass it on
 * to the regular execve().
 */
int
linux_sys_execve(struct proc *p, void *v, register_t *retval)
{
	struct linux_sys_execve_args /* {
		syscallarg(char *) path;
		syscallarg(char **) argv;
		syscallarg(char **) envp;
        } */ *uap = v;
	struct sys_execve_args ap;
	caddr_t sg;

	sg = stackgap_init(p);
	LINUX_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));

	SCARG(&ap, path) = SCARG(uap, path);
	SCARG(&ap, argp) = SCARG(uap, argp);
	SCARG(&ap, envp) = SCARG(uap, envp);

	return (sys_execve(p, &ap, retval));
}
