/*	$OpenBSD: sysctl.h,v 1.62 2003/01/21 16:59:23 markus Exp $	*/
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
#include <sys/time.h>
#include <sys/ucred.h>
#include <sys/proc.h>
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
#define	KERN_PROC		14	/* struct: process entries */
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
#define	KERN_MAXID		65	/* number of valid kern ids */

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
	{ "proc", CTLTYPE_STRUCT }, \
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
}

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
 * KERN_PROC subtype ops return arrays of augmented proc structures:
 */
struct kinfo_proc {
	struct	proc kp_proc;			/* proc structure */
	struct	eproc {
		struct	proc *e_paddr;		/* address of proc */
		struct	session *e_sess;	/* session pointer */
		struct	pcred e_pcred;		/* process credentials */
		struct	ucred e_ucred;		/* current credentials */
		struct	vmspace e_vm;		/* address space */
		struct  pstats e_pstats;	/* process stats */
		int	e_pstats_valid;		/* pstats valid? */
		pid_t	e_ppid;			/* parent process id */
		pid_t	e_pgid;			/* process group id */
		short	e_jobc;			/* job control counter */
		dev_t	e_tdev;			/* controlling tty dev */
		pid_t	e_tpgid;		/* tty process group id */
		struct	session *e_tsess;	/* tty session pointer */
#define	WMESGLEN	7
		char	e_wmesg[WMESGLEN+1];	/* wchan message */
		segsz_t e_xsize;		/* text size */
		short	e_xrssize;		/* text rss */
		short	e_xccount;		/* text references */
		short	e_xswrss;
		long	e_flag;
#define	EPROC_CTTY	0x01	/* controlling tty vnode active */
#define	EPROC_SLEADER	0x02	/* session leader */
		char	e_login[MAXLOGNAME];	/* setlogin() name */
#define	EMULNAMELEN	7
		char	e_emul[EMULNAMELEN+1];	/* syscall emulation name */
	        rlim_t	e_maxrss;
	} kp_eproc;
};

/*
 * KERN_INTR_CNT
 */
#define KERN_INTRCNT_NUM	1	/* int: # intrcnt */
#define KERN_INTRCNT_CNT	2	/* node: intrcnt */
#define KERN_INTRCNT_NAME	3	/* node: names */
#define KERN_INTRCNT_MAXID	4

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
#define	HW_MACHINE	 1		/* string: machine class */
#define	HW_MODEL	 2		/* string: specific machine model */
#define	HW_NCPU		 3		/* int: number of cpus */
#define	HW_BYTEORDER	 4		/* int: machine byte order */
#define	HW_PHYSMEM	 5		/* int: total memory */
#define	HW_USERMEM	 6		/* int: non-kernel memory */
#define	HW_PAGESIZE	 7		/* int: software page size */
#define	HW_DISKNAMES	 8		/* strings: disk drive names */
#define	HW_DISKSTATS	 9		/* struct: diskstats[] */
#define	HW_DISKCOUNT	10		/* int: number of disks */
#define	HW_MAXID	11		/* number of valid hw ids */

#define	CTL_HW_NAMES { \
	{ 0, 0 }, \
	{ "machine", CTLTYPE_STRING }, \
	{ "model", CTLTYPE_STRING }, \
	{ "ncpu", CTLTYPE_INT }, \
	{ "byteorder", CTLTYPE_INT }, \
	{ "physmem", CTLTYPE_INT }, \
	{ "usermem", CTLTYPE_INT }, \
	{ "pagesize", CTLTYPE_INT }, \
	{ "disknames", CTLTYPE_STRING }, \
	{ "diskstats", CTLTYPE_STRUCT }, \
	{ "diskcount", CTLTYPE_INT }, \
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
 * Third level identifier specifies which stucture component.
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
int sysctl_rdint(void *, size_t *, void *, int);
int sysctl_quad(void *, size_t *, void *, size_t, int64_t *);
int sysctl_rdquad(void *, size_t *, void *, int64_t);
int sysctl_string(void *, size_t *, void *, size_t, char *, int);
int sysctl_tstring(void *, size_t *, void *, size_t, char *, int);
int sysctl__string(void *, size_t *, void *, size_t, char *, int, int);
int sysctl_rdstring(void *, size_t *, void *, const char *);
int sysctl_rdstruct(void *, size_t *, void *, const void *, int);
int sysctl_struct(void *, size_t *, void *, size_t, void *, int);
int sysctl_file(char *, size_t *);
int sysctl_doproc(int *, u_int, char *, size_t *);
struct radix_node;
struct walkarg;
int sysctl_dumpentry(struct radix_node *, void *);
int sysctl_iflist(int, struct walkarg *);
int sysctl_rtable(int *, u_int, void *, size_t *, void *, size_t);
int sysctl_clockrate(char *, size_t *);
int sysctl_vnode(char *, size_t *, struct proc *);
#ifdef GPROF
int sysctl_doprof(int *, u_int, void *, size_t *, void *, size_t);
#endif
int sysctl_dopool(int *, u_int, char *, size_t *);

void fill_eproc(struct proc *, struct eproc *);

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

void sysctl_init(void);

#else	/* !_KERNEL */
#include <sys/cdefs.h>

__BEGIN_DECLS
int	sysctl(int *, u_int, void *, size_t *, void *, size_t);
__END_DECLS
#endif	/* _KERNEL */
#endif	/* !_SYS_SYSCTL_H_ */
