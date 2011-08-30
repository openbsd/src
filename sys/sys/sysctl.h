/*	$OpenBSD: sysctl.h,v 1.117 2011/08/30 01:09:29 guenther Exp $	*/
/*	$NetBSD: sysctl.h,v 1.16 1996/04/09 20:55:36 cgd Exp $	*/

/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Mike Karels at Berkeley Software Design, Inc.
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
 *	@(#)sysctl.h	8.2 (Berkeley) 3/30/95
 */

#ifndef _SYS_SYSCTL_H_
#define	_SYS_SYSCTL_H_

/*
 * These are for the eproc structure defined below.
 */
#ifndef _KERNEL
#include <sys/proc.h>		/* for SRUN, SIDL, etc */
#include <sys/resource.h>
#endif

#include <sys/resourcevar.h>	/* XXX */

#include <uvm/uvm_extern.h>

/*
 * Definitions for sysctl call.  The sysctl call uses a hierarchical name
 * for objects that can be examined or modified.  The name is expressed as
 * a sequence of integers.  Like a file path name, the meaning of each
 * component depends on its place in the hierarchy.  The top-level and kern
 * identifiers are defined here, and other identifiers are defined in the
 * respective subsystem header files.
 */

#define	CTL_MAXNAME	12	/* largest number of components supported */

/*
 * Each subsystem defined by sysctl defines a list of variables
 * for that subsystem. Each name is either a node with further
 * levels defined below it, or it is a leaf of some particular
 * type given below. Each sysctl level defines a set of name/type
 * pairs to be used by sysctl(1) in manipulating the subsystem.
 */
struct ctlname {
	char	*ctl_name;	/* subsystem name */
	int	ctl_type;	/* type of name */
};
#define	CTLTYPE_NODE	1	/* name is a node */
#define	CTLTYPE_INT	2	/* name describes an integer */
#define	CTLTYPE_STRING	3	/* name describes a string */
#define	CTLTYPE_QUAD	4	/* name describes a 64-bit number */
#define	CTLTYPE_STRUCT	5	/* name describes a structure */

/*
 * Top-level identifiers
 */
#define	CTL_UNSPEC	0		/* unused */
#define	CTL_KERN	1		/* "high kernel": proc, limits */
#define	CTL_VM		2		/* virtual memory */
#define	CTL_FS		3		/* file system, mount type is next */
#define	CTL_NET		4		/* network, see socket.h */
#define	CTL_DEBUG	5		/* debugging parameters */
#define	CTL_HW		6		/* generic cpu/io */
#define	CTL_MACHDEP	7		/* machine dependent */
#define	CTL_USER	8		/* user-level */
#define	CTL_DDB		9		/* DDB user interface, see db_var.h */
#define	CTL_VFS		10		/* VFS sysctl's */
#define	CTL_MAXID	11		/* number of valid top-level ids */

#define	CTL_NAMES { \
	{ 0, 0 }, \
	{ "kern", CTLTYPE_NODE }, \
	{ "vm", CTLTYPE_NODE }, \
	{ "fs", CTLTYPE_NODE }, \
	{ "net", CTLTYPE_NODE }, \
	{ "debug", CTLTYPE_NODE }, \
	{ "hw", CTLTYPE_NODE }, \
	{ "machdep", CTLTYPE_NODE }, \
	{ "user", CTLTYPE_NODE }, \
	{ "ddb", CTLTYPE_NODE }, \
	{ "vfs", CTLTYPE_NODE }, \
}

/*
 * CTL_KERN identifiers
 */
#define	KERN_OSTYPE	 	 1	/* string: system version */
#define	KERN_OSRELEASE	 	 2	/* string: system release */
#define	KERN_OSREV	 	 3	/* int: system revision */
#define	KERN_VERSION	 	 4	/* string: compile time info */
#define	KERN_MAXVNODES	 	 5	/* int: max vnodes */
#define	KERN_MAXPROC	 	 6	/* int: max processes */
#define	KERN_MAXFILES	 	 7	/* int: max open files */
#define	KERN_ARGMAX	 	 8	/* int: max arguments to exec */
#define	KERN_SECURELVL	 	 9	/* int: system security level */
#define	KERN_HOSTNAME		10	/* string: hostname */
#define	KERN_HOSTID		11	/* int: host identifier */
#define	KERN_CLOCKRATE		12	/* struct: struct clockinfo */
#define	KERN_VNODE		13	/* struct: vnode structures */
/*define gap: was KERN_PROC	14	*/
#define	KERN_FILE		15	/* struct: file entries */
#define	KERN_PROF		16	/* node: kernel profiling info */
#define	KERN_POSIX1		17	/* int: POSIX.1 version */
#define	KERN_NGROUPS		18	/* int: # of supplemental group ids */
#define	KERN_JOB_CONTROL	19	/* int: is job control available */
#define	KERN_SAVED_IDS		20	/* int: saved set-user/group-ID */
#define	KERN_BOOTTIME		21	/* struct: time kernel was booted */
#define	KERN_DOMAINNAME		22	/* string: (YP) domainname */
#define	KERN_MAXPARTITIONS	23	/* int: number of partitions/disk */
#define	KERN_RAWPARTITION	24	/* int: raw partition number */
/*define gap			25	*/
/*define gap			26	*/
#define	KERN_OSVERSION		27	/* string: kernel build version */
#define	KERN_SOMAXCONN		28	/* int: listen queue maximum */
#define	KERN_SOMINCONN		29	/* int: half-open controllable param */
#define	KERN_USERMOUNT		30	/* int: users may mount filesystems */
#define	KERN_RND		31	/* struct: rnd(4) statistics */
#define	KERN_NOSUIDCOREDUMP	32	/* int: no setuid coredumps ever */ 
#define	KERN_FSYNC		33	/* int: file synchronization support */
#define	KERN_SYSVMSG		34	/* int: SysV message queue suppoprt */
#define	KERN_SYSVSEM		35	/* int: SysV semaphore support */
#define	KERN_SYSVSHM		36	/* int: SysV shared memory support */
#define	KERN_ARND		37	/* int: random integer from arc4rnd */
#define	KERN_MSGBUFSIZE		38	/* int: size of message buffer */
#define KERN_MALLOCSTATS	39	/* node: malloc statistics */
#define KERN_CPTIME		40	/* array: cp_time */
#define KERN_NCHSTATS		41	/* struct: vfs cache statistics */
#define KERN_FORKSTAT		42	/* struct: fork statistics */
#define KERN_NSELCOLL		43	/* int: select(2) collisions */
#define KERN_TTY		44	/* node: tty information */
#define	KERN_CCPU		45	/* int: ccpu */
#define	KERN_FSCALE		46	/* int: fscale */
#define	KERN_NPROCS		47	/* int: number of processes */
#define	KERN_MSGBUF		48	/* message buffer, KERN_MSGBUFSIZE */
#define	KERN_POOL		49	/* struct: pool information */
#define	KERN_STACKGAPRANDOM	50	/* int: stackgap_random */
#define	KERN_SYSVIPC_INFO	51	/* struct: SysV sem/shm/msg info */
#define KERN_USERCRYPTO		52	/* int: usercrypto */
#define KERN_CRYPTODEVALLOWSOFT	53	/* int: cryptodevallowsoft */
#define KERN_SPLASSERT		54	/* int: splassert */
#define KERN_PROC_ARGS		55	/* node: proc args and env */
#define	KERN_NFILES		56	/* int: number of open files */
#define	KERN_TTYCOUNT		57	/* int: number of tty devices */
#define KERN_NUMVNODES		58	/* int: number of vnodes in use */
#define	KERN_MBSTAT		59	/* struct: mbuf statistics */
#define KERN_USERASYMCRYPTO	60	/* int: usercrypto */
#define	KERN_SEMINFO		61	/* struct: SysV struct seminfo */
#define	KERN_SHMINFO		62	/* struct: SysV struct shminfo */
#define KERN_INTRCNT		63	/* node: interrupt counters */
#define	KERN_WATCHDOG		64	/* node: watchdog */
#define	KERN_EMUL		65	/* node: emuls */
#define	KERN_PROC		66	/* struct: process entries */
#define	KERN_PROC2		KERN_PROC	/* backwards compat name */
#define	KERN_MAXCLUSTERS	67	/* number of mclusters */
#define KERN_EVCOUNT		68	/* node: event counters */
#define	KERN_TIMECOUNTER	69	/* node: timecounter */
#define	KERN_MAXLOCKSPERUID	70	/* int: locks per uid */
#define	KERN_CPTIME2		71	/* array: cp_time2 */
#define	KERN_CACHEPCT		72	/* buffer cache % of physmem */
#define	KERN_FILE2		73	/* struct: file entries */
#define	KERN_RTHREADS		74	/* kernel rthreads support enabled */
#define	KERN_CONSDEV		75	/* dev_t: console terminal device */
#define	KERN_NETLIVELOCKS	76	/* int: number of network livelocks */
#define	KERN_POOL_DEBUG		77	/* int: enable pool_debug */
#define	KERN_MAXID		78	/* number of valid kern ids */

#define	CTL_KERN_NAMES { \
	{ 0, 0 }, \
	{ "ostype", CTLTYPE_STRING }, \
	{ "osrelease", CTLTYPE_STRING }, \
	{ "osrevision", CTLTYPE_INT }, \
	{ "version", CTLTYPE_STRING }, \
	{ "maxvnodes", CTLTYPE_INT }, \
	{ "maxproc", CTLTYPE_INT }, \
	{ "maxfiles", CTLTYPE_INT }, \
	{ "argmax", CTLTYPE_INT }, \
	{ "securelevel", CTLTYPE_INT }, \
	{ "hostname", CTLTYPE_STRING }, \
	{ "hostid", CTLTYPE_INT }, \
	{ "clockrate", CTLTYPE_STRUCT }, \
	{ "vnode", CTLTYPE_STRUCT }, \
	{ "gap", 0 }, \
	{ "file", CTLTYPE_STRUCT }, \
	{ "profiling", CTLTYPE_NODE }, \
	{ "posix1version", CTLTYPE_INT }, \
	{ "ngroups", CTLTYPE_INT }, \
	{ "job_control", CTLTYPE_INT }, \
	{ "saved_ids", CTLTYPE_INT }, \
	{ "boottime", CTLTYPE_STRUCT }, \
	{ "domainname", CTLTYPE_STRING }, \
	{ "maxpartitions", CTLTYPE_INT }, \
	{ "rawpartition", CTLTYPE_INT }, \
	{ "gap", 0 }, \
	{ "gap", 0 }, \
	{ "osversion", CTLTYPE_STRING }, \
	{ "somaxconn", CTLTYPE_INT }, \
	{ "sominconn", CTLTYPE_INT }, \
	{ "usermount", CTLTYPE_INT }, \
	{ "random", CTLTYPE_STRUCT }, \
	{ "nosuidcoredump", CTLTYPE_INT }, \
	{ "fsync", CTLTYPE_INT }, \
	{ "sysvmsg", CTLTYPE_INT }, \
	{ "sysvsem", CTLTYPE_INT }, \
	{ "sysvshm", CTLTYPE_INT }, \
	{ "arandom", CTLTYPE_INT }, \
	{ "msgbufsize", CTLTYPE_INT }, \
	{ "malloc", CTLTYPE_NODE }, \
	{ "cp_time", CTLTYPE_STRUCT }, \
	{ "nchstats", CTLTYPE_STRUCT }, \
	{ "forkstat", CTLTYPE_STRUCT }, \
	{ "nselcoll", CTLTYPE_INT }, \
	{ "tty", CTLTYPE_NODE }, \
	{ "ccpu", CTLTYPE_INT }, \
	{ "fscale", CTLTYPE_INT }, \
	{ "nprocs", CTLTYPE_INT }, \
	{ "msgbuf", CTLTYPE_STRUCT }, \
	{ "pool", CTLTYPE_NODE }, \
	{ "stackgap_random", CTLTYPE_INT }, \
	{ "sysvipc_info", CTLTYPE_INT }, \
	{ "usercrypto", CTLTYPE_INT }, \
	{ "cryptodevallowsoft", CTLTYPE_INT }, \
	{ "splassert", CTLTYPE_INT }, \
	{ "procargs", CTLTYPE_NODE }, \
	{ "nfiles", CTLTYPE_INT }, \
	{ "ttycount", CTLTYPE_INT }, \
	{ "numvnodes", CTLTYPE_INT }, \
	{ "mbstat", CTLTYPE_STRUCT }, \
	{ "userasymcrypto", CTLTYPE_INT }, \
	{ "seminfo", CTLTYPE_STRUCT }, \
	{ "shminfo", CTLTYPE_STRUCT }, \
	{ "intrcnt", CTLTYPE_NODE }, \
 	{ "watchdog", CTLTYPE_NODE }, \
 	{ "emul", CTLTYPE_NODE }, \
 	{ "proc", CTLTYPE_STRUCT }, \
 	{ "maxclusters", CTLTYPE_INT }, \
	{ "evcount", CTLTYPE_NODE }, \
 	{ "timecounter", CTLTYPE_NODE }, \
 	{ "maxlocksperuid", CTLTYPE_INT }, \
 	{ "cp_time2", CTLTYPE_STRUCT }, \
	{ "bufcachepercent", CTLTYPE_INT }, \
	{ "file2", CTLTYPE_STRUCT }, \
	{ "rthreads", CTLTYPE_INT }, \
	{ "consdev", CTLTYPE_STRUCT }, \
	{ "netlivelocks", CTLTYPE_INT }, \
	{ "pool_debug", CTLTYPE_INT }, \
}

/*
 * KERN_EMUL subtypes.
 */
#define	KERN_EMUL_NUM		0
/* Fourth level sysctl names */
#define KERN_EMUL_NAME		0
#define KERN_EMUL_ENABLED	1


/*
 * KERN_PROC subtypes
 */
#define	KERN_PROC_ALL		0	/* everything but kernel threads */
#define	KERN_PROC_PID		1	/* by process id */
#define	KERN_PROC_PGRP		2	/* by process group id */
#define	KERN_PROC_SESSION	3	/* by session of pid */
#define	KERN_PROC_TTY		4	/* by controlling tty */
#define	KERN_PROC_UID		5	/* by effective uid */
#define	KERN_PROC_RUID		6	/* by real uid */
#define	KERN_PROC_KTHREAD	7	/* also return kernel threads */

/*
 * KERN_SYSVIPC_INFO subtypes
 */
#define KERN_SYSVIPC_MSG_INFO	1	/* msginfo and msqid_ds */
#define KERN_SYSVIPC_SEM_INFO	2	/* seminfo and semid_ds */
#define KERN_SYSVIPC_SHM_INFO	3	/* shminfo and shmid_ds */

/*
 * KERN_PROC_ARGS subtypes
 */
#define KERN_PROC_ARGV		1
#define KERN_PROC_NARGV		2
#define KERN_PROC_ENV		3
#define KERN_PROC_NENV		4

/*
 * KERN_PROC subtype ops return arrays of relatively fixed size
 * structures of process info.   Use 8 byte alignment, and new
 * elements should only be added to the end of this structure so
 * binary compatibility can be preserved.
 */
#define	KI_NGROUPS	16
#define	KI_MAXCOMLEN	24	/* extra for 8 byte alignment */
#define	KI_WMESGLEN	8
#define	KI_MAXLOGNAME	32
#define	KI_EMULNAMELEN	8

#define KI_NOCPU	(~(u_int64_t)0)

struct kinfo_proc {
#ifndef kinfo_proc2
#define kinfo_proc2	kinfo_proc
#endif
	u_int64_t p_forw;		/* PTR: linked run/sleep queue. */
	u_int64_t p_back;
	u_int64_t p_paddr;		/* PTR: address of proc */

	u_int64_t p_addr;		/* PTR: Kernel virtual addr of u-area */
	u_int64_t p_fd;			/* PTR: Ptr to open files structure. */
	u_int64_t p_stats;		/* PTR: Accounting/statistics */
	u_int64_t p_limit;		/* PTR: Process limits. */
	u_int64_t p_vmspace;		/* PTR: Address space. */
	u_int64_t p_sigacts;		/* PTR: Signal actions, state */
	u_int64_t p_sess;		/* PTR: session pointer */
	u_int64_t p_tsess;		/* PTR: tty session pointer */
	u_int64_t p_ru;			/* PTR: Exit information. XXX */

	int32_t	p_eflag;		/* LONG: extra kinfo_proc flags */
#define	EPROC_CTTY	0x01	/* controlling tty vnode active */
#define	EPROC_SLEADER	0x02	/* session leader */
	int32_t	p_exitsig;		/* INT: signal to sent to parent on exit */
	int32_t	p_flag;			/* INT: P_* flags. */

	int32_t	p_pid;			/* PID_T: Process identifier. */
	int32_t	p_ppid;			/* PID_T: Parent process id */
	int32_t	p_sid;			/* PID_T: session id */
	int32_t	p__pgid;		/* PID_T: process group id */
					/* XXX: <sys/proc.h> hijacks p_pgid */
	int32_t	p_tpgid;		/* PID_T: tty process group id */

	u_int32_t p_uid;		/* UID_T: effective user id */
	u_int32_t p_ruid;		/* UID_T: real user id */
	u_int32_t p_gid;		/* GID_T: effective group id */
	u_int32_t p_rgid;		/* GID_T: real group id */

	u_int32_t p_groups[KI_NGROUPS];	/* GID_T: groups */
	int16_t	p_ngroups;		/* SHORT: number of groups */

	int16_t	p_jobc;			/* SHORT: job control counter */
	u_int32_t p_tdev;		/* DEV_T: controlling tty dev */

	u_int32_t p_estcpu;		/* U_INT: Time averaged value of p_cpticks. */
	u_int32_t p_rtime_sec;		/* STRUCT TIMEVAL: Real time. */
	u_int32_t p_rtime_usec;		/* STRUCT TIMEVAL: Real time. */
	int32_t	p_cpticks;		/* INT: Ticks of cpu time. */
	u_int32_t p_pctcpu;		/* FIXPT_T: %cpu for this process during p_swtime */
	u_int32_t p_swtime;		/* U_INT: Time swapped in or out. */
	u_int32_t p_slptime;		/* U_INT: Time since last blocked. */
	int32_t	p_schedflags;		/* INT: PSCHED_* flags */

	u_int64_t p_uticks;		/* U_QUAD_T: Statclock hits in user mode. */
	u_int64_t p_sticks;		/* U_QUAD_T: Statclock hits in system mode. */
	u_int64_t p_iticks;		/* U_QUAD_T: Statclock hits processing intr. */

	u_int64_t p_tracep;		/* PTR: Trace to vnode or file */
	int32_t	p_traceflag;		/* INT: Kernel trace points. */

	int32_t p_holdcnt;		/* INT: If non-zero, don't swap. */

	int32_t p_siglist;		/* INT: Signals arrived but not delivered. */
	u_int32_t p_sigmask;		/* SIGSET_T: Current signal mask. */
	u_int32_t p_sigignore;		/* SIGSET_T: Signals being ignored. */
	u_int32_t p_sigcatch;		/* SIGSET_T: Signals being caught by user. */

	int8_t	p_stat;			/* CHAR: S* process status (from LWP). */
	u_int8_t p_priority;		/* U_CHAR: Process priority. */
	u_int8_t p_usrpri;		/* U_CHAR: User-priority based on p_cpu and ps_nice. */
	u_int8_t p_nice;		/* U_CHAR: Process "nice" value. */

	u_int16_t p_xstat;		/* U_SHORT: Exit status for wait; also stop signal. */
	u_int16_t p_acflag;		/* U_SHORT: Accounting flags. */

	char	p_comm[KI_MAXCOMLEN];

	char	p_wmesg[KI_WMESGLEN];	/* wchan message */
	u_int64_t p_wchan;		/* PTR: sleep address. */

	char	p_login[KI_MAXLOGNAME];	/* setlogin() name */

	int32_t	p_vm_rssize;		/* SEGSZ_T: current resident set size in pages */
	int32_t	p_vm_tsize;		/* SEGSZ_T: text size (pages) */
	int32_t	p_vm_dsize;		/* SEGSZ_T: data size (pages) */
	int32_t	p_vm_ssize;		/* SEGSZ_T: stack size (pages) */

	int64_t	p_uvalid;		/* CHAR: following p_u* members from struct user are valid */
					/* XXX 64 bits for alignment */
	u_int32_t p_ustart_sec;		/* STRUCT TIMEVAL: starting time. */
	u_int32_t p_ustart_usec;	/* STRUCT TIMEVAL: starting time. */

	u_int32_t p_uutime_sec;		/* STRUCT TIMEVAL: user time. */
	u_int32_t p_uutime_usec;	/* STRUCT TIMEVAL: user time. */
	u_int32_t p_ustime_sec;		/* STRUCT TIMEVAL: system time. */
	u_int32_t p_ustime_usec;	/* STRUCT TIMEVAL: system time. */

	u_int64_t p_uru_maxrss;		/* LONG: max resident set size. */
	u_int64_t p_uru_ixrss;		/* LONG: integral shared memory size. */
	u_int64_t p_uru_idrss;		/* LONG: integral unshared data ". */
	u_int64_t p_uru_isrss;		/* LONG: integral unshared stack ". */
	u_int64_t p_uru_minflt;		/* LONG: page reclaims. */
	u_int64_t p_uru_majflt;		/* LONG: page faults. */
	u_int64_t p_uru_nswap;		/* LONG: swaps. */
	u_int64_t p_uru_inblock;	/* LONG: block input operations. */
	u_int64_t p_uru_oublock;	/* LONG: block output operations. */
	u_int64_t p_uru_msgsnd;		/* LONG: messages sent. */
	u_int64_t p_uru_msgrcv;		/* LONG: messages received. */
	u_int64_t p_uru_nsignals;	/* LONG: signals received. */
	u_int64_t p_uru_nvcsw;		/* LONG: voluntary context switches. */
	u_int64_t p_uru_nivcsw;		/* LONG: involuntary ". */

	u_int32_t p_uctime_sec;		/* STRUCT TIMEVAL: child u+s time. */
	u_int32_t p_uctime_usec;	/* STRUCT TIMEVAL: child u+s time. */
	u_int64_t p_realflag;		/* INT: P_* flags (not including LWPs). */
	u_int32_t p_svuid;		/* UID_T: saved user id */
	u_int32_t p_svgid;		/* GID_T: saved group id */
	char    p_emul[KI_EMULNAMELEN];	/* syscall emulation name */
	u_int64_t p_rlim_rss_cur;	/* RLIM_T: soft limit for rss */
	u_int64_t p_cpuid;		/* LONG: CPU id */
	u_int64_t p_vm_map_size;	/* VSIZE_T: virtual size */
};

#if defined(_KERNEL) || defined(_LIBKVM)

/*
 * Macros for filling in the bulk of a kinfo_proc structure, used
 * in the kernel to implement the KERN_PROC sysctl and in userland
 * in libkvm to implement reading from kernel crashes.  The macro
 * arguments are all pointers; by name they are:
 *	kp - target kinfo_proc structure
 *	copy_str - a function or macro invoked as copy_str(dst,src,maxlen)
 *	    that has strlcpy or memcpy semantics; the destination is
 *	    pre-filled with zeros; for libkvm, src is a kvm address
 *	p - source struct proc
 *	pr - source struct process
 *	pc - source struct pcreds
 *	uc - source struct ucreds
 *	pg - source struct pgrp
 *	paddr - kernel address of the source struct proc
 *	sess - source struct session
 *	vm - source struct vmspace
 *	lim - source struct plimits
 *	ps - source struct pstats
 *	sa - source struct sigacts
 * There are some members that are not handled by these macros
 * because they're too painful to generalize: p_ppid, p_sid, p_tdev,
 * p_tpgid, p_tsess, p_vm_rssize, p_u[us]time_{sec,usec}, p_cpuid
 */

#define PTRTOINT64(_x)	((u_int64_t)(u_long)(_x))

#define FILL_KPROC(kp, copy_str, p, pr, pc, uc, pg, paddr, praddr, sess, vm, lim, ps, sa) \
do {									\
	memset((kp), 0, sizeof(*(kp)));					\
									\
	(kp)->p_paddr = PTRTOINT64(paddr);				\
	(kp)->p_fd = PTRTOINT64((p)->p_fd);				\
	(kp)->p_stats = PTRTOINT64((p)->p_stats);			\
	(kp)->p_limit = PTRTOINT64((pr)->ps_limit);			\
	(kp)->p_vmspace = PTRTOINT64((p)->p_vmspace);			\
	(kp)->p_sigacts = PTRTOINT64((p)->p_sigacts);			\
	(kp)->p_sess = PTRTOINT64((pg)->pg_session);			\
	(kp)->p_ru = PTRTOINT64((p)->p_ru);				\
									\
	(kp)->p_exitsig = (p)->p_exitsig;				\
	(kp)->p_flag = (p)->p_flag | (pr)->ps_flags | P_INMEM;		\
									\
	(kp)->p_pid = (p)->p_pid;					\
	(kp)->p__pgid = (pg)->pg_id;					\
									\
	(kp)->p_uid = (uc)->cr_uid;					\
	(kp)->p_ruid = (pc)->p_ruid;					\
	(kp)->p_gid = (uc)->cr_gid;					\
	(kp)->p_rgid = (pc)->p_rgid;					\
	(kp)->p_svuid = (pc)->p_svuid;					\
	(kp)->p_svgid = (pc)->p_svgid;					\
									\
	memcpy((kp)->p_groups, (uc)->cr_groups,				\
	    MIN(sizeof((kp)->p_groups), sizeof((uc)->cr_groups)));	\
	(kp)->p_ngroups = (uc)->cr_ngroups;				\
									\
	(kp)->p_jobc = (pg)->pg_jobc;					\
									\
	(kp)->p_estcpu = (p)->p_estcpu;					\
	(kp)->p_rtime_sec = (p)->p_rtime.tv_sec;			\
	(kp)->p_rtime_usec = (p)->p_rtime.tv_usec;			\
	(kp)->p_cpticks = (p)->p_cpticks;				\
	(kp)->p_pctcpu = (p)->p_pctcpu;					\
									\
	(kp)->p_uticks = (p)->p_uticks;					\
	(kp)->p_sticks = (p)->p_sticks;					\
	(kp)->p_iticks = (p)->p_iticks;					\
									\
	(kp)->p_tracep = PTRTOINT64((p)->p_tracep);			\
	(kp)->p_traceflag = (p)->p_traceflag;				\
									\
	(kp)->p_siglist = (p)->p_siglist;				\
	(kp)->p_sigmask = (p)->p_sigmask;				\
	(kp)->p_sigignore = (sa) ? (sa)->ps_sigignore : 0;		\
	(kp)->p_sigcatch = (sa) ? (sa)->ps_sigcatch : 0;		\
									\
	(kp)->p_stat = (p)->p_stat;					\
	(kp)->p_nice = (pr)->ps_nice;					\
									\
	(kp)->p_xstat = (p)->p_xstat;					\
	(kp)->p_acflag = (p)->p_acflag;					\
									\
	/* XXX depends on e_name being an array and not a pointer */	\
	copy_str((kp)->p_emul, (char *)(p)->p_emul +			\
	    offsetof(struct emul, e_name), sizeof((kp)->p_emul));	\
	strlcpy((kp)->p_comm, (p)->p_comm, sizeof((kp)->p_comm));	\
	strlcpy((kp)->p_login, (sess)->s_login,			\
	    MIN(sizeof((kp)->p_login), sizeof((sess)->s_login)));	\
									\
	if ((sess)->s_ttyvp)						\
		(kp)->p_eflag |= EPROC_CTTY;				\
	if ((sess)->s_leader == (praddr))				\
		(kp)->p_eflag |= EPROC_SLEADER;				\
									\
	if ((p)->p_stat != SIDL && !P_ZOMBIE(p)) {			\
		if ((vm) != NULL) {					\
			(kp)->p_vm_rssize = (vm)->vm_rssize;		\
			(kp)->p_vm_tsize = (vm)->vm_tsize;		\
			(kp)->p_vm_dsize = (vm)->vm_dused;		\
			(kp)->p_vm_ssize = (vm)->vm_ssize;		\
		}							\
		(kp)->p_addr = PTRTOINT64((p)->p_addr);			\
		(kp)->p_stat = (p)->p_stat;				\
		(kp)->p_swtime = (p)->p_swtime;				\
		(kp)->p_slptime = (p)->p_slptime;			\
		(kp)->p_holdcnt = 1;					\
		(kp)->p_priority = (p)->p_priority;			\
		(kp)->p_usrpri = (p)->p_usrpri;				\
		if ((p)->p_wmesg)					\
			copy_str((kp)->p_wmesg, (p)->p_wmesg,		\
			    sizeof((kp)->p_wmesg));			\
		(kp)->p_wchan = PTRTOINT64((p)->p_wchan);		\
	}								\
									\
	if (lim)							\
		(kp)->p_rlim_rss_cur =					\
		    (lim)->pl_rlimit[RLIMIT_RSS].rlim_cur;		\
									\
	if (!P_ZOMBIE(p) && (ps) != NULL) {				\
		struct timeval tv;					\
									\
		(kp)->p_uvalid = 1;					\
									\
		(kp)->p_ustart_sec = (ps)->p_start.tv_sec;		\
		(kp)->p_ustart_usec = (ps)->p_start.tv_usec;		\
									\
		(kp)->p_uru_maxrss = (ps)->p_ru.ru_maxrss;		\
		(kp)->p_uru_ixrss = (ps)->p_ru.ru_ixrss;		\
		(kp)->p_uru_idrss = (ps)->p_ru.ru_idrss;		\
		(kp)->p_uru_isrss = (ps)->p_ru.ru_isrss;		\
		(kp)->p_uru_minflt = (ps)->p_ru.ru_minflt;		\
		(kp)->p_uru_majflt = (ps)->p_ru.ru_majflt;		\
		(kp)->p_uru_nswap = (ps)->p_ru.ru_nswap;		\
		(kp)->p_uru_inblock = (ps)->p_ru.ru_inblock;		\
		(kp)->p_uru_oublock = (ps)->p_ru.ru_oublock;		\
		(kp)->p_uru_msgsnd = (ps)->p_ru.ru_msgsnd;		\
		(kp)->p_uru_msgrcv = (ps)->p_ru.ru_msgrcv;		\
		(kp)->p_uru_nsignals = (ps)->p_ru.ru_nsignals;		\
		(kp)->p_uru_nvcsw = (ps)->p_ru.ru_nvcsw;		\
		(kp)->p_uru_nivcsw = (ps)->p_ru.ru_nivcsw;		\
									\
		timeradd(&(ps)->p_cru.ru_utime,				\
			 &(ps)->p_cru.ru_stime, &tv);			\
		(kp)->p_uctime_sec = tv.tv_sec;				\
		(kp)->p_uctime_usec = tv.tv_usec;			\
	}								\
									\
	(kp)->p_cpuid = KI_NOCPU;					\
} while (0)

#endif /* defined(_KERNEL) || defined(_LIBKVM) */


/*
 * kern.file2 returns an array of these structures, which are designed
 * both to be immune to 32/64 bit emulation issues and to
 * provide backwards compatibility.  The order differs slightly from
 * that of the real struct file, and some fields are taken from other
 * structures (struct vnode, struct proc) in order to make the file
 * information more useful.
 */
#define	KERN_FILE_BYFILE	1
#define	KERN_FILE_BYPID		2
#define	KERN_FILE_BYUID		3
#define	KERN_FILESLOP		10

#define KERN_FILE_TEXT		-1
#define KERN_FILE_CDIR		-2
#define KERN_FILE_RDIR		-3
#define KERN_FILE_TRACE		-4

#define KI_MNAMELEN		96	/* rounded up from 90 */

struct kinfo_file2 {
	uint64_t	f_fileaddr;	/* PTR: address of struct file */
	uint32_t	f_flag;		/* SHORT: flags (see fcntl.h) */
	uint32_t	f_iflags;	/* INT: internal flags */
	uint32_t	f_type;		/* INT: descriptor type */
	uint32_t	f_count;	/* UINT: reference count */
	uint32_t	f_msgcount;	/* UINT: references from msg queue */
	uint32_t	f_usecount;	/* INT: number active users */
	uint64_t	f_ucred;	/* PTR: creds for descriptor */
	uint32_t	f_uid;		/* UID_T: descriptor credentials */
	uint32_t	f_gid;		/* GID_T: descriptor credentials */
	uint64_t	f_ops;		/* PTR: address of fileops */
	uint64_t	f_offset;	/* OFF_T: offset */
	uint64_t	f_data;		/* PTR: descriptor data */
	uint64_t	f_rxfer;	/* UINT64: number of read xfers */
	uint64_t	f_rwfer;	/* UINT64: number of write xfers */
	uint64_t	f_seek;		/* UINT64: number of seek operations */
	uint64_t	f_rbytes;	/* UINT64: total bytes read */
	uint64_t	f_wbytes;	/* UINT64: total bytes written */

	/* information about the vnode associated with this file */
	uint64_t	v_un;		/* PTR: socket, specinfo, etc */
	uint32_t	v_type;		/* ENUM: vnode type */
	uint32_t	v_tag;		/* ENUM: type of underlying data */
	uint32_t	v_flag;		/* UINT: vnode flags */
	uint32_t	va_rdev;	/* DEV_T: raw device */
	uint64_t	v_data;		/* PTR: private data for fs */
	uint64_t	v_mount;	/* PTR: mount info for fs */
	uint64_t	va_fileid;	/* LONG: file id */
	uint64_t	va_size;	/* UINT64_T: file size in bytes */
	uint32_t	va_mode;	/* MODE_T: file access mode and type */
	uint32_t	va_fsid;	/* DEV_T: filesystem device */
	char		f_mntonname[KI_MNAMELEN];

	/* socket information */
	uint32_t	so_type;	/* SHORT: socket type */
	uint32_t	so_state;	/* SHORT: socket state */
	uint64_t	so_pcb;		/* PTR: socket pcb */
	uint32_t	so_protocol;	/* SHORT: socket protocol type */
	uint32_t	so_family;	/* INT: socket domain family */
	uint64_t	inp_ppcb;	/* PTR: pointer to per-protocol pcb */
	uint32_t	inp_lport;	/* SHORT: local inet port */
	uint32_t	inp_laddru[4];	/* STRUCT: local inet addr */
	uint32_t	inp_fport;	/* SHORT: foreign inet port */
	uint32_t	inp_faddru[4];	/* STRUCT: foreign inet addr */
	uint64_t	unp_conn;	/* PTR: connected socket cntrl block */

	/* pipe information */
	uint64_t	pipe_peer;	/* PTR: link with other direction */
	uint32_t	pipe_state;	/* UINT: pipe status info */

	/* kqueue information */
	uint32_t	kq_count;	/* INT: number of pending events */
	uint32_t	kq_state;	/* INT: kqueue status information */

	/* systrace information */
	uint32_t	str_npolicies;	/* INT: number systrace policies */

	/* process information when retrieved via KERN_FILE_BY[PU]ID */
	uint32_t	p_pid;		/* PID_T: process id */
	int32_t		fd_fd;		/* INT: descriptor number */
	uint32_t	fd_ofileflags;	/* CHAR: open file flags */
	uint32_t	p_uid;		/* UID_T: process credentials */
	uint32_t	p_gid;		/* GID_T: process credentials */
	uint32_t	__spare;	/* padding */
	char		p_comm[KI_MAXCOMLEN];
};

/*
 * KERN_INTRCNT
 */
#define KERN_INTRCNT_NUM	1	/* int: # intrcnt */
#define KERN_INTRCNT_CNT	2	/* node: intrcnt */
#define KERN_INTRCNT_NAME	3	/* node: names */
#define KERN_INTRCNT_VECTOR	4	/* node: interrupt vector # */
#define KERN_INTRCNT_MAXID	5

#define CTL_KERN_INTRCNT_NAMES { \
	{ 0, 0 }, \
	{ "nintrcnt", CTLTYPE_INT }, \
	{ "intrcnt", CTLTYPE_NODE }, \
	{ "intrname", CTLTYPE_NODE }, \
}

/*
 * KERN_WATCHDOG
 */
#define KERN_WATCHDOG_PERIOD	1	/* int: watchdog period */
#define KERN_WATCHDOG_AUTO	2	/* int: automatic tickle */
#define KERN_WATCHDOG_MAXID	3

#define CTL_KERN_WATCHDOG_NAMES { \
	{ 0, 0 }, \
	{ "period", CTLTYPE_INT }, \
	{ "auto", CTLTYPE_INT }, \
}

/*
 * KERN_TIMECOUNTER
 */
#define KERN_TIMECOUNTER_TICK		1	/* int: number of revolutions */
#define KERN_TIMECOUNTER_TIMESTEPWARNINGS 2	/* int: log a warning when time changes */
#define KERN_TIMECOUNTER_HARDWARE	3	/* string: tick hardware used */
#define KERN_TIMECOUNTER_CHOICE		4	/* string: tick hardware used */
#define KERN_TIMECOUNTER_MAXID		5

#define CTL_KERN_TIMECOUNTER_NAMES { \
	{ 0, 0 }, \
	{ "tick", CTLTYPE_INT }, \
	{ "timestepwarnings", CTLTYPE_INT }, \
	{ "hardware", CTLTYPE_STRING }, \
	{ "choice", CTLTYPE_STRING }, \
}

/*
 * CTL_FS identifiers
 */
#define	FS_POSIX	1		/* POSIX flags */
#define	FS_MAXID	2

#define	CTL_FS_NAMES { \
	{ 0, 0 }, \
	{ "posix", CTLTYPE_NODE }, \
}

/*
 * CTL_FS identifiers
 */
#define	FS_POSIX_SETUID	1		/* int: always clear SGID/SUID bit when owner change */
#define	FS_POSIX_MAXID	2

#define	CTL_FS_POSIX_NAMES { \
	{ 0, 0 }, \
	{ "setuid", CTLTYPE_INT }, \
}

/*
 * CTL_HW identifiers
 */
#define	HW_MACHINE		 1	/* string: machine class */
#define	HW_MODEL		 2	/* string: specific machine model */
#define	HW_NCPU			 3	/* int: number of cpus being used */
#define	HW_BYTEORDER		 4	/* int: machine byte order */
#define	HW_PHYSMEM		 5	/* int: total memory */
#define	HW_USERMEM		 6	/* int: non-kernel memory */
#define	HW_PAGESIZE		 7	/* int: software page size */
#define	HW_DISKNAMES		 8	/* strings: disk drive names */
#define	HW_DISKSTATS		 9	/* struct: diskstats[] */
#define	HW_DISKCOUNT		10	/* int: number of disks */
#define	HW_SENSORS		11	/* node: hardware monitors */
#define	HW_CPUSPEED		12	/* get CPU frequency */
#define	HW_SETPERF		13	/* set CPU performance % */
#define	HW_VENDOR		14	/* string: vendor name */
#define	HW_PRODUCT		15	/* string: product name */
#define	HW_VERSION		16	/* string: hardware version */
#define	HW_SERIALNO		17	/* string: hardware serial number */
#define	HW_UUID			18	/* string: universal unique id */
#define	HW_PHYSMEM64		19	/* quad: total memory */
#define	HW_USERMEM64		20	/* quad: non-kernel memory */
#define	HW_NCPUFOUND		21	/* int: number of cpus found*/
#define	HW_ALLOWPOWERDOWN	22	/* allow power button shutdown */
#define	HW_MAXID		23	/* number of valid hw ids */

#define	CTL_HW_NAMES { \
	{ 0, 0 }, \
	{ "machine", CTLTYPE_STRING }, \
	{ "model", CTLTYPE_STRING }, \
	{ "ncpu", CTLTYPE_INT }, \
	{ "byteorder", CTLTYPE_INT }, \
	{ "gap", 0 }, \
	{ "gap", 0 }, \
	{ "pagesize", CTLTYPE_INT }, \
	{ "disknames", CTLTYPE_STRING }, \
	{ "diskstats", CTLTYPE_STRUCT }, \
	{ "diskcount", CTLTYPE_INT }, \
	{ "sensors", CTLTYPE_NODE}, \
	{ "cpuspeed", CTLTYPE_INT }, \
	{ "setperf", CTLTYPE_INT }, \
	{ "vendor", CTLTYPE_STRING }, \
	{ "product", CTLTYPE_STRING }, \
	{ "version", CTLTYPE_STRING }, \
	{ "serialno", CTLTYPE_STRING }, \
	{ "uuid", CTLTYPE_STRING }, \
	{ "physmem", CTLTYPE_QUAD }, \
	{ "usermem", CTLTYPE_QUAD }, \
	{ "ncpufound", CTLTYPE_INT }, \
	{ "allowpowerdown", CTLTYPE_INT }, \
}

/*
 * CTL_USER definitions
 */
#define	USER_CS_PATH		 1	/* string: _CS_PATH */
#define	USER_BC_BASE_MAX	 2	/* int: BC_BASE_MAX */
#define	USER_BC_DIM_MAX		 3	/* int: BC_DIM_MAX */
#define	USER_BC_SCALE_MAX	 4	/* int: BC_SCALE_MAX */
#define	USER_BC_STRING_MAX	 5	/* int: BC_STRING_MAX */
#define	USER_COLL_WEIGHTS_MAX	 6	/* int: COLL_WEIGHTS_MAX */
#define	USER_EXPR_NEST_MAX	 7	/* int: EXPR_NEST_MAX */
#define	USER_LINE_MAX		 8	/* int: LINE_MAX */
#define	USER_RE_DUP_MAX		 9	/* int: RE_DUP_MAX */
#define	USER_POSIX2_VERSION	10	/* int: POSIX2_VERSION */
#define	USER_POSIX2_C_BIND	11	/* int: POSIX2_C_BIND */
#define	USER_POSIX2_C_DEV	12	/* int: POSIX2_C_DEV */
#define	USER_POSIX2_CHAR_TERM	13	/* int: POSIX2_CHAR_TERM */
#define	USER_POSIX2_FORT_DEV	14	/* int: POSIX2_FORT_DEV */
#define	USER_POSIX2_FORT_RUN	15	/* int: POSIX2_FORT_RUN */
#define	USER_POSIX2_LOCALEDEF	16	/* int: POSIX2_LOCALEDEF */
#define	USER_POSIX2_SW_DEV	17	/* int: POSIX2_SW_DEV */
#define	USER_POSIX2_UPE		18	/* int: POSIX2_UPE */
#define	USER_STREAM_MAX		19	/* int: POSIX2_STREAM_MAX */
#define	USER_TZNAME_MAX		20	/* int: POSIX2_TZNAME_MAX */
#define	USER_MAXID		21	/* number of valid user ids */

#define	CTL_USER_NAMES { \
	{ 0, 0 }, \
	{ "cs_path", CTLTYPE_STRING }, \
	{ "bc_base_max", CTLTYPE_INT }, \
	{ "bc_dim_max", CTLTYPE_INT }, \
	{ "bc_scale_max", CTLTYPE_INT }, \
	{ "bc_string_max", CTLTYPE_INT }, \
	{ "coll_weights_max", CTLTYPE_INT }, \
	{ "expr_nest_max", CTLTYPE_INT }, \
	{ "line_max", CTLTYPE_INT }, \
	{ "re_dup_max", CTLTYPE_INT }, \
	{ "posix2_version", CTLTYPE_INT }, \
	{ "posix2_c_bind", CTLTYPE_INT }, \
	{ "posix2_c_dev", CTLTYPE_INT }, \
	{ "posix2_char_term", CTLTYPE_INT }, \
	{ "posix2_fort_dev", CTLTYPE_INT }, \
	{ "posix2_fort_run", CTLTYPE_INT }, \
	{ "posix2_localedef", CTLTYPE_INT }, \
	{ "posix2_sw_dev", CTLTYPE_INT }, \
	{ "posix2_upe", CTLTYPE_INT }, \
	{ "stream_max", CTLTYPE_INT }, \
	{ "tzname_max", CTLTYPE_INT }, \
}

/*
 * CTL_DEBUG definitions
 *
 * Second level identifier specifies which debug variable.
 * Third level identifier specifies which structure component.
 */
#define	CTL_DEBUG_NAME		0	/* string: variable name */
#define	CTL_DEBUG_VALUE		1	/* int: variable value */
#define	CTL_DEBUG_MAXID		20

#ifdef	_KERNEL
#ifdef	DEBUG
/*
 * CTL_DEBUG variables.
 *
 * These are declared as separate variables so that they can be
 * individually initialized at the location of their associated
 * variable. The loader prevents multiple use by issuing errors
 * if a variable is initialized in more than one place. They are
 * aggregated into an array in debug_sysctl(), so that it can
 * conveniently locate them when querried. If more debugging
 * variables are added, they must also be declared here and also
 * entered into the array.
 */
struct ctldebug {
	char	*debugname;	/* name of debugging variable */
	int	*debugvar;	/* pointer to debugging variable */
};
extern struct ctldebug debug0, debug1, debug2, debug3, debug4;
extern struct ctldebug debug5, debug6, debug7, debug8, debug9;
extern struct ctldebug debug10, debug11, debug12, debug13, debug14;
extern struct ctldebug debug15, debug16, debug17, debug18, debug19;
#endif	/* DEBUG */

/*
 * Internal sysctl function calling convention:
 *
 *	(*sysctlfn)(name, namelen, oldval, oldlenp, newval, newlen);
 *
 * The name parameter points at the next component of the name to be
 * interpreted.  The namelen parameter is the number of integers in
 * the name.
 */
typedef int (sysctlfn)(int *, u_int, void *, size_t *, void *, size_t, struct proc *);

int sysctl_int(void *, size_t *, void *, size_t, int *);
int sysctl_int_lower(void *, size_t *, void *, size_t, int *);
int sysctl_rdint(void *, size_t *, void *, int);
int sysctl_int_arr(int **, int *, u_int, void *, size_t *, void *, size_t);
int sysctl_quad(void *, size_t *, void *, size_t, int64_t *);
int sysctl_rdquad(void *, size_t *, void *, int64_t);
int sysctl_string(void *, size_t *, void *, size_t, char *, int);
int sysctl_tstring(void *, size_t *, void *, size_t, char *, int);
int sysctl__string(void *, size_t *, void *, size_t, char *, int, int);
int sysctl_rdstring(void *, size_t *, void *, const char *);
int sysctl_rdstruct(void *, size_t *, void *, const void *, int);
int sysctl_struct(void *, size_t *, void *, size_t, void *, int);
int sysctl_file(char *, size_t *, struct proc *);
int sysctl_file2(int *, u_int, char *, size_t *, struct proc *);
int sysctl_doproc(int *, u_int, char *, size_t *);
struct radix_node;
struct walkarg;
int sysctl_dumpentry(struct radix_node *, void *, u_int);
int sysctl_iflist(int, struct walkarg *);
int sysctl_rtable(int *, u_int, void *, size_t *, void *, size_t);
int sysctl_clockrate(char *, size_t *, void *);
int sysctl_vnode(char *, size_t *, struct proc *);
#ifdef GPROF
int sysctl_doprof(int *, u_int, void *, size_t *, void *, size_t);
#endif
int sysctl_dopool(int *, u_int, char *, size_t *);

void fill_file2(struct kinfo_file2 *, struct file *, struct filedesc *,
    int, struct vnode *, struct proc *, struct proc *);

void fill_kproc(struct proc *, struct kinfo_proc *);

int kern_sysctl(int *, u_int, void *, size_t *, void *, size_t,
		     struct proc *);
int hw_sysctl(int *, u_int, void *, size_t *, void *, size_t,
		   struct proc *);
#ifdef DEBUG
int debug_sysctl(int *, u_int, void *, size_t *, void *, size_t,
		      struct proc *);
#endif
int vm_sysctl(int *, u_int, void *, size_t *, void *, size_t,
		   struct proc *);
int fs_sysctl(int *, u_int, void *, size_t *, void *, size_t,
		   struct proc *);
int fs_posix_sysctl(int *, u_int, void *, size_t *, void *, size_t,
			 struct proc *);
int net_sysctl(int *, u_int, void *, size_t *, void *, size_t,
		    struct proc *);
int cpu_sysctl(int *, u_int, void *, size_t *, void *, size_t,
		    struct proc *);
int vfs_sysctl(int *, u_int, void *, size_t *, void *, size_t,
		    struct proc *);
int sysctl_sysvipc(int *, u_int, void *, size_t *);
int sysctl_wdog(int *, u_int, void *, size_t *, void *, size_t);

extern int (*cpu_cpuspeed)(int *);
extern void (*cpu_setperf)(int);

int bpf_sysctl(int *, u_int, void *, size_t *, void *, size_t);
int pflow_sysctl(int *, u_int, void *, size_t *, void *, size_t);
int pipex_sysctl(int *, u_int, void *, size_t *, void *, size_t);

#else	/* !_KERNEL */
#include <sys/cdefs.h>

__BEGIN_DECLS
int	sysctl(int *, u_int, void *, size_t *, void *, size_t);
__END_DECLS
#endif	/* _KERNEL */
#endif	/* !_SYS_SYSCTL_H_ */
