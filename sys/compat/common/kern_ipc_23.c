/*	$OpenBSD: kern_ipc_23.c,v 1.3 2002/12/17 23:11:31 millert Exp $	*/

/*
 * Implementation of SVID semaphores
 *
 * Author:  Daniel Boulet
 *
 * This software is provided ``AS IS'' without any warranties of any kind.
 */
/*
 * Implementation of SVID messages
 *
 * Author:  Daniel Boulet
 *
 * Copyright 1993 Daniel Boulet and RTMX Inc.
 *
 * This system call was implemented by Daniel Boulet under contract from RTMX.
 *
 * Redistribution and use in source forms, with and without modification,
 * are permitted provided that this entire comment appears intact.
 *
 * Redistribution in binary form may occur without any restrictions.
 * Obviously, it would be nice if you gave credit where credit is due
 * but requiring it would be too onerous.
 *
 * This software is provided ``AS IS'' without any warranties of any kind.
 */
/*
 * Copyright (c) 1994 Adam Glass and Charles M. Hannum.  All rights reserved.
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
 *	This product includes software developed by Adam Glass and Charles M.
 *	Hannum.
 * 4. The names of the authors may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/stat.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/malloc.h>
#include <sys/pool.h>

#include <sys/mount.h>
#include <sys/syscallargs.h>

#ifdef SYSVMSG

void msg_freehdr(struct msg *);

int
compat_23_sys_msgctl(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct compat_23_sys_msgctl_args /* {
		syscallarg(int) msqid;
		syscallarg(int) cmd;
		syscallarg(struct msqid_ds *) buf;
	} */ *uap = v;
	int msqid = SCARG(uap, msqid);
	int cmd = SCARG(uap, cmd);
	struct omsqid_ds *user_msqptr = SCARG(uap, buf);
	struct ucred *cred = p->p_ucred;
	int rval, error;
	struct omsqid_ds omsqbuf;
	register struct msqid_ds *msqptr;

#ifdef MSG_DEBUG_OK
	printf("call to msgctl(%d, %d, %p)\n", msqid, cmd, user_msqptr);
#endif

	msqid = IPCID_TO_IX(msqid);

	if (msqid < 0 || msqid >= msginfo.msgmni) {
#ifdef MSG_DEBUG_OK
		printf("msqid (%d) out of range (0<=msqid<%d)\n", msqid,
		    msginfo.msgmni);
#endif
		return(EINVAL);
	}

	msqptr = &msqids[msqid];

	if (msqptr->msg_qbytes == 0) {
#ifdef MSG_DEBUG_OK
		printf("no such msqid\n");
#endif
		return(EINVAL);
	}
	if (msqptr->msg_perm.seq != IPCID_TO_SEQ(SCARG(uap, msqid))) {
#ifdef MSG_DEBUG_OK
		printf("wrong sequence number\n");
#endif
		return(EINVAL);
	}

	error = rval = 0;

	switch (cmd) {

	case IPC_RMID:
	{
		struct msg *msghdr;
		if ((error = ipcperm(cred, &msqptr->msg_perm, IPC_M)) != 0)
			return(error);
		/* Free the message headers */
		msghdr = msqptr->msg_first;
		while (msghdr != NULL) {
			struct msg *msghdr_tmp;

			/* Free the segments of each message */
			msqptr->msg_cbytes -= msghdr->msg_ts;
			msqptr->msg_qnum--;
			msghdr_tmp = msghdr;
			msghdr = msghdr->msg_next;
			msg_freehdr(msghdr_tmp);
		}

#ifdef DIAGNOSTIC
		if (msqptr->msg_cbytes != 0)
			panic("sys_omsgctl: msg_cbytes is screwed up");
		if (msqptr->msg_qnum != 0)
			panic("sys_omsgctl: msg_qnum is screwed up");
#endif

		msqptr->msg_qbytes = 0;	/* Mark it as free */

		wakeup((caddr_t)msqptr);
	}

		break;

	case IPC_SET:
		if ((error = ipcperm(cred, &msqptr->msg_perm, IPC_M)))
			return(error);
		if ((error = copyin(user_msqptr, &omsqbuf, sizeof(omsqbuf))) != 0)
			return(error);
		if (omsqbuf.msg_qbytes > msqptr->msg_qbytes && cred->cr_uid != 0)
			return(EPERM);
		if (omsqbuf.msg_qbytes > msginfo.msgmnb) {
#ifdef MSG_DEBUG_OK
			printf("can't increase msg_qbytes beyond %d (truncating)\n",
			    msginfo.msgmnb);
#endif
			omsqbuf.msg_qbytes = msginfo.msgmnb;	/* silently restrict qbytes to system limit */
		}
		if (omsqbuf.msg_qbytes == 0) {
#ifdef MSG_DEBUG_OK
			printf("can't reduce msg_qbytes to 0\n");
#endif
			return(EINVAL);		/* non-standard errno! */
		}
		msqptr->msg_perm.uid = omsqbuf.msg_perm.uid;	/* change the owner */
		msqptr->msg_perm.gid = omsqbuf.msg_perm.gid;	/* change the owner */
		msqptr->msg_perm.mode = (msqptr->msg_perm.mode & ~0777) |
		    (omsqbuf.msg_perm.mode & 0777);
		msqptr->msg_qbytes = omsqbuf.msg_qbytes;
		msqptr->msg_ctime = time.tv_sec;
		break;

	case IPC_STAT:
		if ((error = ipcperm(cred, &msqptr->msg_perm, IPC_R))) {
#ifdef MSG_DEBUG_OK
			printf("requester doesn't have read access\n");
#endif
			return(error);
		}
		msqid_n2o(msqptr, &omsqbuf);
		error = copyout((caddr_t)&omsqbuf, user_msqptr, sizeof omsqbuf);
		break;

	default:
#ifdef MSG_DEBUG_OK
		printf("invalid command %d\n", cmd);
#endif
		return(EINVAL);
	}

	if (error == 0)
		*retval = rval;
	return(error);
}
#endif /* SYSVMSG */

#ifdef SYSVSHM

struct shmid_ds *shm_find_segment_by_shmid(int);
void shm_deallocate_segment(struct shmid_ds *);

#define SHMSEG_REMOVED		0x0400		/* XXX */

int
compat_23_sys_shmctl(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct compat_23_sys_shmctl_args /* {
		syscallarg(int) shmid;
		syscallarg(int) cmd;
		syscallarg(struct shmid_ds *) buf;
	} */ *uap = v;
	int error;
	struct ucred *cred = p->p_ucred;
	struct oshmid_ds oinbuf;
	struct shmid_ds *shmseg;
	extern int shm_last_free;

	shmseg = shm_find_segment_by_shmid(SCARG(uap, shmid));
	if (shmseg == NULL)
		return EINVAL;
	switch (SCARG(uap, cmd)) {
	case IPC_STAT:
		if ((error = ipcperm(cred, &shmseg->shm_perm, IPC_R)) != 0)
			return error;
		shmid_n2o(shmseg, &oinbuf);
		error = copyout((caddr_t)&oinbuf, SCARG(uap, buf),
		    sizeof(oinbuf));
		if (error)
			return error;
		break;
	case IPC_SET:
		if ((error = ipcperm(cred, &shmseg->shm_perm, IPC_M)) != 0)
			return error;
		error = copyin(SCARG(uap, buf), (caddr_t)&oinbuf,
		    sizeof(oinbuf));
		if (error)
			return error;
		shmseg->shm_perm.uid = oinbuf.shm_perm.uid;
		shmseg->shm_perm.gid = oinbuf.shm_perm.gid;
		shmseg->shm_perm.mode =
		    (shmseg->shm_perm.mode & ~ACCESSPERMS) |
		    (oinbuf.shm_perm.mode & ACCESSPERMS);
		shmseg->shm_ctime = time.tv_sec;
		break;
	case IPC_RMID:
		if ((error = ipcperm(cred, &shmseg->shm_perm, IPC_M)) != 0)
			return error;
		shmseg->shm_perm.key = IPC_PRIVATE;
		shmseg->shm_perm.mode |= SHMSEG_REMOVED;
		if (shmseg->shm_nattch <= 0) {
			shm_deallocate_segment(shmseg);
			shm_last_free = IPCID_TO_IX(SCARG(uap, shmid));
		}
		break;
	case SHM_LOCK:
	case SHM_UNLOCK:
	default:
		return EINVAL;
	}
	return 0;
}
#endif /* SYSVSHM */

#ifdef SYSVSEM

extern struct pool sema_pool;
void semundo_clear(int, int);

int
compat_23_sys___semctl(p, v, retval)
	struct proc *p;
	register void *v;
	register_t *retval;
{
	register struct compat_23_sys___semctl_args /* {
		syscallarg(int) semid;
		syscallarg(int) semnum;
		syscallarg(int) cmd;
		syscallarg(union semun *) arg;
	} */ *uap = v;
	int semid = SCARG(uap, semid);
	int semnum = SCARG(uap, semnum);
	int cmd = SCARG(uap, cmd);
	union semun *arg = SCARG(uap, arg);
	union semun real_arg;
	struct ucred *cred = p->p_ucred;
	int i, rval, error;
	struct semid_ds *semaptr;
	struct osemid_ds osbuf;
	extern int semtot;

#ifdef SEM_DEBUG
	printf("call to semctl(%d, %d, %d, %p)\n", semid, semnum, cmd, arg);
#endif

	semid = IPCID_TO_IX(semid);
	if (semid < 0 || semid >= seminfo.semmsl)
		return(EINVAL);

	if ((semaptr = sema[semid]) == NULL ||
	    semaptr->sem_perm.seq != IPCID_TO_SEQ(SCARG(uap, semid)))
		return(EINVAL);

	error = rval = 0;

	switch (cmd) {
	case IPC_RMID:
		if ((error = ipcperm(cred, &semaptr->sem_perm, IPC_M)) != 0)
			return(error);
		semaptr->sem_perm.cuid = cred->cr_uid;
		semaptr->sem_perm.uid = cred->cr_uid;
		semtot -= semaptr->sem_nsems;
		free(semaptr->sem_base, M_SHM);
		pool_put(&sema_pool, semaptr);
		sema[semid] = NULL;
		semundo_clear(semid, -1);
		wakeup((caddr_t)&sema[semid]);
		break;

	case IPC_SET:
		if ((error = ipcperm(cred, &semaptr->sem_perm, IPC_M)))
			return(error);
		if ((error = copyin(arg, &real_arg, sizeof(real_arg))) != 0)
			return(error);
		if ((error = copyin(real_arg.buf, (caddr_t)&osbuf,
		    sizeof(osbuf))) != 0)
			return(error);
		semaptr->sem_perm.uid = osbuf.sem_perm.uid;
		semaptr->sem_perm.gid = osbuf.sem_perm.gid;
		semaptr->sem_perm.mode = (semaptr->sem_perm.mode & ~0777) |
		    (osbuf.sem_perm.mode & 0777);
		semaptr->sem_ctime = time.tv_sec;
		break;

	case IPC_STAT:
		if ((error = ipcperm(cred, &semaptr->sem_perm, IPC_R)))
			return(error);
		if ((error = copyin(arg, &real_arg, sizeof(real_arg))) != 0)
			return(error);
		semid_n2o(semaptr, &osbuf);
		error = copyout((caddr_t)&osbuf, real_arg.buf, sizeof(osbuf));
		break;

	case GETNCNT:
		if ((error = ipcperm(cred, &semaptr->sem_perm, IPC_R)))
			return(error);
		if (semnum < 0 || semnum >= semaptr->sem_nsems)
			return(EINVAL);
		rval = semaptr->sem_base[semnum].semncnt;
		break;

	case GETPID:
		if ((error = ipcperm(cred, &semaptr->sem_perm, IPC_R)))
			return(error);
		if (semnum < 0 || semnum >= semaptr->sem_nsems)
			return(EINVAL);
		rval = semaptr->sem_base[semnum].sempid;
		break;

	case GETVAL:
		if ((error = ipcperm(cred, &semaptr->sem_perm, IPC_R)))
			return(error);
		if (semnum < 0 || semnum >= semaptr->sem_nsems)
			return(EINVAL);
		rval = semaptr->sem_base[semnum].semval;
		break;

	case GETALL:
		if ((error = ipcperm(cred, &semaptr->sem_perm, IPC_R)))
			return(error);
		if ((error = copyin(arg, &real_arg, sizeof(real_arg))) != 0)
			return(error);
		for (i = 0; i < semaptr->sem_nsems; i++) {
			error = copyout((caddr_t)&semaptr->sem_base[i].semval,
			    &real_arg.array[i], sizeof(real_arg.array[0]));
			if (error != 0)
				break;
		}
		break;

	case GETZCNT:
		if ((error = ipcperm(cred, &semaptr->sem_perm, IPC_R)))
			return(error);
		if (semnum < 0 || semnum >= semaptr->sem_nsems)
			return(EINVAL);
		rval = semaptr->sem_base[semnum].semzcnt;
		break;

	case SETVAL:
		if ((error = ipcperm(cred, &semaptr->sem_perm, IPC_W)))
			return(error);
		if (semnum < 0 || semnum >= semaptr->sem_nsems)
			return(EINVAL);
		if ((error = copyin(arg, &real_arg, sizeof(real_arg))) != 0)
			return(error);
		semaptr->sem_base[semnum].semval = real_arg.val;
		semundo_clear(semid, semnum);
		wakeup((caddr_t)&sema[semid]);
		break;

	case SETALL:
		if ((error = ipcperm(cred, &semaptr->sem_perm, IPC_W)))
			return(error);
		if ((error = copyin(arg, &real_arg, sizeof(real_arg))) != 0)
			return(error);
		for (i = 0; i < semaptr->sem_nsems; i++) {
			error = copyin(&real_arg.array[i],
			    (caddr_t)&semaptr->sem_base[i].semval,
			    sizeof(real_arg.array[0]));
			if (error != 0)
				break;
		}
		semundo_clear(semid, -1);
		wakeup((caddr_t)&sema[semid]);
		break;

	default:
		return(EINVAL);
	}

	if (error == 0)
		*retval = rval;
	return(error);
}
#endif /* SYSVSEM */
