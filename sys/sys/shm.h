/*	$OpenBSD: shm.h,v 1.16 2004/05/03 17:38:47 millert Exp $	*/
/*	$NetBSD: shm.h,v 1.20 1996/04/09 20:55:35 cgd Exp $	*/

/*
 * Copyright (c) 1994 Adam Glass
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
 *      This product includes software developed by Adam Glass.
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
 * As defined+described in "X/Open System Interfaces and Headers"
 *                         Issue 4, p. XXX
 */

#ifndef _SYS_SHM_H_
#define _SYS_SHM_H_

#ifndef _SYS_IPC_H_
#include <sys/ipc.h>
#endif

#if !defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)

/* shm-specific sysctl variables corresponding to members of struct shminfo */
#define KERN_SHMINFO_SHMMAX	1	/* int: max shm segment size (bytes) */
#define KERN_SHMINFO_SHMMIN	2	/* int: min shm segment size (bytes) */
#define KERN_SHMINFO_SHMMNI	3	/* int: max number of shm identifiers */
#define KERN_SHMINFO_SHMSEG	4	/* int: max shm segments per process */
#define KERN_SHMINFO_SHMALL	5	/* int: max amount of shm (pages) */
#define KERN_SHMINFO_MAXID	6	/* number of valid shared memory ids */

#define CTL_KERN_SHMINFO_NAMES { \
	{ 0, 0 }, \
	{ "shmmax", CTLTYPE_INT }, \
	{ "shmmin", CTLTYPE_INT }, \
	{ "shmmni", CTLTYPE_INT }, \
	{ "shmseg", CTLTYPE_INT }, \
	{ "shmall", CTLTYPE_INT }, \
}

/*
 * Old (deprecated) access mode definitions--do not use.
 * Provided for compatibility with old code only.
 */
#define SHM_R		IPC_R
#define SHM_W		IPC_W

#endif /* !_POSIX_C_SOURCE && !_XOPEN_SOURCE */

/*
 * Shared memory operation flags for shmat(2).
 */
#define	SHM_RDONLY	010000	/* Attach read-only (else read-write) */
#define	SHM_RND		020000	/* Round attach address to SHMLBA */

/*
 * Shared memory specific control commands for shmctl().
 * We accept but ignore these (XXX).
 */
#define	SHM_LOCK	3	/* Lock segment in memory. */
#define	SHM_UNLOCK	4	/* Unlock a segment locked by SHM_LOCK. */

/*
 * Segment low boundry address multiple
 * Use PAGE_SIZE for kernel but for userland query the kernel for the value.
 */
#if defined(_KERNEL) || defined(_STANDALONE) || defined(_LKM)
#define	SHMLBA		PAGE_SIZE
#else
#define	SHMLBA		(getpagesize())
#endif /* _KERNEL || _STANDALONE || _LKM */

typedef short		shmatt_t;

struct shmid_ds {
	struct ipc_perm	shm_perm;	/* operation permission structure */
	int		shm_segsz;	/* size of segment in bytes */
	pid_t		shm_lpid;	/* process ID of last shm op */
	pid_t		shm_cpid;	/* process ID of creator */
	shmatt_t	shm_nattch;	/* number of current attaches */
	time_t		shm_atime;	/* time of last shmat() */
	time_t		shm_dtime;	/* time of last shmdt() */
	time_t		shm_ctime;	/* time of last change by shmctl() */
	void		*shm_internal;	/* implementation specific data */
};

#ifdef _KERNEL
struct oshmid_ds {
	struct oipc_perm shm_perm;	/* operation permission structure */
	int		shm_segsz;	/* size of segment in bytes */
	pid_t		shm_lpid;	/* process ID of last shm op */
	pid_t		shm_cpid;	/* process ID of creator */
	short		shm_nattch;	/* number of current attaches */
	time_t		shm_atime;	/* time of last shmat() */
	time_t		shm_dtime;	/* time of last shmdt() */
	time_t		shm_ctime;	/* time of last change by shmctl() */
	void		*shm_internal;	/* implementation specific data */
};
#endif

#if !defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)
/*
 * System V style catch-all structure for shared memory constants that
 * might be of interest to user programs.  Do we really want/need this?
 */
struct shminfo {
	int	shmmax;		/* max shared memory segment size (bytes) */
	int	shmmin;		/* min shared memory segment size (bytes) */
	int	shmmni;		/* max number of shared memory identifiers */
	int	shmseg;		/* max shared memory segments per process */
	int	shmall;		/* max amount of shared memory (pages) */
};

struct shm_sysctl_info {
	struct	shminfo shminfo;
	struct	shmid_ds shmids[1];
};
#endif /* !_POSIX_C_SOURCE && !_XOPEN_SOURCE */

#ifdef _KERNEL
extern struct shminfo shminfo;
extern struct shmid_ds **shmsegs;

/* initial values for machdep.c */
extern int shmseg;
extern int shmmaxpgs;

struct vmspace;

void shminit(void);
void shmfork(struct vmspace *, struct vmspace *);
void shmexit(struct vmspace *);
void shmid_n2o(struct shmid_ds *, struct oshmid_ds *);
int sysctl_sysvshm(int *, u_int, void *, size_t *, void *, size_t);
int sys_shmat1(struct proc *, void *, register_t *, int);

#else /* !_KERNEL */

#include <sys/cdefs.h>

__BEGIN_DECLS
void *shmat(int, const void *, int);
int shmctl(int, int, struct shmid_ds *);
int shmdt(const void *);
int shmget(key_t, size_t, int);
__END_DECLS

#endif /* !_KERNEL */

#endif /* !_SYS_SHM_H_ */
