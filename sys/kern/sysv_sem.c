/*	$OpenBSD: sysv_sem.c,v 1.15 2002/12/30 19:57:34 millert Exp $	*/
/*	$NetBSD: sysv_sem.c,v 1.26 1996/02/09 19:00:25 christos Exp $	*/

/*
 * Copyright (c) 2002 Todd C. Miller <Todd.Miller@courtesan.com>
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
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Implementation of SVID semaphores
 *
 * Author:  Daniel Boulet
 *
 * This software is provided ``AS IS'' without any warranties of any kind.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/sem.h>
#include <sys/sysctl.h>
#include <sys/malloc.h>
#include <sys/pool.h>

#include <sys/mount.h>
#include <sys/syscallargs.h>

/* SVID defines EIDRM but BSD does not */
#ifndef EIDRM
#define EIDRM	EINVAL
#endif

#ifdef SEM_DEBUG
#define DPRINTF(x)	printf x
#else
#define DPRINTF(x)
#endif

int	semtot = 0;
int	semutot = 0;
struct	semid_ds **sema;	/* semaphore id list */
struct	sem_undo *semu_list;	/* list of undo structures */
struct	pool sema_pool;		/* pool for struct semid_ds */
struct	pool semu_pool;		/* pool for struct sem_undo (SEMUSZ) */
unsigned short *semseqs;	/* array of sem sequence numbers */

struct sem_undo *semu_alloc(struct proc *);
int semundo_adjust(struct proc *, struct sem_undo **, int, int, int);
void semundo_clear(int, int);

void
seminit(void)
{

	pool_init(&sema_pool, sizeof(struct semid_ds), 0, 0, 0, "semapl",
	    &pool_allocator_nointr);
	pool_init(&semu_pool, SEMUSZ, 0, 0, 0, "semupl",
	    &pool_allocator_nointr);
	sema = malloc(seminfo.semmni * sizeof(struct semid_ds *),
	    M_SEM, M_WAITOK);
	bzero(sema, seminfo.semmni * sizeof(struct semid_ds *));
	semseqs = malloc(seminfo.semmni * sizeof(unsigned short),
	    M_SEM, M_WAITOK);
	bzero(semseqs, seminfo.semmni * sizeof(unsigned short));
	semu_list = NULL;
}

/*
 * Allocate a new sem_undo structure for a process
 * (returns ptr to structure or NULL if no more room)
 */
struct sem_undo *
semu_alloc(struct proc *p)
{
	struct sem_undo *suptr, *tmp;

	if (semutot == seminfo.semmnu)
		return (NULL);		/* no space */

	/*
	 * Allocate a semu w/o waiting if possible.
	 * If we do have to wait, we must check to verify that a semu
	 * with un_proc == p has not been allocated in the meantime.
	 */
	semutot++;
	if ((suptr = pool_get(&semu_pool, 0)) == NULL) {
		tmp = pool_get(&semu_pool, PR_WAITOK);
		for (suptr = semu_list; suptr != NULL; suptr = suptr->un_next) {
			if (suptr->un_proc == p) {
				pool_put(&semu_pool, tmp);
				semutot--;
				return (suptr);
			}
		}
		suptr = tmp;
	}
	suptr->un_cnt = 0;
	suptr->un_proc = p;
	suptr->un_next = semu_list;
	semu_list = suptr;
	return (suptr);
}

/*
 * Adjust a particular entry for a particular proc
 */
int
semundo_adjust(struct proc *p, struct sem_undo **supptr, int semid, int semnum,
	int adjval)
{
	struct sem_undo *suptr;
	struct undo *sunptr;
	int i;

	/*
	 * Look for and remember the sem_undo if the caller doesn't provide it.
	 */
	suptr = *supptr;
	if (suptr == NULL) {
		for (suptr = semu_list; suptr != NULL; suptr = suptr->un_next) {
			if (suptr->un_proc == p) {
				*supptr = suptr;
				break;
			}
		}
		if (suptr == NULL) {
			if (adjval == 0)
				return (0);
			suptr = semu_alloc(p);
			if (suptr == NULL)
				return (ENOSPC);
			*supptr = suptr;
		}
	}

	/*
	 * Look for the requested entry and adjust it
	 * (delete if adjval becomes 0).
	 */
	sunptr = &suptr->un_ent[0];
	for (i = 0; i < suptr->un_cnt; i++, sunptr++) {
		if (sunptr->un_id != semid || sunptr->un_num != semnum)
			continue;
		if (adjval == 0)
			sunptr->un_adjval = 0;
		else
			sunptr->un_adjval += adjval;
		if (sunptr->un_adjval != 0)
			return (0);

		if (--suptr->un_cnt == 0) {
			struct sem_undo *suprev;

			if (semu_list == suptr)
				semu_list = suptr->un_next;
			else {
				/* this code path should be rare */
				for (suprev = semu_list; suprev != NULL &&
				    suprev->un_next != suptr;
				    suprev = suprev->un_next)
					/* NOTHING */;
#ifdef DIAGNOSTIC
				if (suprev == NULL)
					panic("semundo_adjust: "
					    "suptr not in semu_list");
#endif
				suprev->un_next = suptr->un_next;
			}
			pool_put(&semu_pool, suptr);
			semutot--;
		} else if (i < suptr->un_cnt)
			suptr->un_ent[i] =
			    suptr->un_ent[suptr->un_cnt];
		return (0);
	}

	/* Didn't find the right entry - create it */
	if (adjval == 0)
		return (0);
	if (suptr->un_cnt == SEMUME)
		return (EINVAL);

	sunptr = &suptr->un_ent[suptr->un_cnt];
	suptr->un_cnt++;
	sunptr->un_adjval = adjval;
	sunptr->un_id = semid;
	sunptr->un_num = semnum;
	return (0);
}

void
semundo_clear(int semid, int semnum)
{
	struct sem_undo *suptr, *suprev, *tmp;
	struct undo *sunptr;
	int i;

	for (suptr = semu_list; suptr != NULL; ) {
		sunptr = &suptr->un_ent[0];
		for (i = 0; i < suptr->un_cnt; i++, sunptr++) {
			if (sunptr->un_id == semid) {
				if (semnum == -1 || sunptr->un_num == semnum) {
					suptr->un_cnt--;
					if (i < suptr->un_cnt) {
						suptr->un_ent[i] =
						  suptr->un_ent[suptr->un_cnt];
						i--, sunptr--;
					}
				}
				if (semnum != -1)
					break;
			}
		}
		if (suptr->un_cnt == 0) {
			tmp = suptr;
			if (suptr == semu_list)
				suptr = semu_list = suptr->un_next;
			else
				suptr = suprev->un_next = suptr->un_next;
			pool_put(&semu_pool, tmp);
			semutot--;
		} else {
			suprev = suptr;
			suptr = suptr->un_next;
		}
	}
}

void
semid_n2o(struct semid_ds *n, struct osemid_ds *o)
{
	o->sem_base = n->sem_base;
	o->sem_nsems = n->sem_nsems;
	o->sem_otime = n->sem_otime;
	o->sem_pad1 = n->sem_pad1;
	o->sem_ctime = n->sem_ctime;
	o->sem_pad2 = n->sem_pad2;
	bcopy(n->sem_pad3, o->sem_pad3, sizeof o->sem_pad3);
	ipc_n2o(&n->sem_perm, &o->sem_perm);
}

int
sys___semctl(struct proc *p, void *v, register_t *retval)
{
	struct sys___semctl_args /* {
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
	struct semid_ds sbuf;
	struct semid_ds *semaptr;

	DPRINTF(("call to semctl(%d, %d, %d, %p)\n", semid, semnum, cmd, arg));

	semid = IPCID_TO_IX(semid);
	if (semid < 0 || semid >= seminfo.semmsl)
		return (EINVAL);

	if ((semaptr = sema[semid]) == NULL ||
	    semaptr->sem_perm.seq != IPCID_TO_SEQ(SCARG(uap, semid)))
		return (EINVAL);

	error = rval = 0;

	switch (cmd) {
	case IPC_RMID:
		if ((error = ipcperm(cred, &semaptr->sem_perm, IPC_M)) != 0)
			return (error);
		semaptr->sem_perm.cuid = cred->cr_uid;
		semaptr->sem_perm.uid = cred->cr_uid;
		semtot -= semaptr->sem_nsems;
		free(semaptr->sem_base, M_SEM);
		pool_put(&sema_pool, semaptr);
		sema[semid] = NULL;
		semundo_clear(semid, -1);
		wakeup((caddr_t)&sema[semid]);
		break;

	case IPC_SET:
		if ((error = ipcperm(cred, &semaptr->sem_perm, IPC_M)))
			return (error);
		if ((error = copyin(arg, &real_arg, sizeof(real_arg))) != 0)
			return (error);
		if ((error = copyin(real_arg.buf, (caddr_t)&sbuf,
		    sizeof(sbuf))) != 0)
			return (error);
		semaptr->sem_perm.uid = sbuf.sem_perm.uid;
		semaptr->sem_perm.gid = sbuf.sem_perm.gid;
		semaptr->sem_perm.mode = (semaptr->sem_perm.mode & ~0777) |
		    (sbuf.sem_perm.mode & 0777);
		semaptr->sem_ctime = time.tv_sec;
		break;

	case IPC_STAT:
		if ((error = ipcperm(cred, &semaptr->sem_perm, IPC_R)))
			return (error);
		if ((error = copyin(arg, &real_arg, sizeof(real_arg))) != 0)
			return (error);
		error = copyout((caddr_t)semaptr, real_arg.buf,
		    sizeof(struct semid_ds));
		break;

	case GETNCNT:
		if ((error = ipcperm(cred, &semaptr->sem_perm, IPC_R)))
			return (error);
		if (semnum < 0 || semnum >= semaptr->sem_nsems)
			return (EINVAL);
		rval = semaptr->sem_base[semnum].semncnt;
		break;

	case GETPID:
		if ((error = ipcperm(cred, &semaptr->sem_perm, IPC_R)))
			return (error);
		if (semnum < 0 || semnum >= semaptr->sem_nsems)
			return (EINVAL);
		rval = semaptr->sem_base[semnum].sempid;
		break;

	case GETVAL:
		if ((error = ipcperm(cred, &semaptr->sem_perm, IPC_R)))
			return (error);
		if (semnum < 0 || semnum >= semaptr->sem_nsems)
			return (EINVAL);
		rval = semaptr->sem_base[semnum].semval;
		break;

	case GETALL:
		if ((error = ipcperm(cred, &semaptr->sem_perm, IPC_R)))
			return (error);
		if ((error = copyin(arg, &real_arg, sizeof(real_arg))) != 0)
			return (error);
		for (i = 0; i < semaptr->sem_nsems; i++) {
			error = copyout((caddr_t)&semaptr->sem_base[i].semval,
			    &real_arg.array[i], sizeof(real_arg.array[0]));
			if (error != 0)
				break;
		}
		break;

	case GETZCNT:
		if ((error = ipcperm(cred, &semaptr->sem_perm, IPC_R)))
			return (error);
		if (semnum < 0 || semnum >= semaptr->sem_nsems)
			return (EINVAL);
		rval = semaptr->sem_base[semnum].semzcnt;
		break;

	case SETVAL:
		if ((error = ipcperm(cred, &semaptr->sem_perm, IPC_W)))
			return (error);
		if (semnum < 0 || semnum >= semaptr->sem_nsems)
			return (EINVAL);
		if ((error = copyin(arg, &real_arg, sizeof(real_arg))) != 0)
			return (error);
		semaptr->sem_base[semnum].semval = real_arg.val;
		semundo_clear(semid, semnum);
		wakeup((caddr_t)&sema[semid]);
		break;

	case SETALL:
		if ((error = ipcperm(cred, &semaptr->sem_perm, IPC_W)))
			return (error);
		if ((error = copyin(arg, &real_arg, sizeof(real_arg))) != 0)
			return (error);
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
		return (EINVAL);
	}

	if (error == 0)
		*retval = rval;
	return (error);
}

int
sys_semget(struct proc *p, void *v, register_t *retval)
{
	struct sys_semget_args /* {
		syscallarg(key_t) key;
		syscallarg(int) nsems;
		syscallarg(int) semflg;
	} */ *uap = v;
	int semid, error;
	int key = SCARG(uap, key);
	int nsems = SCARG(uap, nsems);
	int semflg = SCARG(uap, semflg);
	struct semid_ds *semaptr, *semaptr_new = NULL;
	struct ucred *cred = p->p_ucred;

	DPRINTF(("semget(0x%x, %d, 0%o)\n", key, nsems, semflg));

	/*
	 * Preallocate space for the new semaphore.  If we are going
	 * to sleep, we want to sleep now to elliminate any race
	 * condition in allocating a semaphore with a specific key.
	 */
	if (key == IPC_PRIVATE || (semflg & IPC_CREAT)) {
		semaptr_new = pool_get(&sema_pool, PR_WAITOK);
		semaptr_new->sem_base = malloc(nsems * sizeof(struct sem),
		    M_SEM, M_WAITOK);
		bzero(semaptr_new->sem_base, nsems * sizeof(struct sem));
	}

	if (key != IPC_PRIVATE) {
		for (semid = 0, semaptr = NULL; semid < seminfo.semmni; semid++) {
			if ((semaptr = sema[semid]) != NULL &&
			    semaptr->sem_perm.key == key)
				break;
		}
		if (semaptr != NULL) {
			DPRINTF(("found public key\n"));
			if ((error = ipcperm(cred, &semaptr->sem_perm,
			    semflg & 0700)))
				goto error;
			if (nsems > 0 && semaptr->sem_nsems < nsems) {
				DPRINTF(("too small\n"));
				error = EINVAL;
				goto error;
			}
			if ((semflg & IPC_CREAT) && (semflg & IPC_EXCL)) {
				DPRINTF(("not exclusive\n"));
				error = EEXIST;
				goto error;
			}
			goto found;
		}
	}

	DPRINTF(("need to allocate the semid_ds\n"));
	if (key == IPC_PRIVATE || (semflg & IPC_CREAT)) {
		if (nsems <= 0 || nsems > seminfo.semmsl) {
			DPRINTF(("nsems out of range (0<%d<=%d)\n", nsems,
			    seminfo.semmsl));
			error = EINVAL;
			goto error;
		}
		if (nsems > seminfo.semmns - semtot) {
			DPRINTF(("not enough semaphores left (need %d, got %d)\n",
			    nsems, seminfo.semmns - semtot));
			error = ENOSPC;
			goto error;
		}
		for (semid = 0; semid < seminfo.semmni; semid++) {
			if ((semaptr = sema[semid]) == NULL)
				break;
		}
		if (semid == seminfo.semmni) {
			DPRINTF(("no more semid_ds's available\n"));
			error = ENOSPC;
			goto error;
		}
		DPRINTF(("semid %d is available\n", semid));
		semaptr_new->sem_perm.key = key;
		semaptr_new->sem_perm.cuid = cred->cr_uid;
		semaptr_new->sem_perm.uid = cred->cr_uid;
		semaptr_new->sem_perm.cgid = cred->cr_gid;
		semaptr_new->sem_perm.gid = cred->cr_gid;
		semaptr_new->sem_perm.mode = (semflg & 0777);
		semaptr_new->sem_perm.seq = semseqs[semid] =
		    (semseqs[semid] + 1) & 0x7fff;
		semaptr_new->sem_nsems = nsems;
		semaptr_new->sem_otime = 0;
		semaptr_new->sem_ctime = time.tv_sec;
		sema[semid] = semaptr_new;
		semtot += nsems;
	} else {
		DPRINTF(("didn't find it and wasn't asked to create it\n"));
		return (ENOENT);
	}

found:
	*retval = IXSEQ_TO_IPCID(semid, sema[semid]->sem_perm);
	return (0);
error:
	if (semaptr_new != NULL) {
		free(semaptr_new->sem_base, M_SEM);
		pool_put(&sema_pool, semaptr_new);
	}
	return (error);
}

int
sys_semop(struct proc *p, void *v, register_t *retval)
{
	struct sys_semop_args /* {
		syscallarg(int) semid;
		syscallarg(struct sembuf *) sops;
		syscallarg(u_int) nsops;
	} */ *uap = v;
	int semid = SCARG(uap, semid);
	u_int nsops = SCARG(uap, nsops);
	struct sembuf *sops;
	struct semid_ds *semaptr;
	struct sembuf *sopptr = NULL;
	struct sem *semptr = NULL;
	struct sem_undo *suptr = NULL;
	struct ucred *cred = p->p_ucred;
	int i, j, error;
	int do_wakeup, do_undos;

	DPRINTF(("call to semop(%d, %p, %d)\n", semid, sops, nsops));

	semid = IPCID_TO_IX(semid);	/* Convert back to zero origin */

	if (semid < 0 || semid >= seminfo.semmsl)
		return (EINVAL);

	if ((semaptr = sema[semid]) == NULL ||
	    semaptr->sem_perm.seq != IPCID_TO_SEQ(SCARG(uap, semid)))
		return (EINVAL);

	if ((error = ipcperm(cred, &semaptr->sem_perm, IPC_W))) {
		DPRINTF(("error = %d from ipaccess\n", error));
		return (error);
	}

	if (nsops > seminfo.semopm) {
		DPRINTF(("too many sops (max=%d, nsops=%d)\n", seminfo.semopm, nsops));
		return (E2BIG);
	}

	sops = malloc(nsops * sizeof(struct sembuf), M_SEM, M_WAITOK);
	error = copyin(SCARG(uap, sops), sops, nsops * sizeof(struct sembuf));
	if (error != 0) {
		DPRINTF(("error = %d from copyin(%p, %p, %d)\n", error,
		    SCARG(uap, sops), &sops, nsops * sizeof(struct sembuf)));
		free(sops, M_SEM);
		return (error);
	}

	/* 
	 * Loop trying to satisfy the vector of requests.
	 * If we reach a point where we must wait, any requests already
	 * performed are rolled back and we go to sleep until some other
	 * process wakes us up.  At this point, we start all over again.
	 *
	 * This ensures that from the perspective of other tasks, a set
	 * of requests is atomic (never partially satisfied).
	 */
	do_undos = 0;

	for (;;) {
		do_wakeup = 0;

		for (i = 0; i < nsops; i++) {
			sopptr = &sops[i];

			if (sopptr->sem_num >= semaptr->sem_nsems) {
				free(sops, M_SEM);
				return (EFBIG);
			}

			semptr = &semaptr->sem_base[sopptr->sem_num];

			DPRINTF(("semop:  semaptr=%x, sem_base=%x, semptr=%x, sem[%d]=%d : op=%d, flag=%s\n",
			    semaptr, semaptr->sem_base, semptr,
			    sopptr->sem_num, semptr->semval, sopptr->sem_op,
			    (sopptr->sem_flg & IPC_NOWAIT) ? "nowait" : "wait"));

			if (sopptr->sem_op < 0) {
				if ((int)(semptr->semval +
					  sopptr->sem_op) < 0) {
					DPRINTF(("semop:  can't do it now\n"));
					break;
				} else {
					semptr->semval += sopptr->sem_op;
					if (semptr->semval == 0 &&
					    semptr->semzcnt > 0)
						do_wakeup = 1;
				}
				if (sopptr->sem_flg & SEM_UNDO)
					do_undos = 1;
			} else if (sopptr->sem_op == 0) {
				if (semptr->semval > 0) {
					DPRINTF(("semop:  not zero now\n"));
					break;
				}
			} else {
				if (semptr->semncnt > 0)
					do_wakeup = 1;
				semptr->semval += sopptr->sem_op;
				if (sopptr->sem_flg & SEM_UNDO)
					do_undos = 1;
			}
		}

		/*
		 * Did we get through the entire vector?
		 */
		if (i >= nsops)
			goto done;

		/*
		 * No ... rollback anything that we've already done
		 */
		DPRINTF(("semop:  rollback 0 through %d\n", i - 1));
		for (j = 0; j < i; j++)
			semaptr->sem_base[sops[j].sem_num].semval -=
			    sops[j].sem_op;

		/*
		 * If the request that we couldn't satisfy has the
		 * NOWAIT flag set then return with EAGAIN.
		 */
		if (sopptr->sem_flg & IPC_NOWAIT) {
			free(sops, M_SEM);
			return (EAGAIN);
		}

		if (sopptr->sem_op == 0)
			semptr->semzcnt++;
		else
			semptr->semncnt++;

		DPRINTF(("semop:  good night!\n"));
		error = tsleep((caddr_t)&sema[semid], PLOCK | PCATCH,
		    "semwait", 0);
		DPRINTF(("semop:  good morning (error=%d)!\n", error));

		suptr = NULL;	/* sem_undo may have been reallocated */

		if (error != 0) {
			free(sops, M_SEM);
			return (EINTR);
		}
		DPRINTF(("semop:  good morning!\n"));

		/*
		 * Make sure that the semaphore still exists
		 */
		if (sema[semid] == NULL ||
		    semaptr->sem_perm.seq != IPCID_TO_SEQ(SCARG(uap, semid))) {
			free(sops, M_SEM);
			return (EIDRM);
		}

		/*
		 * The semaphore is still alive.  Readjust the count of
		 * waiting processes.
		 */
		if (sopptr->sem_op == 0)
			semptr->semzcnt--;
		else
			semptr->semncnt--;
	}

done:
	/*
	 * Process any SEM_UNDO requests.
	 */
	if (do_undos) {
		for (i = 0; i < nsops; i++) {
			/*
			 * We only need to deal with SEM_UNDO's for non-zero
			 * op's.
			 */
			int adjval;

			if ((sops[i].sem_flg & SEM_UNDO) == 0)
				continue;
			adjval = sops[i].sem_op;
			if (adjval == 0)
				continue;
			error = semundo_adjust(p, &suptr, semid,
			    sops[i].sem_num, -adjval);
			if (error == 0)
				continue;

			/*
			 * Uh-Oh!  We ran out of either sem_undo's or undo's.
			 * Rollback the adjustments to this point and then
			 * rollback the semaphore ups and down so we can return
			 * with an error with all structures restored.  We
			 * rollback the undo's in the exact reverse order that
			 * we applied them.  This guarantees that we won't run
			 * out of space as we roll things back out.
			 */
			for (j = i - 1; j >= 0; j--) {
				if ((sops[j].sem_flg & SEM_UNDO) == 0)
					continue;
				adjval = sops[j].sem_op;
				if (adjval == 0)
					continue;
				if (semundo_adjust(p, &suptr, semid,
				    sops[j].sem_num, adjval) != 0)
					panic("semop - can't undo undos");
			}

			for (j = 0; j < nsops; j++)
				semaptr->sem_base[sops[j].sem_num].semval -=
				    sops[j].sem_op;

			DPRINTF(("error = %d from semundo_adjust\n", error));
			free(sops, M_SEM);
			return (error);
		} /* loop through the sops */
	} /* if (do_undos) */

	/* We're definitely done - set the sempid's */
	for (i = 0; i < nsops; i++) {
		sopptr = &sops[i];
		semptr = &semaptr->sem_base[sopptr->sem_num];
		semptr->sempid = p->p_pid;
	}
	free(sops, M_SEM);

	/* Do a wakeup if any semaphore was up'd. */
	if (do_wakeup) {
		DPRINTF(("semop:  doing wakeup\n"));
		wakeup((caddr_t)&sema[semid]);
		DPRINTF(("semop:  back from wakeup\n"));
	}
	DPRINTF(("semop:  done\n"));
	*retval = 0;
	return (0);
}

/*
 * Go through the undo structures for this process and apply the adjustments to
 * semaphores.
 */
void
semexit(struct proc *p)
{
	struct sem_undo *suptr;
	struct sem_undo **supptr;

	/*
	 * Go through the chain of undo vectors looking for one associated with
	 * this process.
	 */
	for (supptr = &semu_list; (suptr = *supptr) != NULL;
	    supptr = &suptr->un_next) {
		if (suptr->un_proc == p)
			break;
	}

	/*
	 * No (i.e. we are in case 1 or 2).
	 *
	 * If there is no undo vector, skip to the end and unlock the
	 * semaphore facility if necessary.
	 */
	if (suptr == NULL)
		return;

	/*
	 * We are now in case 1 or 2, and we have an undo vector for this
	 * process.
	 */
	DPRINTF(("proc @%p has undo structure with %d entries\n", p,
	    suptr->un_cnt));

	/*
	 * If there are any active undo elements then process them.
	 */
	if (suptr->un_cnt > 0) {
		int ix;

		for (ix = 0; ix < suptr->un_cnt; ix++) {
			int semid = suptr->un_ent[ix].un_id;
			int semnum = suptr->un_ent[ix].un_num;
			int adjval = suptr->un_ent[ix].un_adjval;
			struct semid_ds *semaptr;

			if ((semaptr = sema[semid]) == NULL)
				panic("semexit - semid not allocated");
			if (semnum >= semaptr->sem_nsems)
				panic("semexit - semnum out of range");

			DPRINTF(("semexit:  %p id=%d num=%d(adj=%d) ; sem=%d\n",
			    suptr->un_proc, suptr->un_ent[ix].un_id,
			    suptr->un_ent[ix].un_num,
			    suptr->un_ent[ix].un_adjval,
			    semaptr->sem_base[semnum].semval));

			if (adjval < 0 &&
			    semaptr->sem_base[semnum].semval < -adjval)
				semaptr->sem_base[semnum].semval = 0;
			else
				semaptr->sem_base[semnum].semval += adjval;

			wakeup((caddr_t)&sema[semid]);
			DPRINTF(("semexit:  back from wakeup\n"));
		}
	}

	/*
	 * Deallocate the undo vector.
	 */
	DPRINTF(("removing vector\n"));
	*supptr = suptr->un_next;
	pool_put(&semu_pool, suptr);
	semutot--;
}

/*
 * Userland access to struct seminfo.
 */
int
sysctl_sysvsem(int *name, u_int namelen, void *oldp, size_t *oldlenp,
	void *newp, size_t newlen)
{
	int error, val;
	struct semid_ds **sema_new;
	unsigned short *newseqs;

	if (namelen != 2) {
		switch (name[0]) {
		case KERN_SEMINFO_SEMMNI:
		case KERN_SEMINFO_SEMMNS:
		case KERN_SEMINFO_SEMMNU:
		case KERN_SEMINFO_SEMMSL:
		case KERN_SEMINFO_SEMOPM:
		case KERN_SEMINFO_SEMUME:
		case KERN_SEMINFO_SEMUSZ:
		case KERN_SEMINFO_SEMVMX:
		case KERN_SEMINFO_SEMAEM:
			break;
		default:
                        return (ENOTDIR);       /* overloaded */
                }
        }

	switch (name[0]) {
	case KERN_SEMINFO_SEMMNI:
		val = seminfo.semmni;
		if ((error = sysctl_int(oldp, oldlenp, newp, newlen, &val)) ||
		    val == seminfo.semmni)
			return (error);

		if (val < seminfo.semmni)
			return (EINVAL);	/* can't decrease semmni */

		/* Expand semsegs and semseqs arrays */
		sema_new = malloc(val * sizeof(struct semid_ds *),
		    M_SEM, M_WAITOK);
		bcopy(sema, sema_new,
		    seminfo.semmni * sizeof(struct semid_ds *));
		bzero(sema_new + seminfo.semmni,
		    (val - seminfo.semmni) * sizeof(struct semid_ds *));
		newseqs = malloc(val * sizeof(unsigned short), M_SEM, M_WAITOK);
		bcopy(semseqs, newseqs,
		    seminfo.semmni * sizeof(unsigned short));
		bzero(newseqs + seminfo.semmni,
		    (val - seminfo.semmni) * sizeof(unsigned short));
		free(sema, M_SEM);
		free(semseqs, M_SEM);
		sema = sema_new;
		semseqs = newseqs;
		seminfo.semmni = val;
		return (0);
	case KERN_SEMINFO_SEMMNS:
		val = seminfo.semmns;
		if ((error = sysctl_int(oldp, oldlenp, newp, newlen, &val)) ||
		    val == seminfo.semmns)
			return (error);
		if (val < seminfo.semmns)
			return (EINVAL);	/* can't decrease semmns */
		seminfo.semmns = val;
		return (0);
	case KERN_SEMINFO_SEMMNU:
		val = seminfo.semmnu;
		if ((error = sysctl_int(oldp, oldlenp, newp, newlen, &val)) ||
		    val == seminfo.semmnu)
			return (error);
		if (val < seminfo.semmnu)
			return (EINVAL);	/* can't decrease semmnu */
		seminfo.semmnu = val;
		return (0);
	case KERN_SEMINFO_SEMMSL:
		val = seminfo.semmsl;
		if ((error = sysctl_int(oldp, oldlenp, newp, newlen, &val)) ||
		    val == seminfo.semmsl)
			return (error);
		if (val < seminfo.semmsl)
			return (EINVAL);	/* can't decrease semmsl */
		seminfo.semmsl = val;
		return (0);
	case KERN_SEMINFO_SEMOPM:
		val = seminfo.semopm;
		if ((error = sysctl_int(oldp, oldlenp, newp, newlen, &val)) ||
		    val == seminfo.semopm)
			return (error);
		if (val <= 0)
			return (EINVAL);	/* semopm must be >= 1 */
		seminfo.semopm = val;
		return (0);
	case KERN_SEMINFO_SEMUME:
		return (sysctl_rdint(oldp, oldlenp, newp, seminfo.semume));
	case KERN_SEMINFO_SEMUSZ:
		return (sysctl_rdint(oldp, oldlenp, newp, seminfo.semusz));
	case KERN_SEMINFO_SEMVMX:
		return (sysctl_rdint(oldp, oldlenp, newp, seminfo.semvmx));
	case KERN_SEMINFO_SEMAEM:
		return (sysctl_rdint(oldp, oldlenp, newp, seminfo.semaem));
	default:
		return (EOPNOTSUPP);
	}
	/* NOTREACHED */
}
