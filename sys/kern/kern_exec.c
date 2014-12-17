/*	$OpenBSD: kern_exec.c,v 1.153 2014/12/17 06:58:11 guenther Exp $	*/
/*	$NetBSD: kern_exec.c,v 1.75 1996/02/09 18:59:28 christos Exp $	*/

/*-
 * Copyright (C) 1993, 1994 Christopher G. Demetriou
 * Copyright (C) 1992 Wolfgang Solfrank.
 * Copyright (C) 1992 TooLs GmbH.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/filedesc.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/malloc.h>
#include <sys/pool.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/acct.h>
#include <sys/exec.h>
#include <sys/ktrace.h>
#include <sys/resourcevar.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/signalvar.h>
#include <sys/stat.h>
#include <sys/conf.h>
#ifdef SYSVSHM
#include <sys/shm.h>
#endif

#include <sys/syscallargs.h>

#include <uvm/uvm_extern.h>

#include <machine/reg.h>

#ifdef __HAVE_MD_TCB
# include <machine/tcb.h>
#endif

#include "systrace.h"

#if NSYSTRACE > 0
#include <dev/systrace.h>
#endif

const struct kmem_va_mode kv_exec = {
	.kv_wait = 1,
	.kv_map = &exec_map
};

/*
 * Map the shared signal code.
 */
int exec_sigcode_map(struct process *, struct emul *);

/*
 * If non-zero, stackgap_random specifies the upper limit of the random gap size
 * added to the fixed stack gap. Must be n^2.
 */
int stackgap_random = STACKGAP_RANDOM;

/*
 * check exec:
 * given an "executable" described in the exec package's namei info,
 * see what we can do with it.
 *
 * ON ENTRY:
 *	exec package with appropriate namei info
 *	proc pointer of exec'ing proc
 *	NO SELF-LOCKED VNODES
 *
 * ON EXIT:
 *	error:	nothing held, etc.  exec header still allocated.
 *	ok:	filled exec package, one locked vnode.
 *
 * EXEC SWITCH ENTRY:
 * 	Locked vnode to check, exec package, proc.
 *
 * EXEC SWITCH EXIT:
 *	ok:	return 0, filled exec package, one locked vnode.
 *	error:	destructive:
 *			everything deallocated except exec header.
 *		non-destructive:
 *			error code, locked vnode, exec header unmodified
 */
int
check_exec(struct proc *p, struct exec_package *epp)
{
	int error, i;
	struct vnode *vp;
	struct nameidata *ndp;
	size_t resid;

	ndp = epp->ep_ndp;
	ndp->ni_cnd.cn_nameiop = LOOKUP;
	ndp->ni_cnd.cn_flags = FOLLOW | LOCKLEAF | SAVENAME;
	/* first get the vnode */
	if ((error = namei(ndp)) != 0)
		return (error);
	epp->ep_vp = vp = ndp->ni_vp;

	/* check for regular file */
	if (vp->v_type == VDIR) {
		error = EISDIR;
		goto bad1;
	}
	if (vp->v_type != VREG) {
		error = EACCES;
		goto bad1;
	}

	/* get attributes */
	if ((error = VOP_GETATTR(vp, epp->ep_vap, p->p_ucred, p)) != 0)
		goto bad1;

	/* Check mount point */
	if (vp->v_mount->mnt_flag & MNT_NOEXEC) {
		error = EACCES;
		goto bad1;
	}

	if ((vp->v_mount->mnt_flag & MNT_NOSUID))
		epp->ep_vap->va_mode &= ~(VSUID | VSGID);

	/* check access.  for root we have to see if any exec bit on */
	if ((error = VOP_ACCESS(vp, VEXEC, p->p_ucred, p)) != 0)
		goto bad1;
	if ((epp->ep_vap->va_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) == 0) {
		error = EACCES;
		goto bad1;
	}

	/* try to open it */
	if ((error = VOP_OPEN(vp, FREAD, p->p_ucred, p)) != 0)
		goto bad1;

	/* unlock vp, we need it unlocked from here */
	VOP_UNLOCK(vp, 0, p);

	/* now we have the file, get the exec header */
	error = vn_rdwr(UIO_READ, vp, epp->ep_hdr, epp->ep_hdrlen, 0,
	    UIO_SYSSPACE, 0, p->p_ucred, &resid, p);
	if (error)
		goto bad2;
	epp->ep_hdrvalid = epp->ep_hdrlen - resid;

	/*
	 * set up the vmcmds for creation of the process
	 * address space
	 */
	error = ENOEXEC;
	for (i = 0; i < nexecs && error != 0; i++) {
		int newerror;

		if (execsw[i].es_check == NULL)
			continue;
		newerror = (*execsw[i].es_check)(p, epp);
		if (!newerror && !(epp->ep_emul->e_flags & EMUL_ENABLED))
			newerror = EPERM;
		/* make sure the first "interesting" error code is saved. */
		if (!newerror || error == ENOEXEC)
			error = newerror;
		if (epp->ep_flags & EXEC_DESTR && error != 0)
			return (error);
	}
	if (!error) {
		/* check that entry point is sane */
		if (epp->ep_entry > VM_MAXUSER_ADDRESS) {
			error = ENOEXEC;
		}

		/* check limits */
		if ((epp->ep_tsize > MAXTSIZ) ||
		    (epp->ep_dsize > p->p_rlimit[RLIMIT_DATA].rlim_cur))
			error = ENOMEM;

		if (!error)
			return (0);
	}

	/*
	 * free any vmspace-creation commands,
	 * and release their references
	 */
	kill_vmcmds(&epp->ep_vmcmds);

bad2:
	/*
	 * close the vnode, free the pathname buf, and punt.
	 */
	vn_close(vp, FREAD, p->p_ucred, p);
	pool_put(&namei_pool, ndp->ni_cnd.cn_pnbuf);
	return (error);

bad1:
	/*
	 * free the namei pathname buffer, and put the vnode
	 * (which we don't yet have open).
	 */
	pool_put(&namei_pool, ndp->ni_cnd.cn_pnbuf);
	vput(vp);
	return (error);
}

/*
 * exec system call
 */
/* ARGSUSED */
int
sys_execve(struct proc *p, void *v, register_t *retval)
{
	struct sys_execve_args /* {
		syscallarg(const char *) path;
		syscallarg(char *const *) argp;
		syscallarg(char *const *) envp;
	} */ *uap = v;
	int error;
	struct exec_package pack;
	struct nameidata nid;
	struct vattr attr;
	struct ucred *cred = p->p_ucred;
	char *argp;
	char * const *cpp, *dp, *sp;
	struct process *pr = p->p_p;
	long argc, envc;
	size_t len, sgap;
#ifdef MACHINE_STACK_GROWS_UP
	size_t slen;
#endif
	char *stack;
	struct ps_strings arginfo;
	struct vmspace *vm = pr->ps_vmspace;
	char **tmpfap;
	extern struct emul emul_native;
#if NSYSTRACE > 0
	int wassugid = ISSET(pr->ps_flags, PS_SUGID | PS_SUGIDEXEC);
	size_t pathbuflen;
#endif
	char *pathbuf = NULL;
	struct vnode *otvp;

	/* get other threads to stop */
	if ((error = single_thread_set(p, SINGLE_UNWIND, 1)))
		return (error);

	/*
	 * Cheap solution to complicated problems.
	 * Mark this process as "leave me alone, I'm execing".
	 */
	atomic_setbits_int(&pr->ps_flags, PS_INEXEC);

#if NSYSTRACE > 0
	if (ISSET(p->p_flag, P_SYSTRACE)) {
		systrace_execve0(p);
		pathbuf = pool_get(&namei_pool, PR_WAITOK);
		error = copyinstr(SCARG(uap, path), pathbuf, MAXPATHLEN,
		    &pathbuflen);
		if (error != 0)
			goto clrflag;
	}
#endif
	if (pathbuf != NULL) {
		NDINIT(&nid, LOOKUP, NOFOLLOW, UIO_SYSSPACE, pathbuf, p);
	} else {
		NDINIT(&nid, LOOKUP, NOFOLLOW, UIO_USERSPACE,
		    SCARG(uap, path), p);
	}

	/*
	 * initialize the fields of the exec package.
	 */
	if (pathbuf != NULL)
		pack.ep_name = pathbuf;
	else
		pack.ep_name = (char *)SCARG(uap, path);
	pack.ep_hdr = malloc(exec_maxhdrsz, M_EXEC, M_WAITOK);
	pack.ep_hdrlen = exec_maxhdrsz;
	pack.ep_hdrvalid = 0;
	pack.ep_ndp = &nid;
	pack.ep_interp = NULL;
	pack.ep_emul_arg = NULL;
	VMCMDSET_INIT(&pack.ep_vmcmds);
	pack.ep_vap = &attr;
	pack.ep_emul = &emul_native;
	pack.ep_flags = 0;

	/* see if we can run it. */
	if ((error = check_exec(p, &pack)) != 0) {
		goto freehdr;
	}

	/* XXX -- THE FOLLOWING SECTION NEEDS MAJOR CLEANUP */

	/* allocate an argument buffer */
	argp = km_alloc(NCARGS, &kv_exec, &kp_pageable, &kd_waitok);
#ifdef DIAGNOSTIC
	if (argp == NULL)
		panic("execve: argp == NULL");
#endif
	dp = argp;
	argc = 0;

	/* copy the fake args list, if there's one, freeing it as we go */
	if (pack.ep_flags & EXEC_HASARGL) {
		tmpfap = pack.ep_fa;
		while (*tmpfap != NULL) {
			char *cp;

			cp = *tmpfap;
			while (*cp)
				*dp++ = *cp++;
			*dp++ = '\0';

			free(*tmpfap, M_EXEC, 0);
			tmpfap++; argc++;
		}
		free(pack.ep_fa, M_EXEC, 0);
		pack.ep_flags &= ~EXEC_HASARGL;
	}

	/* Now get argv & environment */
	if (!(cpp = SCARG(uap, argp))) {
		error = EFAULT;
		goto bad;
	}

	if (pack.ep_flags & EXEC_SKIPARG)
		cpp++;

	while (1) {
		len = argp + ARG_MAX - dp;
		if ((error = copyin(cpp, &sp, sizeof(sp))) != 0)
			goto bad;
		if (!sp)
			break;
		if ((error = copyinstr(sp, dp, len, &len)) != 0) {
			if (error == ENAMETOOLONG)
				error = E2BIG;
			goto bad;
		}
		dp += len;
		cpp++;
		argc++;
	}

	envc = 0;
	/* environment does not need to be there */
	if ((cpp = SCARG(uap, envp)) != NULL ) {
		while (1) {
			len = argp + ARG_MAX - dp;
			if ((error = copyin(cpp, &sp, sizeof(sp))) != 0)
				goto bad;
			if (!sp)
				break;
			if ((error = copyinstr(sp, dp, len, &len)) != 0) {
				if (error == ENAMETOOLONG)
					error = E2BIG;
				goto bad;
			}
			dp += len;
			cpp++;
			envc++;
		}
	}

	dp = (char *)(((long)dp + _STACKALIGNBYTES) & ~_STACKALIGNBYTES);

	sgap = STACKGAPLEN;
	if (stackgap_random != 0) {
		sgap += arc4random() & (stackgap_random - 1);
		sgap = (sgap + _STACKALIGNBYTES) & ~_STACKALIGNBYTES;
	}

	/* Now check if args & environ fit into new stack */
	len = ((argc + envc + 2 + pack.ep_emul->e_arglen) * sizeof(char *) +
	    sizeof(long) + dp + sgap + sizeof(struct ps_strings)) - argp;

	len = (len + _STACKALIGNBYTES) &~ _STACKALIGNBYTES;

	if (len > pack.ep_ssize) { /* in effect, compare to initial limit */
		error = ENOMEM;
		goto bad;
	}

	/* adjust "active stack depth" for process VSZ */
	pack.ep_ssize = len;	/* maybe should go elsewhere, but... */

	/*
	 * we're committed: any further errors will kill the process, so
	 * kill the other threads now.
	 */
	single_thread_set(p, SINGLE_EXIT, 0);

	/*
	 * Prepare vmspace for remapping. Note that uvmspace_exec can replace
	 * pr_vmspace!
	 */
	uvmspace_exec(p, VM_MIN_ADDRESS, VM_MAXUSER_ADDRESS);

	vm = pr->ps_vmspace;
	/* Now map address space */
	vm->vm_taddr = (char *)trunc_page(pack.ep_taddr);
	vm->vm_tsize = atop(round_page(pack.ep_taddr + pack.ep_tsize) -
	    trunc_page(pack.ep_taddr));
	vm->vm_daddr = (char *)trunc_page(pack.ep_daddr);
	vm->vm_dsize = atop(round_page(pack.ep_daddr + pack.ep_dsize) -
	    trunc_page(pack.ep_daddr));
	vm->vm_dused = 0;
	vm->vm_ssize = atop(round_page(pack.ep_ssize));
	vm->vm_maxsaddr = (char *)pack.ep_maxsaddr;
	vm->vm_minsaddr = (char *)pack.ep_minsaddr;

	/* create the new process's VM space by running the vmcmds */
#ifdef DIAGNOSTIC
	if (pack.ep_vmcmds.evs_used == 0)
		panic("execve: no vmcmds");
#endif
	error = exec_process_vmcmds(p, &pack);

	/* if an error happened, deallocate and punt */
	if (error)
		goto exec_abort;

	/* remember information about the process */
	arginfo.ps_nargvstr = argc;
	arginfo.ps_nenvstr = envc;

#ifdef MACHINE_STACK_GROWS_UP
	stack = (char *)USRSTACK + sizeof(arginfo) + sgap;
	slen = len - sizeof(arginfo) - sgap;
#else
	stack = (char *)(USRSTACK - len);
#endif
	/* Now copy argc, args & environ to new stack */
	if (!(*pack.ep_emul->e_copyargs)(&pack, &arginfo, stack, argp))
		goto exec_abort;

	/* copy out the process's ps_strings structure */
	if (copyout(&arginfo, (char *)PS_STRINGS, sizeof(arginfo)))
		goto exec_abort;

	stopprofclock(pr);	/* stop profiling */
	fdcloseexec(p);		/* handle close on exec */
	execsigs(p);		/* reset caught signals */
	TCB_SET(p, NULL);	/* reset the TCB address */

	/* set command name & other accounting info */
	memset(p->p_comm, 0, sizeof(p->p_comm));
	len = min(nid.ni_cnd.cn_namelen, MAXCOMLEN);
	memcpy(p->p_comm, nid.ni_cnd.cn_nameptr, len);
	pr->ps_acflag &= ~AFORK;

	/* record proc's vnode, for use by sysctl */
	otvp = pr->ps_textvp;
	vref(pack.ep_vp);
	pr->ps_textvp = pack.ep_vp;
	if (otvp)
		vrele(otvp);

	atomic_setbits_int(&pr->ps_flags, PS_EXEC);
	if (pr->ps_flags & PS_PPWAIT) {
		atomic_clearbits_int(&pr->ps_flags, PS_PPWAIT);
		atomic_clearbits_int(&pr->ps_pptr->ps_flags, PS_ISPWAIT);
		wakeup(pr->ps_pptr);
	}

	/*
	 * If process does execve() while it has a mismatched real,
	 * effective, or saved uid/gid, we set PS_SUGIDEXEC.
	 */
	if (cred->cr_uid != cred->cr_ruid ||
	    cred->cr_uid != cred->cr_svuid ||
	    cred->cr_gid != cred->cr_rgid ||
	    cred->cr_gid != cred->cr_svgid)
		atomic_setbits_int(&pr->ps_flags, PS_SUGIDEXEC);
	else
		atomic_clearbits_int(&pr->ps_flags, PS_SUGIDEXEC);

	/*
	 * deal with set[ug]id.
	 * MNT_NOEXEC has already been used to disable s[ug]id.
	 */
	if ((attr.va_mode & (VSUID | VSGID)) && proc_cansugid(p)) {
		int i;

		atomic_setbits_int(&pr->ps_flags, PS_SUGID|PS_SUGIDEXEC);

#ifdef KTRACE
		/*
		 * If process is being ktraced, turn off - unless
		 * root set it.
		 */
		if (pr->ps_tracevp && !(pr->ps_traceflag & KTRFAC_ROOT))
			ktrcleartrace(pr);
#endif
		p->p_ucred = cred = crcopy(cred);
		if (attr.va_mode & VSUID)
			cred->cr_uid = attr.va_uid;
		if (attr.va_mode & VSGID)
			cred->cr_gid = attr.va_gid;

		/*
		 * For set[ug]id processes, a few caveats apply to
		 * stdin, stdout, and stderr.
		 */
		error = 0;
		fdplock(p->p_fd);
		for (i = 0; i < 3; i++) {
			struct file *fp = NULL;

			/*
			 * NOTE - This will never return NULL because of
			 * immature fds. The file descriptor table is not
			 * shared because we're suid.
			 */
			fp = fd_getfile(p->p_fd, i);

			/*
			 * Ensure that stdin, stdout, and stderr are already
			 * allocated.  We do not want userland to accidentally
			 * allocate descriptors in this range which has implied
			 * meaning to libc.
			 */
			if (fp == NULL) {
				short flags = FREAD | (i == 0 ? 0 : FWRITE);
				struct vnode *vp;
				int indx;

				if ((error = falloc(p, &fp, &indx)) != 0)
					break;
#ifdef DIAGNOSTIC
				if (indx != i)
					panic("sys_execve: falloc indx != i");
#endif
				if ((error = cdevvp(getnulldev(), &vp)) != 0) {
					fdremove(p->p_fd, indx);
					closef(fp, p);
					break;
				}
				if ((error = VOP_OPEN(vp, flags, cred, p)) != 0) {
					fdremove(p->p_fd, indx);
					closef(fp, p);
					vrele(vp);
					break;
				}
				if (flags & FWRITE)
					vp->v_writecount++;
				fp->f_flag = flags;
				fp->f_type = DTYPE_VNODE;
				fp->f_ops = &vnops;
				fp->f_data = (caddr_t)vp;
				FILE_SET_MATURE(fp, p);
			}
		}
		fdpunlock(p->p_fd);
		if (error)
			goto exec_abort;
	} else
		atomic_clearbits_int(&pr->ps_flags, PS_SUGID);

	/*
	 * Reset the saved ugids and update the process's copy of the
	 * creds if the creds have been changed
	 */
	if (cred->cr_uid != cred->cr_svuid ||
	    cred->cr_gid != cred->cr_svgid) {
		/* make sure we have unshared ucreds */
		p->p_ucred = cred = crcopy(cred);
		cred->cr_svuid = cred->cr_uid;
		cred->cr_svgid = cred->cr_gid;
	}

	if (pr->ps_ucred != cred) {
		struct ucred *ocred;

		ocred = pr->ps_ucred;
		crhold(cred);
		pr->ps_ucred = cred;
		crfree(ocred);
	}

	if (pr->ps_flags & PS_SUGIDEXEC) {
		int i, s = splclock();

		timeout_del(&pr->ps_realit_to);
		for (i = 0; i < nitems(pr->ps_timer); i++) {
			timerclear(&pr->ps_timer[i].it_interval);
			timerclear(&pr->ps_timer[i].it_value);
		}
		splx(s);
	}

	/* reset CPU time usage for the thread, but not the process */
	timespecclear(&p->p_tu.tu_runtime);
	p->p_tu.tu_uticks = p->p_tu.tu_sticks = p->p_tu.tu_iticks = 0;

	km_free(argp, NCARGS, &kv_exec, &kp_pageable);

	pool_put(&namei_pool, nid.ni_cnd.cn_pnbuf);
	vn_close(pack.ep_vp, FREAD, cred, p);

	/*
	 * notify others that we exec'd
	 */
	KNOTE(&pr->ps_klist, NOTE_EXEC);

	/* setup new registers and do misc. setup. */
	if (pack.ep_emul->e_fixup != NULL) {
		if ((*pack.ep_emul->e_fixup)(p, &pack) != 0)
			goto free_pack_abort;
	}
#ifdef MACHINE_STACK_GROWS_UP
	(*pack.ep_emul->e_setregs)(p, &pack, (u_long)stack + slen, retval);
#else
	(*pack.ep_emul->e_setregs)(p, &pack, (u_long)stack, retval);
#endif

	/* map the process's signal trampoline code */
	if (exec_sigcode_map(pr, pack.ep_emul))
		goto free_pack_abort;

#ifdef __HAVE_EXEC_MD_MAP
	/* perform md specific mappings that process might need */
	if (exec_md_map(p, &pack))
		goto free_pack_abort;
#endif

	if (pr->ps_flags & PS_TRACED)
		psignal(p, SIGTRAP);

	free(pack.ep_hdr, M_EXEC, 0);

	/*
	 * Call emulation specific exec hook. This can setup per-process
	 * p->p_emuldata or do any other per-process stuff an emulation needs.
	 *
	 * If we are executing process of different emulation than the
	 * original forked process, call e_proc_exit() of the old emulation
	 * first, then e_proc_exec() of new emulation. If the emulation is
	 * same, the exec hook code should deallocate any old emulation
	 * resources held previously by this process.
	 */
	if (pr->ps_emul && pr->ps_emul->e_proc_exit &&
	    pr->ps_emul != pack.ep_emul)
		(*pr->ps_emul->e_proc_exit)(p);

	p->p_descfd = 255;
	if ((pack.ep_flags & EXEC_HASFD) && pack.ep_fd < 255)
		p->p_descfd = pack.ep_fd;

	/*
	 * Call exec hook. Emulation code may NOT store reference to anything
	 * from &pack.
	 */
	if (pack.ep_emul->e_proc_exec)
		(*pack.ep_emul->e_proc_exec)(p, &pack);

	/* update ps_emul, the old value is no longer needed */
	pr->ps_emul = pack.ep_emul;

#ifdef KTRACE
	if (KTRPOINT(p, KTR_EMUL))
		ktremul(p);
#endif

	atomic_clearbits_int(&pr->ps_flags, PS_INEXEC);
	single_thread_clear(p, P_SUSPSIG);

#if NSYSTRACE > 0
	if (ISSET(p->p_flag, P_SYSTRACE) &&
	    wassugid && !ISSET(pr->ps_flags, PS_SUGID | PS_SUGIDEXEC))
		systrace_execve1(pathbuf, p);
#endif

	if (pathbuf != NULL)
		pool_put(&namei_pool, pathbuf);

	return (0);

bad:
	/* free the vmspace-creation commands, and release their references */
	kill_vmcmds(&pack.ep_vmcmds);
	/* kill any opened file descriptor, if necessary */
	if (pack.ep_flags & EXEC_HASFD) {
		pack.ep_flags &= ~EXEC_HASFD;
		fdplock(p->p_fd);
		(void) fdrelease(p, pack.ep_fd);
		fdpunlock(p->p_fd);
	}
	if (pack.ep_interp != NULL)
		pool_put(&namei_pool, pack.ep_interp);
	if (pack.ep_emul_arg != NULL)
		free(pack.ep_emul_arg, M_TEMP, 0);
	/* close and put the exec'd file */
	vn_close(pack.ep_vp, FREAD, cred, p);
	pool_put(&namei_pool, nid.ni_cnd.cn_pnbuf);
	km_free(argp, NCARGS, &kv_exec, &kp_pageable);

 freehdr:
	free(pack.ep_hdr, M_EXEC, 0);
#if NSYSTRACE > 0
 clrflag:
#endif
	atomic_clearbits_int(&pr->ps_flags, PS_INEXEC);
	single_thread_clear(p, P_SUSPSIG);

	if (pathbuf != NULL)
		pool_put(&namei_pool, pathbuf);

	return (error);

exec_abort:
	/*
	 * the old process doesn't exist anymore.  exit gracefully.
	 * get rid of the (new) address space we have created, if any, get rid
	 * of our namei data and vnode, and exit noting failure
	 */
	uvm_deallocate(&vm->vm_map, VM_MIN_ADDRESS,
		VM_MAXUSER_ADDRESS - VM_MIN_ADDRESS);
	if (pack.ep_interp != NULL)
		pool_put(&namei_pool, pack.ep_interp);
	if (pack.ep_emul_arg != NULL)
		free(pack.ep_emul_arg, M_TEMP, 0);
	pool_put(&namei_pool, nid.ni_cnd.cn_pnbuf);
	vn_close(pack.ep_vp, FREAD, cred, p);
	km_free(argp, NCARGS, &kv_exec, &kp_pageable);

free_pack_abort:
	free(pack.ep_hdr, M_EXEC, 0);
	exit1(p, W_EXITCODE(0, SIGABRT), EXIT_NORMAL);

	/* NOTREACHED */
	atomic_clearbits_int(&pr->ps_flags, PS_INEXEC);
	if (pathbuf != NULL)
		pool_put(&namei_pool, pathbuf);

	return (0);
}


void *
copyargs(struct exec_package *pack, struct ps_strings *arginfo, void *stack,
    void *argp)
{
	char **cpp = stack;
	char *dp, *sp;
	size_t len;
	void *nullp = NULL;
	long argc = arginfo->ps_nargvstr;
	int envc = arginfo->ps_nenvstr;

	if (copyout(&argc, cpp++, sizeof(argc)))
		return (NULL);

	dp = (char *) (cpp + argc + envc + 2 + pack->ep_emul->e_arglen);
	sp = argp;

	/* XXX don't copy them out, remap them! */
	arginfo->ps_argvstr = cpp; /* remember location of argv for later */

	for (; --argc >= 0; sp += len, dp += len)
		if (copyout(&dp, cpp++, sizeof(dp)) ||
		    copyoutstr(sp, dp, ARG_MAX, &len))
			return (NULL);

	if (copyout(&nullp, cpp++, sizeof(nullp)))
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
exec_sigcode_map(struct process *pr, struct emul *e)
{
	vsize_t sz;

	sz = (vaddr_t)e->e_esigcode - (vaddr_t)e->e_sigcode;

	/*
	 * If we don't have a sigobject for this emulation, create one.
	 *
	 * sigobject is an anonymous memory object (just like SYSV shared
	 * memory) that we keep a permanent reference to and that we map
	 * in all processes that need this sigcode. The creation is simple,
	 * we create an object, add a permanent reference to it, map it in
	 * kernel space, copy out the sigcode to it and unmap it.
	 * Then we map it with PROT_READ|PROT_EXEC into the process just
	 * the way sys_mmap would map it.
	 */
	if (e->e_sigobject == NULL) {
		vaddr_t va;
		int r;

		e->e_sigobject = uao_create(sz, 0);
		uao_reference(e->e_sigobject);	/* permanent reference */

		if ((r = uvm_map(kernel_map, &va, round_page(sz), e->e_sigobject,
		    0, 0, UVM_MAPFLAG(PROT_READ | PROT_WRITE, PROT_READ | PROT_WRITE,
		    MAP_INHERIT_SHARE, MADV_RANDOM, 0)))) {
			uao_detach(e->e_sigobject);
			return (ENOMEM);
		}
		memcpy((void *)va, e->e_sigcode, sz);
		uvm_unmap(kernel_map, va, va + round_page(sz));
	}

	pr->ps_sigcode = 0; /* no hint */
	uao_reference(e->e_sigobject);
	if (uvm_map(&pr->ps_vmspace->vm_map, &pr->ps_sigcode, round_page(sz),
	    e->e_sigobject, 0, 0, UVM_MAPFLAG(PROT_READ | PROT_EXEC,
	    PROT_READ | PROT_EXEC, MAP_INHERIT_SHARE, MADV_RANDOM, 0))) {
		uao_detach(e->e_sigobject);
		return (ENOMEM);
	}

	return (0);
}
