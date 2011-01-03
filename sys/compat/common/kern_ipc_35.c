/*	$OpenBSD: kern_ipc_35.c,v 1.4 2011/01/03 23:08:07 guenther Exp $	*/

/*
 * Copyright (c) 2004 Todd C. Miller <Todd.Miller@courtesan.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>

#include <sys/mount.h>
#include <sys/syscallargs.h>

/*
 * Old-style (OpenBSD 3.5 and earlier) structures, where ipc_perm
 * used a 16bit int for 'mode'.
 */
struct ipc_perm35 {
	uid_t		cuid;	/* creator user id */
	gid_t		cgid;	/* creator group id */
	uid_t		uid;	/* user id */
	gid_t		gid;	/* group id */
	u_int16_t	mode;	/* r/w permission */
	unsigned short	seq;	/* sequence # (to generate unique id) */
	key_t		key;	/* user specified msg/sem/shm key */
};

struct msqid_ds35 {
	struct ipc_perm35 msg_perm;	/* msg queue permission bits */
	struct msg	  *msg_first;	/* first message in the queue */
	struct msg	  *msg_last;	/* last message in the queue */
	unsigned long	  msg_cbytes;	/* number of bytes in use on queue */
	unsigned long	  msg_qnum;	/* number of msgs in the queue */
	unsigned long	  msg_qbytes;	/* max # of bytes on the queue */
	pid_t		  msg_lspid;	/* pid of last msgsnd() */
	pid_t		  msg_lrpid;	/* pid of last msgrcv() */
	time_t		  msg_stime;	/* time of last msgsnd() */
	long		  msg_pad1;
	time_t		  msg_rtime;	/* time of last msgrcv() */
	long		  msg_pad2;
	time_t		  msg_ctime;	/* time of last msgctl() */
	long		  msg_pad3;
	long		  msg_pad4[4];
};

struct semid_ds35 {
	struct ipc_perm35 sem_perm;	/* operation permission struct */
	struct sem	  *sem_base;	/* pointer to first semaphore in set */
	unsigned short	  sem_nsems;	/* number of sems in set */
	time_t		  sem_otime;	/* last operation time */
	long		  sem_pad1;	/* SVABI/386 says I need this here */
	time_t		  sem_ctime;	/* last change time */
	    				/* Times measured in secs since */
	    				/* 00:00:00 GMT, Jan. 1, 1970 */
	long		  sem_pad2;	/* SVABI/386 says I need this here */
	long		  sem_pad3[4];	/* SVABI/386 says I need this here */
};

struct shmid_ds35 {
	struct ipc_perm35 shm_perm;	/* operation permission structure */
	int		  shm_segsz;	/* size of segment in bytes */
	pid_t		  shm_lpid;	/* process ID of last shm op */
	pid_t		  shm_cpid;	/* process ID of creator */
	shmatt_t	  shm_nattch;	/* number of current attaches */
	time_t		  shm_atime;	/* time of last shmat() */
	time_t		  shm_dtime;	/* time of last shmdt() */
	time_t		  shm_ctime;	/* time of last change by shmctl() */
	void		  *shm_internal;/* implementation specific data */
};

#ifdef SYSVMSG
/*
 * Old-style shmget(2) used int for the size parameter, we now use size_t.
 */
int
compat_35_sys_shmget(struct proc *p, void *v, register_t *retval)
{
	struct compat_35_sys_shmget_args /* {
		syscallarg(key_t) key;
		syscallarg(int) size;
		syscallarg(int) shmflg;
	} */ *uap = v;
	struct sys_shmget_args /* {
		syscallarg(key_t) key;
		syscallarg(size_t) size;
		syscallarg(int) shmflg;
	} */ shmget_args;

	SCARG(&shmget_args, key) = SCARG(uap, key);
	SCARG(&shmget_args, size) = (size_t)SCARG(uap, size);
	SCARG(&shmget_args, shmflg) = SCARG(uap, shmflg);

	return (sys_shmget(p, &shmget_args, retval));
}
#endif

#ifdef SYSVSEM
/*
 * Old-style shmget(2) used u_int for the nsops parameter, we now use size_t.
 */
int
compat_35_sys_semop(struct proc *p, void *v, register_t *retval)
{
	struct compat_35_sys_semop_args /* {
		syscallarg(int) semid;
		syscallarg(struct sembuf *) sops;
		syscallarg(u_int) nsops;
	} */ *uap = v;
	struct sys_semop_args /* {
		syscallarg(int) semid;
		syscallarg(struct sembuf *) sops;
		syscallarg(size_t) nsops;
	} */ semop_args;

	SCARG(&semop_args, semid) = SCARG(uap, semid);
	SCARG(&semop_args, sops) = SCARG(uap, sops);
	SCARG(&semop_args, nsops) = (size_t)SCARG(uap, nsops);

	return (sys_semop(p, &semop_args, retval));
}
#endif

/*
 * Convert between new and old struct {msq,sem,shm}id_ds (both ways)
 */
#if defined(SYSVMSG) || defined(SYSVSEM) || defined(SYSVSHM)
#define cvt_ds(to, from, type, base) do {				\
	(to)->type##_perm.cuid = (from)->type##_perm.cuid;		\
	(to)->type##_perm.cgid = (from)->type##_perm.cgid;		\
	(to)->type##_perm.uid = (from)->type##_perm.uid;		\
	(to)->type##_perm.gid = (from)->type##_perm.gid;		\
	(to)->type##_perm.mode = (from)->type##_perm.mode & 0xffffU;	\
	(to)->type##_perm.seq = (from)->type##_perm.seq;		\
	(to)->type##_perm.key = (from)->type##_perm.key;		\
	bcopy((caddr_t)&(from)->base, (caddr_t)&(to)->base,		\
	    sizeof(*(to)) - ((caddr_t)&(to)->base - (caddr_t)to));	\
} while (0)
#endif /* SYSVMSG || SYSVSEM || SYSVSHM */

#ifdef SYSVMSG
/*
 * Copy a struct msqid_ds35 from userland and convert to struct msqid_ds
 */
static int
msqid_copyin(const void *uaddr, void *kaddr, size_t len)
{
	struct msqid_ds *msqbuf = kaddr;
	struct msqid_ds35 omsqbuf;
	int error;

	if (len != sizeof(struct msqid_ds))
		return (EFAULT);
	if ((error = copyin(uaddr, &omsqbuf, sizeof(omsqbuf))) == 0)
		cvt_ds(msqbuf, &omsqbuf, msg, msg_first);
	return (error);
}

/*
 * Convert a struct msqid_ds to struct msqid_ds35 and copy to userland
 */
static int
msqid_copyout(const void *kaddr, void *uaddr, size_t len)
{
	const struct msqid_ds *msqbuf = kaddr;
	struct msqid_ds35 omsqbuf;

	if (len != sizeof(struct msqid_ds))
		return (EFAULT);
	cvt_ds(&omsqbuf, msqbuf, msg, msg_first);
	return (copyout(&omsqbuf, uaddr, sizeof(omsqbuf)));
}

/*
 * OpenBSD 3.5 msgctl(2) with 16bit mode_t in struct ipcperm.
 */
int
compat_35_sys_msgctl(struct proc *p, void *v, register_t *retval)
{
	struct compat_35_sys_msgctl_args /* {
		syscallarg(int) msqid;
		syscallarg(int) cmd;
		syscallarg(struct msqid_ds35 *) buf;
	} */ *uap = v;

	return (msgctl1(p, SCARG(uap, msqid), SCARG(uap, cmd),
	    (caddr_t)SCARG(uap, buf), msqid_copyin, msqid_copyout));
}
#endif /* SYSVMSG */

#ifdef SYSVSEM
/*
 * Copy a struct semid_ds35 from userland and convert to struct semid_ds
 */
static int
semid_copyin(const void *uaddr, void *kaddr, size_t len)
{
	struct semid_ds *sembuf = kaddr;
	struct semid_ds35 osembuf;
	int error;

	if (len != sizeof(struct semid_ds))
		return (EFAULT);
	if ((error = copyin(uaddr, &osembuf, sizeof(osembuf))) == 0)
		cvt_ds(sembuf, &osembuf, sem, sem_base);
	return (error);
}

/*
 * Convert a struct semid_ds to struct semid_ds35 and copy to userland
 */
static int
semid_copyout(const void *kaddr, void *uaddr, size_t len)
{
	const struct semid_ds *sembuf = kaddr;
	struct semid_ds35 osembuf;

	if (len != sizeof(struct semid_ds))
		return (EFAULT);
	cvt_ds(&osembuf, sembuf, sem, sem_base);
	return (copyout(&osembuf, uaddr, sizeof(osembuf)));
}

/*
 * OpenBSD 3.5 semctl(2) with 16bit mode_t in struct ipcperm.
 */
int
compat_35_sys___semctl(struct proc *p, void *v, register_t *retval)
{
	struct compat_35_sys___semctl_args /* {
		syscallarg(int) semid;
		syscallarg(int) semnum;
		syscallarg(int) cmd;
		syscallarg(union semun *) arg;
	} */ *uap = v;
	union semun arg;
	int error = 0, cmd = SCARG(uap, cmd);

	switch (cmd) {
	case IPC_SET:
	case IPC_STAT:
	case GETALL:
	case SETVAL:
	case SETALL:
		error = copyin(SCARG(uap, arg), &arg, sizeof(arg));
		break;
	}
	if (error == 0) {
		error = semctl1(p, SCARG(uap, semid), SCARG(uap, semnum),
		    cmd, &arg, retval, semid_copyin, semid_copyout);
	}
	return (error);
}
#endif /* SYSVSEM */

#ifdef SYSVSHM
/*
 * Copy a struct shmid_ds35 from userland and convert to struct shmid_ds
 */
static int
shmid_copyin(const void *uaddr, void *kaddr, size_t len)
{
	struct shmid_ds *shmbuf = kaddr;
	struct shmid_ds35 oshmbuf;
	int error;

	if (len != sizeof(struct shmid_ds))
		return (EFAULT);
	if ((error = copyin(uaddr, &oshmbuf, sizeof(oshmbuf))) == 0)
		cvt_ds(shmbuf, &oshmbuf, shm, shm_segsz);
	return (error);
}

/*
 * Convert a struct shmid_ds to struct shmid_ds35 and copy to userland
 */
static int
shmid_copyout(const void *kaddr, void *uaddr, size_t len)
{
	const struct shmid_ds *shmbuf = kaddr;
	struct shmid_ds35 oshmbuf;

	if (len != sizeof(struct shmid_ds))
		return (EFAULT);
	cvt_ds(&oshmbuf, shmbuf, shm, shm_segsz);
	return (copyout(&oshmbuf, uaddr, sizeof(oshmbuf)));
}

/*
 * OpenBSD 3.5 shmctl(2) with 16bit mode_t in struct ipcperm.
 */
int
compat_35_sys_shmctl(struct proc *p, void *v, register_t *retval)
{
	struct compat_35_sys_shmctl_args /* {
		syscallarg(int) shmid;
		syscallarg(int) cmd;
		syscallarg(struct shmid_ds35 *) buf;
	} */ *uap = v;

	return (shmctl1(p, SCARG(uap, shmid), SCARG(uap, cmd),
	    (caddr_t)SCARG(uap, buf), shmid_copyin, shmid_copyout));
}
#endif /* SYSVSHM */
