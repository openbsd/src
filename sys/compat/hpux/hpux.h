/*	$OpenBSD: hpux.h,v 1.14 2004/09/19 22:03:16 mickey Exp $	*/
/*	$NetBSD: hpux.h,v 1.11 1997/04/01 19:58:58 scottr Exp $	*/

/*
 * Copyright (c) 1995 Jason R. Thorpe.  All rights reserved.
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * from: Utah $Hdr: hpux.h 1.33 93/08/05$
 *
 *	@(#)hpux.h	8.4 (Berkeley) 2/13/94
 */

#include <compat/hpux/hpux_exec.h>

/* HP-UX style UTSNAME struct used by uname syscall */

struct hpux_utsname {
	char	sysname[9];
	char	nodename[9];
	char	release[9];
	char	version[9];
	char	machine[9];
	char	idnumber[15];
};

/* HP-UX style "old" IOCTLs */

struct hpux_sgttyb {
	char	sg_ispeed;
	char	sg_ospeed;
	char	sg_erase;
	char	sg_kill;
	int	sg_flags;	/* only a short in BSD */
};

#define V7_HUPCL	00001
#define V7_XTABS	00002
#define V7_NOAL		04000

#define	HPUXTIOCGETP	_IOR('t', 8, struct hpux_sgttyb)
#define	HPUXTIOCSETP	_IOW('t', 9, struct hpux_sgttyb)

/* 6.5 job control related ioctls which need to be mapped */

#define	HPUXTIOCSLTC	_IOW('T', 23, struct ltchars)
#define	HPUXTIOCGLTC	_IOR('T', 24, struct ltchars)
#define	HPUXTIOCLBIS	_IOW('T', 25, int)
#define	HPUXTIOCLBIC	_IOW('T', 26, int)
#define	HPUXTIOCLSET	_IOW('T', 27, int)
#define	HPUXTIOCLGET	_IOR('T', 28, int)
#	define HPUXLTOSTOP	0000001
#define	HPUXTIOCSPGRP	_IOW('T', 29, int)
#define	HPUXTIOCGPGRP	_IOR('T', 30, int)
#define HPUXTIOCCONS	_IO('t', 104)
#define HPUXTIOCSWINSZ	_IOW('t', 106, struct winsize)
#define HPUXTIOCGWINSZ	_IOR('t', 107, struct winsize)

/* non-blocking IO--doesn't interfere with O_NDELAY */
#define HPUXFIOSNBIO	_IOW('f', 126, int)

/* HP-UX stat structure */

#define bsdtohpuxdev(d)	((major(d) << 24) | minor(d))

struct	hpux_stat {
	int		hst_dev;
	u_int		hst_ino;
	u_short		hst_mode;
	short		hst_nlink;
	u_short		hst_old_uid;	/* these have since moved */
	u_short		hst_old_gid;	/* ... */
	int		hst_rdev;
	int		hst_size;
	int		hst_atime;
	int		hst_spare1;
	int		hst_mtime;
	int		hst_spare2;
	int		hst_ctime;
	int		hst_spare3;
	int		hst_blksize;
	int		hst_blocks;
	u_int		hst_remote;
	int		hst_netdev;
	u_int		hst_netino;
	u_short		hst_cnode;
	u_short		hst_rcnode;
	u_short		hst_netsite;
	short		hst_fstype;
	int		hst_realdev;
	u_short		hst_basemode;
	u_short		hst_spareshort1;
	int		hst_uid;
	int		hst_gid;
	int		hst_spare4[3];
};

struct	hpux_stat64 {
	int		hst_dev;
	u_quad_t	hst_ino;
	u_short		hst_mode;
	short		hst_nlink;
	u_short		hst_old_uid;	/* these have since moved */
	u_short		hst_old_gid;	/* ... */
	int		hst_rdev;
	quad_t		hst_size;
	quad_t		hst_atime;
	int		hst_spare1;
	quad_t		hst_mtime;
	int		hst_spare2;
	quad_t		hst_ctime;
	int		hst_spare3;
	quad_t		hst_blksize;
	quad_t		hst_blocks;
	u_int		hst_remote;
	int		hst_netdev;
	u_quad_t	hst_netino;
	u_short		hst_cnode;
	u_short		hst_rcnode;
	u_short		hst_netsite;
	short		hst_fstype;
	int		hst_realdev;
	u_short		hst_basemode;
	u_short		hst_spareshort1;
	int		hst_uid;
	int		hst_gid;
	int		hst_spare4[3];
};

#define	HST_REMOTE_REMOTE	0x01	/* set if file is remote */
#define	HST_REMOTE_ACL		0x02	/* set if file has ACL entries */
#define	HST_REMOTE_ACLV		0x04	/* set if file has ACL v2 entries */

/* from old timeb.h */
struct hpux_otimeb {
	time_t	time;
	u_short	millitm;
	short	timezone;
	short	dstflag;
};

/* ye ole stat structure */
struct	hpux_ostat {
	u_short	hst_dev;
	u_short	hst_ino;
	u_short hst_mode;
	short	hst_nlink;
	short	hst_uid;
	short	hst_gid;
	u_short	hst_rdev;
	int	hst_size;
	int	hst_atime;
	int	hst_mtime;
	int	hst_ctime;
};

/* HP-UX compat file flags */
#define HPUXNDELAY	00000004
#define HPUXFCREAT	00000400
#define	HPUXFTRUNC	00001000
#define	HPUXFEXCL	00002000
#define HPUXFSYNCIO	00100000
#define HPUXNONBLOCK	00200000
#define HPUXFREMOTE	01000000

/* HP-UX fcntl file locking */
struct hpux_flock {
	short	hl_type;
	short	hl_whence;
	long	hl_start;
	long	hl_len;
	long	hl_pid;
};

#define HPUXF_GETLK	7
#define HPUXF_SETLK	8
#define HPUXF_SETLKW	9

#define HPUXF_RDLCK	1
#define HPUXF_WRLCK	2
#define HPUXF_UNLCK	3

/* HP-UX only sysV shmctl() commands */
#define SHM_LOCK	3	/* Lock segment in core */
#define SHM_UNLOCK	4	/* Unlock segment */

/* SHM stuff reflecting POSIX types */
struct hpux_ipc_perm {
	long	uid;	/* owner's user id */
	long	gid;	/* owner's group id */
	long	cuid;	/* creator's user id */
	long	cgid;	/* creator's group id */
	u_short	mode;	/* access modes */
	u_short	seq;	/* slot usage sequence number */
	long	key;	/* key */
};

struct hpux_shmid_ds {
	struct hpux_ipc_perm shm_perm;	/* operation permission struct */
	int		shm_segsz;	/* segment size (bytes) */
	struct pte	*shm_ptbl;	/* ptr to associated page table */
	long		shm_lpid;	/* pid of last shmop */
	long		shm_cpid;	/* pid of creator */
	u_short		shm_nattch;	/* current # attached */
	u_short		shm_cnattch;	/* in memory # attached */
	time_t		shm_atime;	/* last shmat time */
	time_t		shm_dtime;	/* last shmdt time */
	time_t		shm_ctime;	/* last change time */
	/* actually longer */
};

/* HP-UX rtprio values */
#define RTPRIO_MIN	0
#define RTPRIO_MAX	127
#define RTPRIO_NOCHG	1000
#define RTPRIO_RTOFF	1001

/* HP-UX only sigvec sv_flags values */
#define HPUXSV_RESET	000000004

/*
 * HP-UX returns SIGILL instead of SIGFPE for the CHK and TRAPV exceptions.
 * It also returns different u_code values for certain illegal instruction
 * and floating point exceptions.  Here are the proper HP-UX u_code values
 * (numbers from hpux 6.2 manual pages).
 */

/* SIGILL codes */
#define	HPUX_ILL_ILLINST_TRAP	0	/* T_ILLINST+USER */
#define	HPUX_ILL_CHK_TRAP	6	/* T_CHKINST+USER */
#define	HPUX_ILL_TRAPV_TRAP	7	/* T_TRAPVINST+USER */
#define	HPUX_ILL_PRIV_TRAP	8	/* T_PRIVINST+USER */

/* SIGFPE codes */
#define	HPUX_FPE_INTDIV_TRAP	5	/* T_ZERODIV+USER */

/* HP-UX POSIX signal stuff implementation */
typedef struct __hpux_sigset_t { u_int sigset[8]; } hpux_sigset_t;
struct hpux_sigaction {
	void		(*sa__handler)(int);
	hpux_sigset_t	sa_mask;
	int		sa_flags;
};
#define HPUXSA_ONSTACK		1
#define HPUXSA_RESETHAND	4
#define HPUXSA_NOCLDSTOP	8
#define HPUXSA_NODEFER		32
#define HPUXSA_RESTART		64
#define HPUXSA_NOCLDWAIT	128

#define	HPUXSIG_BLOCK	0	/* block specified signal set */
#define	HPUXSIG_UNBLOCK	1	/* unblock specified signal set */
#define	HPUXSIG_SETMASK	2	/* set specified signal set */

/* sysconf stuff */
#define HPUX_SYSCONF_CLKTICK	2
#define HPUX_SYSCONF_OPENMAX	4
#define HPUX_SYSCONF_CPUTYPE	10001
#define HPUX_SYSCONF_CPUM020	0x20C
#define HPUX_SYSCONF_CPUM030	0x20D
#define HPUX_SYSCONF_CPUM040	0x20E
#define HPUX_SYSCONF_CPUPA10	0x20B
#define HPUX_SYSCONF_CPUPA11	0x210
#define HPUX_SYSCONF_CPUPA20	0x214

/* mmap stuff */
#define HPUXMAP_FIXED	0x04
#define HPUXMAP_REPLACE	0x08
#define HPUXMAP_ANON	0x10

/* rlimit stuff */
#define HPUXRLIMIT_NOFILE	6

/*
 * In BSD EAGAIN and EWOULDBLOCK are the same error code.
 * However, for HP-UX we must split them out to separate codes.
 * The easiest way to do this was to check the return value of
 * BSD routines which are known to return EAGAIN (but never
 * EWOULDBLOCK) and change it to the pseudo-code OEAGAIN when
 * we see it.  The error translation table will them map that
 * code to the HP-UX EAGAIN value.
 */
#define OEAGAIN	82

/*
 * Extensions to the fd_ofileflags flags.
 */
#define	HPUX_UF_NONBLOCK_ON	0x10
#define	HPUX_UF_FNDELAY_ON	0x20
#define	HPUX_UF_FIONBIO_ON	0x40 

#ifdef __m68k__
/*
 * Skeletal 6.X HP-UX user structure info for ptrace() mapping.
 * Yes, this is as bogus as it gets...
 */

/* 6.0/6.2 offsets */
#define ooHU_AROFF	0x004
#define ooHU_TSOFF	0x092
#define ooHU_EDOFF	0x91E
#define ooHU_FPOFF	0xA66

/* 6.5 offsets */
#define oHU_AROFF	0x004
#define oHU_TSOFF	0x0B2
#define oHU_EDOFF	0x93A
#define oHU_FPOFF	0xA86

/* 7.X offsets */
#define HU_AROFF	0x004
#define HU_TSOFF	0x0B4
#define HU_EDOFF	0x8C8
#define HU_FPOFF	0xA28

#define HU_PAD1	(HU_AROFF)
#define HU_PAD2	(HU_TSOFF-HU_AROFF-4)
#define HU_PAD3	(HU_EDOFF-HU_TSOFF-12)
#define HU_PAD4	(HU_FPOFF-HU_EDOFF-sizeof(struct hpux_exec))

struct hpux_user {
	u_char	whocares1[HU_PAD1];	/* +0x000 */
	int	*hpuxu_ar0;		/* +0x004 */
	u_char	whocares2[HU_PAD2];	/* +0x008 */
	int	hpuxu_tsize;		/* +0x0B2 */
	int	hpuxu_dsize;		/* +0x0B6 */
	int	hpuxu_ssize;		/* +0x0BA */
	u_char	whocares3[HU_PAD3];	/* +0x0BE */
	struct	hpux_exec hpuxu_exdata;	/* +0x93A */
	u_char	whocares4[HU_PAD4];	/* +0x95E */
	struct	hpux_fp {		/* +0xA66 */
		int hpfp_save[54];
		int hpfp_ctrl[3];
		int hpfp_reg[24];
	} hpuxu_fp;
	short	hpuxu_dragon;		/* +0xBCA */
 };
#endif
