/*	$OpenBSD: svr4_ipc.c,v 1.9 2002/03/14 01:26:51 millert Exp $	*/
/*	$NetBSD: svr4_ipc.c,v 1.3 1997/03/30 17:21:02 christos Exp $	*/

/*
 * Copyright (c) 1995 Christos Zoulas.  All rights reserved.
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
 *	This product includes software developed by Christos Zoulas.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 * Mostly stolen from: linux_ipc.c,v 1.4 1995/06/24 20:20:22 christos Exp
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/proc.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <sys/malloc.h>
#include <sys/mman.h>
#include <sys/systm.h>
#include <sys/stat.h>

#include <sys/mount.h>
#include <sys/syscallargs.h>

#include <compat/svr4/svr4_types.h>
#include <compat/svr4/svr4_signal.h>
#include <compat/svr4/svr4_syscallargs.h>
#include <compat/svr4/svr4_util.h>
#include <compat/svr4/svr4_ipc.h>

#if defined(SYSVMSG) || defined(SYSVSHM) || defined(SYSVSEM)
static void svr4_to_bsd_ipc_perm(const struct svr4_ipc_perm *,
				      struct ipc_perm *);
static void bsd_to_svr4_ipc_perm(const struct ipc_perm *,
				      struct svr4_ipc_perm *);
#endif

#ifdef SYSVSEM
static void bsd_to_svr4_semid_ds(const struct semid_ds *,
				      struct svr4_semid_ds *);
static void svr4_to_bsd_semid_ds(const struct svr4_semid_ds *,
				      struct semid_ds *);
static int svr4_semop(struct proc *, void *, register_t *);
static int svr4_semget(struct proc *, void *, register_t *);
static int svr4_semctl(struct proc *, void *, register_t *);
#endif

#ifdef SYSVMSG
static void bsd_to_svr4_msqid_ds(const struct msqid_ds *,
				      struct svr4_msqid_ds *);
static void svr4_to_bsd_msqid_ds(const struct svr4_msqid_ds *,
				      struct msqid_ds *);
static int svr4_msgsnd(struct proc *, void *, register_t *);
static int svr4_msgrcv(struct proc *, void *, register_t *);
static int svr4_msgget(struct proc *, void *, register_t *);
static int svr4_msgctl(struct proc *, void *, register_t *);
#endif

#ifdef SYSVSHM
static void bsd_to_svr4_shmid_ds(const struct shmid_ds *,
				      struct svr4_shmid_ds *);
static void svr4_to_bsd_shmid_ds(const struct svr4_shmid_ds *,
				      struct shmid_ds *);
static int svr4_shmat(struct proc *, void *, register_t *);
static int svr4_shmdt(struct proc *, void *, register_t *);
static int svr4_shmget(struct proc *, void *, register_t *);
static int svr4_shmctl(struct proc *, void *, register_t *);
#endif

#if defined(SYSVMSG) || defined(SYSVSHM) || defined(SYSVSEM)

static void
svr4_to_bsd_ipc_perm(spp, bpp)
	const struct svr4_ipc_perm *spp;
	struct ipc_perm *bpp;
{
	bpp->key = spp->key;
	bpp->uid = spp->uid;
	bpp->gid = spp->gid;
	bpp->cuid = spp->cuid;
	bpp->cgid = spp->cgid;
	bpp->mode = spp->mode;
	bpp->seq = spp->seq;
}

static void
bsd_to_svr4_ipc_perm(bpp, spp)
	const struct ipc_perm *bpp;
	struct svr4_ipc_perm *spp;
{
	spp->key = bpp->key;
	spp->uid = bpp->uid;
	spp->gid = bpp->gid;
	spp->cuid = bpp->cuid;
	spp->cgid = bpp->cgid;
	spp->mode = bpp->mode;
	spp->seq = bpp->seq;
}
#endif

#ifdef SYSVSEM
static void
bsd_to_svr4_semid_ds(bds, sds)
	const struct semid_ds *bds;
	struct svr4_semid_ds *sds;
{
	bsd_to_svr4_ipc_perm(&bds->sem_perm, &sds->sem_perm);
	sds->sem_base = (struct svr4_sem *) bds->sem_base;
	sds->sem_nsems = bds->sem_nsems;
	sds->sem_otime = bds->sem_otime;
	sds->sem_pad1 = bds->sem_pad1;
	sds->sem_ctime = bds->sem_ctime;
	sds->sem_pad2 = bds->sem_pad2;
}

static void
svr4_to_bsd_semid_ds(sds, bds)
	const struct svr4_semid_ds *sds;
	struct semid_ds *bds;
{
	svr4_to_bsd_ipc_perm(&sds->sem_perm, &bds->sem_perm);
	bds->sem_base = (struct sem *) sds->sem_base;
	bds->sem_nsems = sds->sem_nsems;
	bds->sem_otime = sds->sem_otime;
	bds->sem_pad1 = sds->sem_pad1;
	bds->sem_ctime = sds->sem_ctime;
	bds->sem_pad2 = sds->sem_pad2;
}

struct svr4_sys_semctl_args {
	syscallarg(int) what;
	syscallarg(int) semid;
	syscallarg(int) semnum;
	syscallarg(int) cmd;
	syscallarg(void *) arg;
};

static int
svr4_semctl(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	int error;
	struct svr4_sys_semctl_args *uap = v;
	struct sys___semctl_args ap;
	struct svr4_semid_ds ss;
	struct semid_ds bs;
	caddr_t sg = stackgap_init(p->p_emul);

	SCARG(&ap, semid) = SCARG(uap, semid);
	SCARG(&ap, semnum) = SCARG(uap, semnum);

	switch (SCARG(uap, cmd)) {
	case SVR4_SEM_GETZCNT:
	case SVR4_SEM_GETNCNT:
	case SVR4_SEM_GETPID:
	case SVR4_SEM_GETVAL:
		switch (SCARG(uap, cmd)) {
		case SVR4_SEM_GETZCNT:
			SCARG(&ap, cmd) = GETZCNT;
			break;
		case SVR4_SEM_GETNCNT:
			SCARG(&ap, cmd) = GETNCNT;
			break;
		case SVR4_SEM_GETPID:
			SCARG(&ap, cmd) = GETPID;
			break;
		case SVR4_SEM_GETVAL:
			SCARG(&ap, cmd) = GETVAL;
			break;
		}
		return sys___semctl(p, &ap, retval);

	case SVR4_SEM_SETVAL:
		SCARG(&ap, arg)->val = (int) SCARG(uap, arg);
		SCARG(&ap, cmd) = SETVAL;
		return sys___semctl(p, &ap, retval);

	case SVR4_SEM_GETALL:
		SCARG(&ap, arg)->array = SCARG(uap, arg);
		SCARG(&ap, cmd) = GETVAL;
		return sys___semctl(p, &ap, retval);

	case SVR4_SEM_SETALL:
		SCARG(&ap, arg)->array = SCARG(uap, arg);
		SCARG(&ap, cmd) = SETVAL;
		return sys___semctl(p, &ap, retval);

	case SVR4_IPC_STAT:
		SCARG(&ap, cmd) = IPC_STAT;
		SCARG(&ap, arg)->buf = stackgap_alloc(&sg, sizeof(bs));
		if ((error = sys___semctl(p, &ap, retval)) != 0)
			return error;
		error = copyin(SCARG(&ap, arg)->buf, &bs, sizeof bs);
		if (error)
			return error;
		bsd_to_svr4_semid_ds(&bs, &ss);
		return copyout(&ss, SCARG(uap, arg), sizeof ss);

	case SVR4_IPC_SET:
		SCARG(&ap, cmd) = IPC_SET;
		SCARG(&ap, arg)->buf = stackgap_alloc(&sg, sizeof(bs));
		error = copyin(SCARG(uap, arg), (caddr_t) &ss, sizeof ss);
		if (error)
			return error;
		svr4_to_bsd_semid_ds(&ss, &bs);
		error = copyout(&bs, SCARG(&ap, arg)->buf, sizeof bs);
		if (error)
			return error;
		return sys___semctl(p, &ap, retval);

	case SVR4_IPC_RMID:
		SCARG(&ap, cmd) = IPC_RMID;
		SCARG(&ap, arg)->buf = stackgap_alloc(&sg, sizeof(bs));
		error = copyin(SCARG(uap, arg), &ss, sizeof ss);
		if (error)
			return error;
		svr4_to_bsd_semid_ds(&ss, &bs);
		error = copyout(&bs, SCARG(&ap, arg)->buf, sizeof bs);
		if (error)
			return error;
		return sys___semctl(p, &ap, retval);

	default:
		return EINVAL;
	}
}

struct svr4_sys_semget_args {
	syscallarg(int) what;
	syscallarg(svr4_key_t) key;
	syscallarg(int) nsems;
	syscallarg(int) semflg;
};

static int
svr4_semget(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_semget_args *uap = v;
	struct sys_semget_args ap;

	SCARG(&ap, key) = SCARG(uap, key);
	SCARG(&ap, nsems) = SCARG(uap, nsems);
	SCARG(&ap, semflg) = SCARG(uap, semflg);

	return sys_semget(p, &ap, retval);
}

struct svr4_sys_semop_args {
	syscallarg(int) what;
	syscallarg(int) semid;
	syscallarg(struct svr4_sembuf *) sops;
	syscallarg(u_int) nsops;
};

static int
svr4_semop(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_semop_args *uap = v;
	struct sys_semop_args ap;

	SCARG(&ap, semid) = SCARG(uap, semid);
	/* These are the same */
	SCARG(&ap, sops) = (struct sembuf *) SCARG(uap, sops);
	SCARG(&ap, nsops) = SCARG(uap, nsops);

	return sys_semop(p, &ap, retval);
}

int
svr4_sys_semsys(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_semsys_args *uap = v;

	DPRINTF(("svr4_semsys(%d)\n", SCARG(uap, what)));

	switch (SCARG(uap, what)) {
	case SVR4_semctl:
		return svr4_semctl(p, v, retval);
	case SVR4_semget:
		return svr4_semget(p, v, retval);
	case SVR4_semop:
		return svr4_semop(p, v, retval);
	default:
		return EINVAL;
	}
}
#endif

#ifdef SYSVMSG
static void
bsd_to_svr4_msqid_ds(bds, sds)
	const struct msqid_ds *bds;
	struct svr4_msqid_ds *sds;
{
	bsd_to_svr4_ipc_perm(&bds->msg_perm, &sds->msg_perm);
	sds->msg_first = (struct svr4_msg *) bds->msg_first;
	sds->msg_last = (struct svr4_msg *) bds->msg_last;
	sds->msg_cbytes = bds->msg_cbytes;
	sds->msg_qnum = bds->msg_qnum;
	sds->msg_qbytes = bds->msg_qbytes;
	sds->msg_lspid = bds->msg_lspid;
	sds->msg_lrpid = bds->msg_lrpid;
	sds->msg_stime = bds->msg_stime;
	sds->msg_pad1 = bds->msg_pad1;
	sds->msg_rtime = bds->msg_rtime;
	sds->msg_pad2 = bds->msg_pad2;
	sds->msg_ctime = bds->msg_ctime;
	sds->msg_pad3 = bds->msg_pad3;

	/* use the padding for the rest of the fields */
	{
		const short *pad = (const short *) bds->msg_pad4;
		sds->msg_cv = pad[0];
		sds->msg_qnum_cv = pad[1];
	}
}

static void
svr4_to_bsd_msqid_ds(sds, bds)
	const struct svr4_msqid_ds *sds;
	struct msqid_ds *bds;
{
	svr4_to_bsd_ipc_perm(&sds->msg_perm, &bds->msg_perm);
	bds->msg_first = (struct msg *) sds->msg_first;
	bds->msg_last = (struct msg *) sds->msg_last;
	bds->msg_cbytes = sds->msg_cbytes;
	bds->msg_qnum = sds->msg_qnum;
	bds->msg_qbytes = sds->msg_qbytes;
	bds->msg_lspid = sds->msg_lspid;
	bds->msg_lrpid = sds->msg_lrpid;
	bds->msg_stime = sds->msg_stime;
	bds->msg_pad1 = sds->msg_pad1;
	bds->msg_rtime = sds->msg_rtime;
	bds->msg_pad2 = sds->msg_pad2;
	bds->msg_ctime = sds->msg_ctime;
	bds->msg_pad3 = sds->msg_pad3;

	/* use the padding for the rest of the fields */
	{
		short *pad = (short *) bds->msg_pad4;
		pad[0] = sds->msg_cv;
		pad[1] = sds->msg_qnum_cv;
	}
}

struct svr4_sys_msgsnd_args {
	syscallarg(int) what;
	syscallarg(int) msqid;
	syscallarg(void *) msgp;
	syscallarg(size_t) msgsz;
	syscallarg(int) msgflg;
};

static int
svr4_msgsnd(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_msgsnd_args *uap = v;
	struct sys_msgsnd_args ap;

	SCARG(&ap, msqid) = SCARG(uap, msqid);
	SCARG(&ap, msgp) = SCARG(uap, msgp);
	SCARG(&ap, msgsz) = SCARG(uap, msgsz);
	SCARG(&ap, msgflg) = SCARG(uap, msgflg);

	return sys_msgsnd(p, &ap, retval);
}

struct svr4_sys_msgrcv_args {
	syscallarg(int) what;
	syscallarg(int) msqid;
	syscallarg(void *) msgp;
	syscallarg(size_t) msgsz;
	syscallarg(long) msgtyp;
	syscallarg(int) msgflg;
};

static int
svr4_msgrcv(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_msgrcv_args *uap = v;
	struct sys_msgrcv_args ap;

	SCARG(&ap, msqid) = SCARG(uap, msqid);
	SCARG(&ap, msgp) = SCARG(uap, msgp);
	SCARG(&ap, msgsz) = SCARG(uap, msgsz);
	SCARG(&ap, msgtyp) = SCARG(uap, msgtyp);
	SCARG(&ap, msgflg) = SCARG(uap, msgflg);

	return sys_msgrcv(p, &ap, retval);
}
	
struct svr4_sys_msgget_args {
	syscallarg(int) what;
	syscallarg(svr4_key_t) key;
	syscallarg(int) msgflg;
};

static int
svr4_msgget(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_msgget_args *uap = v;
	struct sys_msgget_args ap;

	SCARG(&ap, key) = SCARG(uap, key);
	SCARG(&ap, msgflg) = SCARG(uap, msgflg);

	return sys_msgget(p, &ap, retval);
}

struct svr4_sys_msgctl_args {
	syscallarg(int) what;
	syscallarg(int) msqid;
	syscallarg(int) cmd;
	syscallarg(struct svr4_msqid_ds *) buf;
};

static int
svr4_msgctl(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	int error;
	struct svr4_sys_msgctl_args *uap = v;
	struct sys_msgctl_args ap;
	struct svr4_msqid_ds ss;
	struct msqid_ds bs;
	caddr_t sg = stackgap_init(p->p_emul);

	SCARG(&ap, msqid) = SCARG(uap, msqid);
	SCARG(&ap, cmd) = SCARG(uap, cmd);
	SCARG(&ap, buf) = stackgap_alloc(&sg, sizeof(bs));

	switch (SCARG(uap, cmd)) {
	case SVR4_IPC_STAT:
		SCARG(&ap, cmd) = IPC_STAT;
		if ((error = sys_msgctl(p, &ap, retval)) != 0)
			return error;
		error = copyin(SCARG(&ap, buf), &bs, sizeof bs);
		if (error)
			return error;
		bsd_to_svr4_msqid_ds(&bs, &ss);
		return copyout(&ss, SCARG(uap, buf), sizeof ss);

	case SVR4_IPC_SET:
		SCARG(&ap, cmd) = IPC_SET;
		error = copyin(SCARG(uap, buf), &ss, sizeof ss);
		if (error)
			return error;
		svr4_to_bsd_msqid_ds(&ss, &bs);
		error = copyout(&bs, SCARG(&ap, buf), sizeof bs);
		if (error)
			return error;
		return sys_msgctl(p, &ap, retval);

	case SVR4_IPC_RMID:
		SCARG(&ap, cmd) = IPC_RMID;
		error = copyin(SCARG(uap, buf), &ss, sizeof ss);
		if (error)
			return error;
		svr4_to_bsd_msqid_ds(&ss, &bs);
		error = copyout(&bs, SCARG(&ap, buf), sizeof bs);
		if (error)
			return error;
		return sys_msgctl(p, &ap, retval);

	default:
		return EINVAL;
	}
}

int
svr4_sys_msgsys(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_msgsys_args *uap = v;

	DPRINTF(("svr4_msgsys(%d)\n", SCARG(uap, what)));

	switch (SCARG(uap, what)) {
	case SVR4_msgsnd:
		return svr4_msgsnd(p, v, retval);
	case SVR4_msgrcv:
		return svr4_msgrcv(p, v, retval);
	case SVR4_msgget:
		return svr4_msgget(p, v, retval);
	case SVR4_msgctl:
		return svr4_msgctl(p, v, retval);
	default:
		return EINVAL;
	}
}
#endif

#ifdef SYSVSHM

static void
bsd_to_svr4_shmid_ds(bds, sds)
	const struct shmid_ds *bds;
	struct svr4_shmid_ds *sds;
{
	bsd_to_svr4_ipc_perm(&bds->shm_perm, &sds->shm_perm);
	sds->shm_segsz = bds->shm_segsz;
	sds->shm_lkcnt = 0;
	sds->shm_lpid = bds->shm_lpid;
	sds->shm_cpid = bds->shm_cpid;
	sds->shm_amp = bds->shm_internal;
	sds->shm_nattch = bds->shm_nattch;
	sds->shm_cnattch = 0;
	sds->shm_atime = bds->shm_atime;
	sds->shm_pad1 = 0;
	sds->shm_dtime = bds->shm_dtime;
	sds->shm_pad2 = 0;
	sds->shm_ctime = bds->shm_ctime;
	sds->shm_pad3 = 0;
}

static void
svr4_to_bsd_shmid_ds(sds, bds)
	const struct svr4_shmid_ds *sds;
	struct shmid_ds *bds;
{
	svr4_to_bsd_ipc_perm(&sds->shm_perm, &bds->shm_perm);
	bds->shm_segsz = sds->shm_segsz;
	bds->shm_lpid = sds->shm_lpid;
	bds->shm_cpid = sds->shm_cpid;
	bds->shm_internal = sds->shm_amp;
	bds->shm_nattch = sds->shm_nattch;
	bds->shm_atime = sds->shm_atime;
	bds->shm_dtime = sds->shm_dtime;
	bds->shm_ctime = sds->shm_ctime;
}

struct svr4_sys_shmat_args {
	syscallarg(int) what;
	syscallarg(int) shmid;
	syscallarg(void *) shmaddr;
	syscallarg(int) shmflg;
};

static int
svr4_shmat(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_shmat_args *uap = v;
	struct sys_shmat_args ap;

	SCARG(&ap, shmid) = SCARG(uap, shmid);
	SCARG(&ap, shmaddr) = SCARG(uap, shmaddr);
	SCARG(&ap, shmflg) = SCARG(uap, shmflg);

	return sys_shmat(p, &ap, retval);
}

struct svr4_sys_shmdt_args {
	syscallarg(int) what;
	syscallarg(void *) shmaddr;
};

static int
svr4_shmdt(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_shmdt_args *uap = v;
	struct sys_shmdt_args ap;

	SCARG(&ap, shmaddr) = SCARG(uap, shmaddr);

	return sys_shmdt(p, &ap, retval);
}

struct svr4_sys_shmget_args {
	syscallarg(int) what;
	syscallarg(key_t) key;
	syscallarg(int) size;
	syscallarg(int) shmflg;
};

static int
svr4_shmget(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_shmget_args *uap = v;
	struct sys_shmget_args ap;

	SCARG(&ap, key) = SCARG(uap, key);
	SCARG(&ap, size) = SCARG(uap, size);
	SCARG(&ap, shmflg) = SCARG(uap, shmflg);

	return sys_shmget(p, &ap, retval);
}

struct svr4_sys_shmctl_args {
	syscallarg(int) what;
	syscallarg(int) shmid;
	syscallarg(int) cmd;
	syscallarg(struct svr4_shmid_ds *) buf;
};

int
svr4_shmctl(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_shmctl_args *uap = v;
	int error;
	caddr_t sg = stackgap_init(p->p_emul);
	struct sys_shmctl_args ap;
	struct shmid_ds bs;
	struct svr4_shmid_ds ss;

	SCARG(&ap, shmid) = SCARG(uap, shmid);

	if (SCARG(uap, buf) != NULL) {
		SCARG(&ap, buf) = stackgap_alloc(&sg, sizeof (struct shmid_ds));
		switch (SCARG(uap, cmd)) {
                case SVR4_IPC_SET:
                case SVR4_IPC_RMID:
                case SVR4_SHM_LOCK:
                case SVR4_SHM_UNLOCK:
			error = copyin(SCARG(uap, buf), (caddr_t) &ss,
                            sizeof ss);
                        if (error)
                                return error;
                        svr4_to_bsd_shmid_ds(&ss, &bs);
                        error = copyout(&bs, SCARG(&ap, buf), sizeof bs);
                        if (error)
                                return error;
                        break;
                default:
                        break;
                }
	}
	else
		SCARG(&ap, buf) = NULL;


	switch (SCARG(uap, cmd)) {
	case SVR4_IPC_STAT:
		SCARG(&ap, cmd) = IPC_STAT;
		if ((error = sys_shmctl(p, &ap, retval)) != 0)
			return error;
		if (SCARG(uap, buf) == NULL)
			return 0;
		error = copyin(SCARG(&ap, buf), &bs, sizeof bs);
		if (error)
			return error;
		bsd_to_svr4_shmid_ds(&bs, &ss);
		return copyout(&ss, SCARG(uap, buf), sizeof ss);

	case SVR4_IPC_SET:
		SCARG(&ap, cmd) = IPC_SET;
		return sys_shmctl(p, &ap, retval);

	case SVR4_IPC_RMID:
	case SVR4_SHM_LOCK:
	case SVR4_SHM_UNLOCK:
		switch (SCARG(uap, cmd)) {
		case SVR4_IPC_RMID:
			SCARG(&ap, cmd) = IPC_RMID;
			break;
		case SVR4_SHM_LOCK:
			SCARG(&ap, cmd) = SHM_LOCK;
			break;
		case SVR4_SHM_UNLOCK:
			SCARG(&ap, cmd) = SHM_UNLOCK;
			break;
		default:
			return EINVAL;
		}
		return sys_shmctl(p, &ap, retval);

	default:
		return EINVAL;
	}
}

int
svr4_sys_shmsys(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_shmsys_args *uap = v;

	DPRINTF(("svr4_shmsys(%d)\n", SCARG(uap, what)));

	switch (SCARG(uap, what)) {
	case SVR4_shmat:
		return svr4_shmat(p, v, retval);
	case SVR4_shmdt:
		return svr4_shmdt(p, v, retval);
	case SVR4_shmget:
		return svr4_shmget(p, v, retval);
	case SVR4_shmctl:
		return svr4_shmctl(p, v, retval);
	default:
		return ENOSYS;
	}
}
#endif /* SYSVSHM */
