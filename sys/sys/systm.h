/*	$OpenBSD: systm.h,v 1.88 2011/04/26 17:20:20 jsing Exp $	*/
/*	$NetBSD: systm.h,v 1.50 1996/06/09 04:55:09 briggs Exp $	*/

/*-
 * Copyright (c) 1982, 1988, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *	@(#)systm.h	8.4 (Berkeley) 2/23/94
 */

#ifndef __SYSTM_H__
#define __SYSTM_H__

#include <sys/queue.h>
#include <sys/stdarg.h>

/*
 * The `securelevel' variable controls the security level of the system.
 * It can only be decreased by process 1 (/sbin/init).
 *
 * Security levels are as follows:
 *   -1	permanently insecure mode - always run system in level 0 mode.
 *    0	insecure mode - immutable and append-only flags may be turned off.
 *	All devices may be read or written subject to permission modes.
 *    1	secure mode - immutable and append-only flags may not be changed;
 *	raw disks of mounted filesystems, /dev/mem, and /dev/kmem are
 *	read-only.
 *    2	highly secure mode - same as (1) plus raw disks are always
 *	read-only whether mounted or not. This level precludes tampering
 *	with filesystems by unmounting them, but also inhibits running
 *	newfs while the system is secured.
 *
 * In normal operation, the system runs in level 0 mode while single user
 * and in level 1 mode while multiuser. If level 2 mode is desired while
 * running multiuser, it can be set in the multiuser startup script
 * (/etc/rc.local) using sysctl(1). If it is desired to run the system
 * in level 0 mode while multiuser, initialize the variable securelevel
 * in /sys/kern/kern_sysctl.c to -1. Note that it is NOT initialized to
 * zero as that would allow the vmunix binary to be patched to -1.
 * Without initialization, securelevel loads in the BSS area which only
 * comes into existence when the kernel is loaded and hence cannot be
 * patched by a stalking hacker.
 */
extern int securelevel;		/* system security level */
extern const char *panicstr;	/* panic message */
extern const char version[];		/* system version */
extern const char copyright[];	/* system copyright */
extern const char ostype[];
extern const char osversion[];
extern const char osrelease[];
extern int cold;		/* cold start flag initialized in locore */

extern int ncpus;		/* number of CPUs used */
extern int ncpusfound;		/* number of CPUs found */
extern int nblkdev;		/* number of entries in bdevsw */
extern int nchrdev;		/* number of entries in cdevsw */

extern int selwait;		/* select timeout address */

#ifdef MULTIPROCESSOR
#define curpriority (curcpu()->ci_schedstate.spc_curpriority)
#else
extern u_char curpriority;	/* priority of current process */
#endif

extern int maxmem;		/* max memory per process */
extern int physmem;		/* physical memory */

extern dev_t dumpdev;		/* dump device */
extern long dumplo;		/* offset into dumpdev */

extern dev_t rootdev;		/* root device */
extern u_char rootduid[8];	/* root device disklabel uid */
extern struct vnode *rootvp;	/* vnode equivalent to above */

extern dev_t swapdev;		/* swapping device */
extern struct vnode *swapdev_vp;/* vnode equivalent to above */

struct proc;
#define curproc curcpu()->ci_curproc

extern int rthreads_enabled;

typedef int	sy_call_t(struct proc *, void *, register_t *);

extern struct sysent {		/* system call table */
	short	sy_narg;	/* number of args */
	short	sy_argsize;	/* total size of arguments */
	int	sy_flags;
	sy_call_t *sy_call;	/* implementing function */
} sysent[];

#define SY_MPSAFE		0x01
#define SY_NOLOCK		0x02

#if	_BYTE_ORDER == _BIG_ENDIAN
#define SCARG(p, k)	((p)->k.be.datum)	/* get arg from args pointer */
#elif	_BYTE_ORDER == _LITTLE_ENDIAN
#define SCARG(p, k)	((p)->k.le.datum)	/* get arg from args pointer */
#else
#error	"what byte order is this machine?"
#endif

#if defined(_KERNEL) && defined(SYSCALL_DEBUG)
void scdebug_call(struct proc *p, register_t code, register_t retval[]);
void scdebug_ret(struct proc *p, register_t code, int error, register_t retval[]);
#endif /* _KERNEL && SYSCALL_DEBUG */

extern int boothowto;		/* reboot flags, from console subsystem */

extern void (*v_putc)(int); /* Virtual console putc routine */

/*
 * General function declarations.
 */
int	nullop(void *);
int	enodev(void);
int	enosys(void);
int	enoioctl(void);
int	enxio(void);
int	eopnotsupp(void *);

int	lkmenodev(void);

struct vnodeopv_desc;
void vfs_opv_init_explicit(struct vnodeopv_desc *);
void vfs_opv_init_default(struct vnodeopv_desc *);
void vfs_op_init(void);

int	seltrue(dev_t dev, int which, struct proc *);
int	selfalse(dev_t dev, int which, struct proc *);
void	*hashinit(int, int, int, u_long *);
int	sys_nosys(struct proc *, void *, register_t *);

void	panic(const char *, ...)
    __attribute__((__noreturn__,__format__(__kprintf__,1,2)));
void	__assert(const char *, const char *, int, const char *)
    __attribute__((__noreturn__));
int	printf(const char *, ...)
    __attribute__((__format__(__kprintf__,1,2)));
void	uprintf(const char *, ...)
    __attribute__((__format__(__kprintf__,1,2)));
int	vprintf(const char *, va_list);
int	vsnprintf(char *, size_t, const char *, va_list);
int	snprintf(char *buf, size_t, const char *, ...)
    __attribute__((__format__(__kprintf__,3,4)));
struct tty;
void	ttyprintf(struct tty *, const char *, ...)
    __attribute__((__format__(__kprintf__,2,3)));

void	splassert_fail(int, int, const char *);
extern	int splassert_ctl;

void	assertwaitok(void);

void	tablefull(const char *);

int	kcopy(const void *, void *, size_t)
		__attribute__ ((__bounded__(__buffer__,1,3)))
		__attribute__ ((__bounded__(__buffer__,2,3)));

void	bcopy(const void *, void *, size_t)
		__attribute__ ((__bounded__(__buffer__,1,3)))
		__attribute__ ((__bounded__(__buffer__,2,3)));
void	ovbcopy(const void *, void *, size_t)
		__attribute__ ((__bounded__(__buffer__,1,3)))
		__attribute__ ((__bounded__(__buffer__,2,3)));
void	bzero(void *, size_t)
		__attribute__ ((__bounded__(__buffer__,1,2)));
void	explicit_bzero(void *, size_t)
		__attribute__ ((__bounded__(__buffer__,1,2)));
int	bcmp(const void *, const void *, size_t);
void	*memcpy(void *, const void *, size_t)
		__attribute__ ((__bounded__(__buffer__,1,3)))
		__attribute__ ((__bounded__(__buffer__,2,3)));
void	*memmove(void *, const void *, size_t)
		__attribute__ ((__bounded__(__buffer__,1,3)))
		__attribute__ ((__bounded__(__buffer__,2,3)));
void	*memset(void *, int, size_t)
		__attribute__ ((__bounded__(__buffer__,1,3)));

int	copystr(const void *, void *, size_t, size_t *)
		__attribute__ ((__bounded__(__string__,2,3)));
int	copyinstr(const void *, void *, size_t, size_t *)
		__attribute__ ((__bounded__(__string__,2,3)));
int	copyoutstr(const void *, void *, size_t, size_t *);
int	copyin(const void *, void *, size_t)
		__attribute__ ((__bounded__(__buffer__,2,3)));
int	copyout(const void *, void *, size_t);

struct timeval;
int	hzto(const struct timeval *);
int	tvtohz(const struct timeval *);
void	realitexpire(void *);

struct clockframe;
void	hardclock(struct clockframe *);
void	softclock(void *);
void	statclock(struct clockframe *);

void	initclocks(void);
void	inittodr(time_t);
void	resettodr(void);
void	cpu_initclocks(void);

void	startprofclock(struct proc *);
void	stopprofclock(struct proc *);
void	setstatclockrate(int);

struct sleep_state;
void	sleep_setup(struct sleep_state *, const volatile void *, int,
	    const char *);
void	sleep_setup_timeout(struct sleep_state *, int);
void	sleep_setup_signal(struct sleep_state *, int);
void	sleep_finish(struct sleep_state *, int);
int	sleep_finish_timeout(struct sleep_state *);
int	sleep_finish_signal(struct sleep_state *);
void	sleep_queue_init(void);

struct mutex;
void    wakeup_n(const volatile void *, int);
void    wakeup(const volatile void *);
#define wakeup_one(c) wakeup_n((c), 1)
int	tsleep(const volatile void *, int, const char *, int);
int	msleep(const volatile void *, struct mutex *, int,  const char*, int);
void	yield(void);

void	wdog_register(void *, int (*)(void *, int));

/*
 * Startup/shutdown hooks.  Startup hooks are functions running after
 * the scheduler has started but before any threads have been created
 * or root has been mounted. The shutdown hooks are functions to be run
 * with all interrupts disabled immediately before the system is
 * halted or rebooted.
 */

struct hook_desc {
	TAILQ_ENTRY(hook_desc) hd_list;
	void	(*hd_fn)(void *);
	void	*hd_arg;
};
TAILQ_HEAD(hook_desc_head, hook_desc);

extern struct hook_desc_head shutdownhook_list, startuphook_list,
    mountroothook_list;

void	*hook_establish(struct hook_desc_head *, int, void (*)(void *), void *);
void	hook_disestablish(struct hook_desc_head *, void *);
void	dohooks(struct hook_desc_head *, int);

#define HOOK_REMOVE	0x01
#define HOOK_FREE	0x02

#define startuphook_establish(fn, arg) \
	hook_establish(&startuphook_list, 1, (fn), (arg))
#define startuphook_disestablish(vhook) \
	hook_disestablish(&startuphook_list, (vhook))
#define dostartuphooks() dohooks(&startuphook_list, HOOK_REMOVE|HOOK_FREE)

#define shutdownhook_establish(fn, arg) \
	hook_establish(&shutdownhook_list, 0, (fn), (arg))
#define shutdownhook_disestablish(vhook) \
	hook_disestablish(&shutdownhook_list, (vhook))
#define doshutdownhooks() dohooks(&shutdownhook_list, HOOK_REMOVE)

#define mountroothook_establish(fn, arg) \
	hook_establish(&mountroothook_list, 1, (fn), (arg))
#define mountroothook_disestablish(vhook) \
	hook_disestablish(&mountroothook_list, (vhook))
#define domountroothooks() dohooks(&mountroothook_list, HOOK_REMOVE|HOOK_FREE)

struct uio;
int	uiomove(void *, int, struct uio *);

#if defined(_KERNEL)
int	setjmp(label_t *);
void	longjmp(label_t *);
#endif

void	consinit(void);

void	cpu_startup(void);
void	cpu_configure(void);
void	diskconf(void);

#ifdef GPROF
void	kmstartup(void);
#endif

int nfs_mountroot(void);
int dk_mountroot(void);
extern int (*mountroot)(void);

#include <lib/libkern/libkern.h>

#if defined(DDB) || defined(KGDB)
/* debugger entry points */
void	Debugger(void);	/* in DDB only */
int	read_symtab_from_file(struct proc *,struct vnode *,const char *);
#endif

#ifdef BOOT_CONFIG
void	user_config(void);
#endif

#if defined(MULTIPROCESSOR)
void	_kernel_lock_init(void);
void	_kernel_lock(void);
void	_kernel_unlock(void);
void	_kernel_proc_lock(struct proc *);
void	_kernel_proc_unlock(struct proc *);

#define	KERNEL_LOCK_INIT()		_kernel_lock_init()
#define	KERNEL_LOCK()			_kernel_lock()
#define	KERNEL_UNLOCK()			_kernel_unlock()
#define	KERNEL_PROC_LOCK(p)		_kernel_proc_lock((p))
#define	KERNEL_PROC_UNLOCK(p)		_kernel_proc_unlock((p))

#else /* ! MULTIPROCESSOR */

#define	KERNEL_LOCK_INIT()		/* nothing */
#define	KERNEL_LOCK()			/* nothing */
#define	KERNEL_UNLOCK()			/* nothing */
#define	KERNEL_PROC_LOCK(p)		/* nothing */
#define	KERNEL_PROC_UNLOCK(p)		/* nothing */

#endif /* MULTIPROCESSOR */

#endif /* __SYSTM_H__ */
