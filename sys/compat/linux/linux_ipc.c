/*	$OpenBSD: linux_ipc.c,v 1.15 2011/10/27 07:56:28 robert Exp $	*/
/*	$NetBSD: linux_ipc.c,v 1.10 1996/04/05 00:01:44 christos Exp $	*/

/*
 * Copyright (c) 1995 Frank van der Linden
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
 *      This product includes software developed for the NetBSD Project
 *      by Frank van der Linden
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

#include <sys/types.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <sys/proc.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <sys/malloc.h>
#include <sys/mman.h>
#include <sys/systm.h>
#include <sys/stat.h>

#include <sys/mount.h>
#include <sys/syscallargs.h>

#include <compat/linux/linux_types.h>
#include <compat/linux/linux_signal.h>
#include <compat/linux/linux_syscallargs.h>
#include <compat/linux/linux_util.h>
#include <compat/linux/linux_ipc.h>
#include <compat/linux/linux_msg.h>
#include <compat/linux/linux_shm.h>
#include <compat/linux/linux_sem.h>
#include <compat/linux/linux_ipccall.h>

/*
 * Stuff to deal with the SysV ipc/shm/semaphore interface in Linux.
 * The main difference is, that Linux handles it all via one
 * system call, which has the usual maximum amount of 5 arguments.
 * This results in a kludge for calls that take 6 of them.
 *
 * The SYSVXXXX options have to be enabled to get the appropriate
 * functions to work.
 */

#ifdef SYSVSEM
int linux_semop(struct proc *, void *, register_t *);
int linux_semget(struct proc *, void *, register_t *);
int linux_semctl(struct proc *, void *, register_t *);
void bsd_to_linux_semid_ds(struct semid_ds *, struct linux_semid_ds *);
void linux_to_bsd_semid_ds(struct linux_semid_ds *, struct semid_ds *);
#endif

#ifdef SYSVMSG
int linux_msgsnd(struct proc *, void *, register_t *);
int linux_msgrcv(struct proc *, void *, register_t *);
int linux_msgget(struct proc *, void *, register_t *);
int linux_msgctl(struct proc *, void *, register_t *);
void linux_to_bsd_msqid_ds(struct linux_msqid_ds *, struct msqid_ds *);
void bsd_to_linux_msqid_ds(struct msqid_ds *, struct linux_msqid_ds *);
#endif

#ifdef SYSVSHM
int linux_shmat(struct proc *, void *, register_t *);
int linux_shmdt(struct proc *, void *, register_t *);
int linux_shmget(struct proc *, void *, register_t *);
int linux_shmctl(struct proc *, void *, register_t *);
void linux_to_bsd_shmid_ds(struct linux_shmid_ds *, struct shmid_ds *);
void bsd_to_linux_shmid_ds(struct shmid_ds *, struct linux_shmid_ds *);
#endif

#if defined(SYSVMSG) || defined(SYSVSEM) || defined(SYSVSHM)
void linux_to_bsd_ipc_perm(struct linux_ipc_perm *, struct ipc_perm *);
void bsd_to_linux_ipc_perm(struct ipc_perm *, struct linux_ipc_perm *);
#endif

int
linux_sys_ipc(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_ipc_args /* {
		syscallarg(int) what;
		syscallarg(int) a1;
		syscallarg(int) a2;
		syscallarg(int) a3;
		syscallarg(caddr_t) ptr;
	} */ *uap = v;

	switch (SCARG(uap, what)) {
#ifdef SYSVSEM
	case LINUX_SYS_semop:
		return linux_semop(p, uap, retval);
	case LINUX_SYS_semget:
		return linux_semget(p, uap, retval);
	case LINUX_SYS_semctl:
		return linux_semctl(p, uap, retval);
#endif
#ifdef SYSVMSG
	case LINUX_SYS_msgsnd:
		return linux_msgsnd(p, uap, retval);
	case LINUX_SYS_msgrcv:
		return linux_msgrcv(p, uap, retval);
	case LINUX_SYS_msgget:
		return linux_msgget(p, uap, retval);
	case LINUX_SYS_msgctl:
		return linux_msgctl(p, uap, retval);
#endif
#ifdef SYSVSHM
	case LINUX_SYS_shmat:
		return linux_shmat(p, uap, retval);
	case LINUX_SYS_shmdt:
		return linux_shmdt(p, uap, retval);
	case LINUX_SYS_shmget:
		return linux_shmget(p, uap, retval);
	case LINUX_SYS_shmctl:
		return linux_shmctl(p, uap, retval);
#endif
	default:
		return ENOSYS;
	}
}

#if defined(SYSVMSG) || defined(SYSVSEM) || defined(SYSVSHM)
/*
 * Convert between Linux and OpenBSD ipc_perm structures. Only the
 * order of the fields is different.
 */
void
linux_to_bsd_ipc_perm(lpp, bpp)
	struct linux_ipc_perm *lpp;
	struct ipc_perm *bpp;
{

	bpp->key = lpp->l_key;
	bpp->uid = lpp->l_uid;
	bpp->gid = lpp->l_gid;
	bpp->cuid = lpp->l_cuid;
	bpp->cgid = lpp->l_cgid;
	bpp->mode = lpp->l_mode;
	bpp->seq = lpp->l_seq;
}

void
bsd_to_linux_ipc_perm(bpp, lpp)
	struct ipc_perm *bpp;
	struct linux_ipc_perm *lpp;
{

	lpp->l_key = bpp->key;
	lpp->l_uid = bpp->uid;
	lpp->l_gid = bpp->gid;
	lpp->l_cuid = bpp->cuid;
	lpp->l_cgid = bpp->cgid;
	lpp->l_mode = bpp->mode;
	lpp->l_seq = bpp->seq;
}
#endif

#ifdef SYSVSEM
/*
 * Semaphore operations. Most constants and structures are the same on
 * both systems. Only semctl() needs some extra work.
 */

/*
 * Convert between Linux and OpenBSD semid_ds structures.
 */
void
bsd_to_linux_semid_ds(bs, ls)
	struct semid_ds *bs;
	struct linux_semid_ds *ls;
{

	bsd_to_linux_ipc_perm(&bs->sem_perm, &ls->l_sem_perm);
	ls->l_sem_otime = bs->sem_otime;
	ls->l_sem_ctime = bs->sem_ctime;
	ls->l_sem_nsems = bs->sem_nsems;
	ls->l_sem_base = bs->sem_base;
}

void
linux_to_bsd_semid_ds(ls, bs)
	struct linux_semid_ds *ls;
	struct semid_ds *bs;
{

	linux_to_bsd_ipc_perm(&ls->l_sem_perm, &bs->sem_perm);
	bs->sem_otime = ls->l_sem_otime;
	bs->sem_ctime = ls->l_sem_ctime;
	bs->sem_nsems = ls->l_sem_nsems;
	bs->sem_base = ls->l_sem_base;
}

int
linux_semop(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_ipc_args /* {
		syscallarg(int) what;
		syscallarg(int) a1;
		syscallarg(int) a2;
		syscallarg(int) a3;
		syscallarg(caddr_t) ptr;
	} */ *uap = v;
	struct sys_semop_args bsa;

	SCARG(&bsa, semid) = SCARG(uap, a1);
	SCARG(&bsa, sops) = (struct sembuf *)SCARG(uap, ptr);
	SCARG(&bsa, nsops) = SCARG(uap, a2);

	return sys_semop(p, &bsa, retval);
}

int
linux_semget(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_ipc_args /* {
		syscallarg(int) what;
		syscallarg(int) a1;
		syscallarg(int) a2;
		syscallarg(int) a3;
		syscallarg(caddr_t) ptr;
	} */ *uap = v;
	struct sys_semget_args bsa;

	SCARG(&bsa, key) = (key_t)SCARG(uap, a1);
	SCARG(&bsa, nsems) = SCARG(uap, a2);
	SCARG(&bsa, semflg) = SCARG(uap, a3);

	return sys_semget(p, &bsa, retval);
}

/*
 * Most of this can be handled by directly passing the arguments on,
 * buf IPC_* require a lot of copy{in,out} because of the extra indirection
 * (we are passed a pointer to a union cointaining a pointer to a semid_ds
 * structure.
 */
int
linux_semctl(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_ipc_args /* {
		syscallarg(int) what;
		syscallarg(int) a1;
		syscallarg(int) a2;
		syscallarg(int) a3;
		syscallarg(caddr_t) ptr;
	} */ *uap = v;
	caddr_t sg, unptr, dsp, ldsp;
	int error, cmd;
	struct sys___semctl_args bsa;
	struct linux_semid_ds lm;
	struct semid_ds bm;

	SCARG(&bsa, semid) = SCARG(uap, a1);
	SCARG(&bsa, semnum) = SCARG(uap, a2);
	SCARG(&bsa, cmd) = SCARG(uap, a3);
	SCARG(&bsa, arg) = (union semun *)SCARG(uap, ptr);
	switch(SCARG(uap, a3)) {
	case LINUX_GETVAL:
		cmd = GETVAL;
		break;
	case LINUX_GETPID:
		cmd = GETPID;
		break;
	case LINUX_GETNCNT:
		cmd = GETNCNT;
		break;
	case LINUX_GETZCNT:
		cmd = GETZCNT;
		break;
	case LINUX_SETVAL:
		cmd = SETVAL;
		break;
	case LINUX_IPC_RMID:
		cmd = IPC_RMID;
		break;
	case LINUX_IPC_SET:
		if ((error = copyin(SCARG(uap, ptr), &ldsp, sizeof ldsp)))
			return error;
		if ((error = copyin(ldsp, (caddr_t)&lm, sizeof lm)))
			return error;
		linux_to_bsd_semid_ds(&lm, &bm);
		sg = stackgap_init(p->p_emul);
		unptr = stackgap_alloc(&sg, sizeof (union semun));
		dsp = stackgap_alloc(&sg, sizeof (struct semid_ds));
		if ((error = copyout((caddr_t)&bm, dsp, sizeof bm)))
			return error;
		if ((error = copyout((caddr_t)&dsp, unptr, sizeof dsp)))
			return error;
		SCARG(&bsa, arg) = (union semun *)unptr;
		return sys___semctl(p, &bsa, retval);
	case LINUX_IPC_STAT:
		sg = stackgap_init(p->p_emul);
		unptr = stackgap_alloc(&sg, sizeof (union semun));
		dsp = stackgap_alloc(&sg, sizeof (struct semid_ds));
		if ((error = copyout((caddr_t)&dsp, unptr, sizeof dsp)))
			return error;
		SCARG(&bsa, arg) = (union semun *)unptr;
		if ((error = sys___semctl(p, &bsa, retval)))
			return error;
		if ((error = copyin(dsp, (caddr_t)&bm, sizeof bm)))
			return error;
		bsd_to_linux_semid_ds(&bm, &lm);
		if ((error = copyin(SCARG(uap, ptr), &ldsp, sizeof ldsp)))
			return error;
		return copyout((caddr_t)&lm, ldsp, sizeof lm);
	default:
		return EINVAL;
	}
	SCARG(&bsa, cmd) = cmd;

	return sys___semctl(p, &bsa, retval);
}
#endif /* SYSVSEM */

#ifdef SYSVMSG

void
linux_to_bsd_msqid_ds(lmp, bmp)
	struct linux_msqid_ds *lmp;
	struct msqid_ds *bmp;
{

	linux_to_bsd_ipc_perm(&lmp->l_msg_perm, &bmp->msg_perm);
	bmp->msg_first = lmp->l_msg_first;
	bmp->msg_last = lmp->l_msg_last;
	bmp->msg_cbytes = lmp->l_msg_cbytes;
	bmp->msg_qnum = lmp->l_msg_qnum;
	bmp->msg_qbytes = lmp->l_msg_qbytes;
	bmp->msg_lspid = lmp->l_msg_lspid;
	bmp->msg_lrpid = lmp->l_msg_lrpid;
	bmp->msg_stime = lmp->l_msg_stime;
	bmp->msg_rtime = lmp->l_msg_rtime;
	bmp->msg_ctime = lmp->l_msg_ctime;
}

void
bsd_to_linux_msqid_ds(bmp, lmp)
	struct msqid_ds *bmp;
	struct linux_msqid_ds *lmp;
{

	bsd_to_linux_ipc_perm(&bmp->msg_perm, &lmp->l_msg_perm);
	lmp->l_msg_first = bmp->msg_first;
	lmp->l_msg_last = bmp->msg_last;
	lmp->l_msg_cbytes = bmp->msg_cbytes;
	lmp->l_msg_qnum = bmp->msg_qnum;
	lmp->l_msg_qbytes = bmp->msg_qbytes;
	lmp->l_msg_lspid = bmp->msg_lspid;
	lmp->l_msg_lrpid = bmp->msg_lrpid;
	lmp->l_msg_stime = bmp->msg_stime;
	lmp->l_msg_rtime = bmp->msg_rtime;
	lmp->l_msg_ctime = bmp->msg_ctime;
}

int
linux_msgsnd(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_ipc_args /* {
		syscallarg(int) what;
		syscallarg(int) a1;
		syscallarg(int) a2;
		syscallarg(int) a3;
		syscallarg(caddr_t) ptr;
	} */ *uap = v;
	struct sys_msgsnd_args bma;

	SCARG(&bma, msqid) = SCARG(uap, a1);
	SCARG(&bma, msgp) = SCARG(uap, ptr);
	SCARG(&bma, msgsz) = SCARG(uap, a2);
	SCARG(&bma, msgflg) = SCARG(uap, a3);

	return sys_msgsnd(p, &bma, retval);
}

int
linux_msgrcv(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_ipc_args /* {
		syscallarg(int) what;
		syscallarg(int) a1;
		syscallarg(int) a2;
		syscallarg(int) a3;
		syscallarg(caddr_t) ptr;
	} */ *uap = v;
	struct sys_msgrcv_args bma;
	struct linux_msgrcv_msgarg kluge;
	int error;

	if ((error = copyin(SCARG(uap, ptr), &kluge, sizeof kluge)))
		return error;

	SCARG(&bma, msqid) = SCARG(uap, a1);
	SCARG(&bma, msgp) = kluge.msg;
	SCARG(&bma, msgsz) = SCARG(uap, a2);
	SCARG(&bma, msgtyp) = kluge.type;
	SCARG(&bma, msgflg) = SCARG(uap, a3);

	return sys_msgrcv(p, &bma, retval);
}

int
linux_msgget(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_ipc_args /* {
		syscallarg(int) what;
		syscallarg(int) a1;
		syscallarg(int) a2;
		syscallarg(int) a3;
		syscallarg(caddr_t) ptr;
	} */ *uap = v;
	struct sys_msgget_args bma;

	SCARG(&bma, key) = (key_t)SCARG(uap, a1);
	SCARG(&bma, msgflg) = SCARG(uap, a2);

	return sys_msgget(p, &bma, retval);
}

int
linux_msgctl(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_ipc_args /* {
		syscallarg(int) what;
		syscallarg(int) a1;
		syscallarg(int) a2;
		syscallarg(int) a3;
		syscallarg(caddr_t) ptr;
	} */ *uap = v;
	struct sys_msgctl_args bma;
	caddr_t umsgptr, sg;
	struct linux_msqid_ds lm;
	struct msqid_ds bm;
	int error;

	SCARG(&bma, msqid) = SCARG(uap, a1);
	SCARG(&bma, cmd) = SCARG(uap, a2);
	switch (SCARG(uap, a2)) {
	case LINUX_IPC_RMID:
		return sys_msgctl(p, &bma, retval);
	case LINUX_IPC_SET:
		if ((error = copyin(SCARG(uap, ptr), (caddr_t)&lm, sizeof lm)))
			return error;
		linux_to_bsd_msqid_ds(&lm, &bm);
		sg = stackgap_init(p->p_emul);
		umsgptr = stackgap_alloc(&sg, sizeof bm);
		if ((error = copyout((caddr_t)&bm, umsgptr, sizeof bm)))
			return error;
		SCARG(&bma, buf) = (struct msqid_ds *)umsgptr;
		return sys_msgctl(p, &bma, retval);
	case LINUX_IPC_STAT:
		sg = stackgap_init(p->p_emul);
		umsgptr = stackgap_alloc(&sg, sizeof (struct msqid_ds));
		SCARG(&bma, buf) = (struct msqid_ds *)umsgptr;
		if ((error = sys_msgctl(p, &bma, retval)))
			return error;
		if ((error = copyin(umsgptr, (caddr_t)&bm, sizeof bm)))
			return error;
		bsd_to_linux_msqid_ds(&bm, &lm);
		return copyout((caddr_t)&lm, SCARG(uap, ptr), sizeof lm);
	}
	return EINVAL;
}
#endif /* SYSVMSG */

#ifdef SYSVSHM
/*
 * shmat(2). Very straightforward, except that Linux passes a pointer
 * in which the return value is to be passed. This is subsequently
 * handled by libc, apparently.
 */
int
linux_shmat(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_ipc_args /* {
		syscallarg(int) what;
		syscallarg(int) a1;
		syscallarg(int) a2;
		syscallarg(int) a3;
		syscallarg(caddr_t) ptr;
	} */ *uap = v;
	struct sys_shmat_args bsa;
	int error;

	SCARG(&bsa, shmid) = SCARG(uap, a1);
	SCARG(&bsa, shmaddr) = SCARG(uap, ptr);
	SCARG(&bsa, shmflg) = SCARG(uap, a2);

	if ((error = sys_shmat(p, &bsa, retval)))
		return error;

	if ((error = copyout(&retval[0], (caddr_t) SCARG(uap, a3),
	     sizeof retval[0])))
		return error;

	retval[0] = 0;
	return 0;
}

/*
 * shmdt(): this could have been mapped directly, if it wasn't for
 * the extra indirection by the linux_ipc system call.
 */
int
linux_shmdt(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_ipc_args /* {
		syscallarg(int) what;
		syscallarg(int) a1;
		syscallarg(int) a2;
		syscallarg(int) a3;
		syscallarg(caddr_t) ptr;
	} */ *uap = v;
	struct sys_shmdt_args bsa;

	SCARG(&bsa, shmaddr) = SCARG(uap, ptr);

	return sys_shmdt(p, &bsa, retval);
}

/*
 * Same story as shmdt.
 */
int
linux_shmget(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_ipc_args /* {
		syscallarg(int) what;
		syscallarg(int) a1;
		syscallarg(int) a2;
		syscallarg(int) a3;
		syscallarg(caddr_t) ptr;
	} */ *uap = v;
	struct sys_shmget_args bsa;

	SCARG(&bsa, key) = SCARG(uap, a1);
	SCARG(&bsa, size) = SCARG(uap, a2);
	SCARG(&bsa, shmflg) = SCARG(uap, a3);

	return sys_shmget(p, &bsa, retval);
}

/*
 * Convert between Linux and OpenBSD shmid_ds structures.
 * The order of the fields is once again the difference, and
 * we also need a place to store the internal data pointer
 * in, which is unfortunately stored in this structure.
 *
 * We abuse a Linux internal field for that.
 */
void
linux_to_bsd_shmid_ds(lsp, bsp)
	struct linux_shmid_ds *lsp;
	struct shmid_ds *bsp;
{

	linux_to_bsd_ipc_perm(&lsp->l_shm_perm, &bsp->shm_perm);
	bsp->shm_segsz = lsp->l_shm_segsz;
	bsp->shm_lpid = lsp->l_shm_lpid;
	bsp->shm_cpid = lsp->l_shm_cpid;
	bsp->shm_nattch = lsp->l_shm_nattch;
	bsp->shm_atime = lsp->l_shm_atime;
	bsp->shm_dtime = lsp->l_shm_dtime;
	bsp->shm_ctime = lsp->l_shm_ctime;
	bsp->shm_internal = lsp->l_private2;	/* XXX Oh well. */
}

void
bsd_to_linux_shmid_ds(bsp, lsp)
	struct shmid_ds *bsp;
	struct linux_shmid_ds *lsp;
{

	bsd_to_linux_ipc_perm(&bsp->shm_perm, &lsp->l_shm_perm);
	lsp->l_shm_segsz = bsp->shm_segsz;
	lsp->l_shm_lpid = bsp->shm_lpid;
	lsp->l_shm_cpid = bsp->shm_cpid;
	lsp->l_shm_nattch = bsp->shm_nattch;
	lsp->l_shm_atime = bsp->shm_atime;
	lsp->l_shm_dtime = bsp->shm_dtime;
	lsp->l_shm_ctime = bsp->shm_ctime;
	lsp->l_private2 = bsp->shm_internal;	/* XXX */
}

/*
 * shmctl. Not implemented (for now): IPC_INFO, SHM_INFO, SHM_STAT
 * SHM_LOCK and SHM_UNLOCK are passed on, but currently not implemented
 * by OpenBSD itself.
 *
 * The usual structure conversion and massaging is done.
 */
int
linux_shmctl(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_ipc_args /* {
		syscallarg(int) what;
		syscallarg(int) a1;
		syscallarg(int) a2;
		syscallarg(int) a3;
		syscallarg(caddr_t) ptr;
	} */ *uap = v;
	int error;
	caddr_t sg;
	struct sys_shmctl_args bsa;
	struct shmid_ds *bsp, bs;
	struct linux_shmid_ds lseg;

	switch (SCARG(uap, a2)) {
	case LINUX_IPC_STAT:
		sg = stackgap_init(p->p_emul);
		bsp = stackgap_alloc(&sg, sizeof (struct shmid_ds));
		SCARG(&bsa, shmid) = SCARG(uap, a1);
		SCARG(&bsa, cmd) = IPC_STAT;
		SCARG(&bsa, buf) = bsp;
		if ((error = sys_shmctl(p, &bsa, retval)))
			return error;
		if ((error = copyin((caddr_t) bsp, (caddr_t) &bs, sizeof bs)))
			return error;
		bsd_to_linux_shmid_ds(&bs, &lseg);
		return copyout((caddr_t) &lseg, SCARG(uap, ptr), sizeof lseg);
	case LINUX_IPC_SET:
		if ((error = copyin(SCARG(uap, ptr), (caddr_t) &lseg,
		     sizeof lseg)))
			return error;
		linux_to_bsd_shmid_ds(&lseg, &bs);
		sg = stackgap_init(p->p_emul);
		bsp = stackgap_alloc(&sg, sizeof (struct shmid_ds));
		if ((error = copyout((caddr_t) &bs, (caddr_t) bsp, sizeof bs)))
			return error;
		SCARG(&bsa, shmid) = SCARG(uap, a1);
		SCARG(&bsa, cmd) = IPC_SET;
		SCARG(&bsa, buf) = bsp;
		return sys_shmctl(p, &bsa, retval);
	case LINUX_IPC_RMID:
	case LINUX_SHM_LOCK:
	case LINUX_SHM_UNLOCK:
		SCARG(&bsa, shmid) = SCARG(uap, a1);
		switch (SCARG(uap, a2)) {
		case LINUX_IPC_RMID:
			SCARG(&bsa, cmd) = IPC_RMID;
			break;
		case LINUX_SHM_LOCK:
			SCARG(&bsa, cmd) = SHM_LOCK;
			break;
		case LINUX_SHM_UNLOCK:
			SCARG(&bsa, cmd) = SHM_UNLOCK;
			break;
		}
		SCARG(&bsa, buf) = NULL;
		return sys_shmctl(p, &bsa, retval);
	case LINUX_IPC_INFO:
	case LINUX_SHM_STAT:
	case LINUX_SHM_INFO:
	default:
		return EINVAL;
	}
}
#endif /* SYSVSHM */

int
linux_sys_pipe2(struct proc *p, void *v, register_t *retval)
{
	struct linux_sys_pipe2_args *uap = v;
	struct sys_pipe_args pargs;

	/*
	 * We don't really support pipe2, but glibc falls back to pipe
	 * if we signal that.
	 */
	if (SCARG(uap, flags) != 0)
		return ENOSYS;

	/* If no flag is set then this is a plain pipe call. */
	SCARG(&pargs, fdp) = SCARG(uap, fdp);
	return sys_pipe(p, &pargs, retval);
}
