/*	$NetBSD: osf1_misc.c,v 1.7 1995/10/07 06:53:04 mycroft Exp $	*/

/*
 * Copyright (c) 1994, 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/filedesc.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/signal.h>
#include <sys/signalvar.h>
#include <sys/reboot.h>
#include <sys/syscallargs.h>

#include <compat/osf1/osf1_syscall.h>
#include <compat/osf1/osf1_syscallargs.h>
#include <compat/osf1/osf1_util.h>

#include <vm/vm.h>

#ifdef SYSCALL_DEBUG
extern int scdebug;
#endif

extern struct sysent osf1_sysent[];
extern char *osf1_syscallnames[];
extern void cpu_exec_ecoff_setregs __P((struct proc *, struct exec_package *,
					u_long, register_t *));

extern char sigcode[], esigcode[];

struct emul emul_osf1 = {
	"osf1",
	NULL,
	sendsig,
	OSF1_SYS_syscall,
	OSF1_SYS_MAXSYSCALL,
	osf1_sysent,
	osf1_syscallnames,
	0,
	copyargs,
	cpu_exec_ecoff_setregs,
	sigcode,
	esigcode,
};

int
osf1_sys_open(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct osf1_sys_open_args /* {
		syscallarg(char *) path;
		syscallarg(int) flags;
		syscallarg(int) mode;
	} */ *uap = v;
	struct sys_open_args /* {
		syscallarg(char *) path;
		syscallarg(int) flags;
		syscallarg(int) mode;
	} */ a;
#ifdef SYSCALL_DEBUG
	char pnbuf[1024];

	if (scdebug &&
	    copyinstr(SCARG(uap, path), pnbuf, sizeof pnbuf, NULL) == 0)
		printf("osf1_open: open: %s\n", pnbuf);
#endif

	SCARG(&a, path) = SCARG(uap, path);
	SCARG(&a, flags) = SCARG(uap, flags);		/* XXX translate */
	SCARG(&a, mode) = SCARG(uap, mode);

	return sys_open(p, &a, retval);
}

int
osf1_sys_setsysinfo(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct osf1_setsysinfo_args /* {
		syscallarg(u_long) op;
		syscallarg(caddr_t) buffer;
		syscallarg(u_long) nbytes;
		syscallarg(caddr_t) arg;
		syscallarg(u_long) flag;
	} */ *uap = v;

	return (0);
}

#define OSF1_RLIMIT_LASTCOMMON	5		/* last one that's common */
#define OSF1_RLIMIT_NOFILE	6		/* OSF1's RLIMIT_NOFILE */
#define OSF1_RLIMIT_NLIMITS	8		/* Number of OSF1 rlimits */

osf1_sys_getrlimit(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct osf1_sys_getrlimit_args /* { 
		syscallarg(u_int) which;
		syscallarg(struct rlimit *) rlp;
	} */ *uap = v;
	struct sys_getrlimit_args /* {
		syscallarg(u_int) which;
		syscallarg(struct rlimit *) rlp;
	} */ a;

	if (SCARG(uap, which) >= OSF1_RLIMIT_NLIMITS)
		return (EINVAL);

	if (SCARG(uap, which) <= OSF1_RLIMIT_LASTCOMMON)
		SCARG(&a, which) = SCARG(uap, which);
	else if (SCARG(uap, which) == OSF1_RLIMIT_NOFILE)
		SCARG(&a, which) = RLIMIT_NOFILE;
	else
		return (0);
	SCARG(&a, rlp) = SCARG(uap, rlp);

	return sys_getrlimit(p, &a, retval);
}

osf1_sys_setrlimit(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct osf1_sys_setrlimit_args /* {
		syscallarg(u_int) which;
		syscallarg(struct rlimit *) rlp;
	} */ *uap = v;
	struct sys_setrlimit_args /* {
		syscallarg(u_int) which;
		syscallarg(struct rlimit *) rlp;
	} */ a;

	if (SCARG(uap, which) >= OSF1_RLIMIT_NLIMITS)
		return (EINVAL);

	if (SCARG(uap, which) <= OSF1_RLIMIT_LASTCOMMON)
		SCARG(&a, which) = SCARG(uap, which);
	else if (SCARG(uap, which) == OSF1_RLIMIT_NOFILE)
		SCARG(&a, which) = RLIMIT_NOFILE;
	else
		return (0);
	SCARG(&a, rlp) = SCARG(uap, rlp);

	return sys_setrlimit(p, &a, retval);
}

#define	OSF1_MAP_SHARED		0x001
#define OSF1_MAP_PRIVATE	0x002
#define	OSF1_MAP_ANONYMOUS	0x010
#define	OSF1_MAP_FILE		0x000
#define OSF1_MAP_TYPE		0x0f0
#define	OSF1_MAP_FIXED		0x100
#define	OSF1_MAP_HASSEMAPHORE	0x200
#define	OSF1_MAP_INHERIT	0x400
#define	OSF1_MAP_UNALIGNED	0x800

int
osf1_sys_mmap(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct osf1_sys_mmap_args /* {
		syscallarg(caddr_t) addr;
		syscallarg(size_t) len;
		syscallarg(int) prot;
		syscallarg(int) flags;
		syscallarg(int) fd;
		syscallarg(off_t) pos;  
	} */ *uap = v;
	struct sys_mmap_args /* {
		syscallarg(caddr_t) addr;
		syscallarg(size_t) len;
		syscallarg(int) prot;
		syscallarg(int) flags;
		syscallarg(int) fd;
		syscallarg(long) pad;
		syscallarg(off_t) pos;
	} */ a;

	SCARG(&a, addr) = SCARG(uap, addr);
	SCARG(&a, len) = SCARG(uap, len);
	SCARG(&a, prot) = SCARG(uap, prot);
	SCARG(&a, fd) = SCARG(uap, fd);
	SCARG(&a, pad) = 0;
	SCARG(&a, pos) = SCARG(uap, pos);

	SCARG(&a, flags) = 0;
	if (SCARG(uap, flags) & OSF1_MAP_SHARED)
		SCARG(&a, flags) |= MAP_SHARED;
	if (SCARG(uap, flags) & OSF1_MAP_PRIVATE)
		SCARG(&a, flags) |= MAP_PRIVATE;
	switch (SCARG(uap, flags) & OSF1_MAP_TYPE) {
	case OSF1_MAP_ANONYMOUS:
		SCARG(&a, flags) |= MAP_ANON;
		break;
	case OSF1_MAP_FILE:
		SCARG(&a, flags) |= MAP_FILE;
		break;
	default:
		return (EINVAL);
	}
	if (SCARG(uap, flags) & OSF1_MAP_FIXED)
		SCARG(&a, flags) |= MAP_FIXED;
	if (SCARG(uap, flags) & OSF1_MAP_HASSEMAPHORE)
		SCARG(&a, flags) |= MAP_HASSEMAPHORE;
	if (SCARG(uap, flags) & OSF1_MAP_INHERIT)
		SCARG(&a, flags) |= MAP_INHERIT;
	if (SCARG(uap, flags) & OSF1_MAP_UNALIGNED)
		return (EINVAL);

	return sys_mmap(p, &a, retval);
}

int
osf1_sys_usleep_thread(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct osf1_sys_usleep_thread_args /* {
		syscallarg(struct timeval *) sleep;
		syscallarg(struct timeval *) slept;
	} */ *uap = v;
	struct timeval tv, endtv;
	u_long ticks;
	int error, s;

	if (error = copyin(SCARG(uap, sleep), &tv, sizeof tv))
		return (error);

	ticks = ((u_long)tv.tv_sec * 1000000 + tv.tv_usec) / tick;
	s = splclock();
	tv = time;
	splx(s);

	tsleep(p, PUSER|PCATCH, "OSF/1", ticks);	/* XXX */

	if (SCARG(uap, slept) != NULL) {
		s = splclock();
		timersub(&time, &tv, &endtv);
		splx(s);
		if (tv.tv_sec < 0 || tv.tv_usec < 0)
			tv.tv_sec = tv.tv_usec = 0;

		error = copyout(&endtv, SCARG(uap, slept), sizeof endtv);
	}
	return (error);
}

struct osf1_stat {
	int32_t		st_dev;
	u_int32_t	st_ino;
	u_int32_t	st_mode;
	u_int16_t	st_nlink;
	u_int32_t	st_uid;
	u_int32_t	st_gid;
	int32_t		st_rdev;
	u_int64_t	st_size;
	int32_t		st_atime_sec;
	int32_t		st_spare1;
	int32_t		st_mtime_sec;
	int32_t		st_spare2;
	int32_t		st_ctime_sec;
	int32_t		st_spare3;
	u_int32_t	st_blksize;
	int32_t		st_blocks;
	u_int32_t	st_flags;
	u_int32_t	st_gen;
};

/*
 * Get file status; this version follows links.
 */
/* ARGSUSED */
osf1_sys_stat(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct osf1_sys_stat_args /* {
		syscallarg(char *) path;
		syscallarg(struct osf1_stat *) ub;
	} */ *uap = v;
	struct stat sb;
	struct osf1_stat osb;
	int error;
	struct nameidata nd;

	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF, UIO_USERSPACE,
	    SCARG(uap, path), p);
	if (error = namei(&nd))
		return (error);
	error = vn_stat(nd.ni_vp, &sb, p);
	vput(nd.ni_vp);
	if (error)
		return (error);
	cvtstat2osf1(&sb, &osb);
	error = copyout((caddr_t)&osb, (caddr_t)SCARG(uap, ub), sizeof (osb));
	return (error);
}

/*
 * Get file status; this version does not follow links.
 */
/* ARGSUSED */
osf1_sys_lstat(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct osf1_sys_lstat_args /* {
		syscallarg(char *) path;
		syscallarg(struct osf1_stat *) ub;
	} */ *uap = v;
	struct stat sb;
	struct osf1_stat osb;
	int error;
	struct nameidata nd;

	NDINIT(&nd, LOOKUP, NOFOLLOW | LOCKLEAF, UIO_USERSPACE,
	    SCARG(uap, path), p);
	if (error = namei(&nd))
		return (error);
	error = vn_stat(nd.ni_vp, &sb, p);
	vput(nd.ni_vp);
	if (error)
		return (error);
	cvtstat2osf1(&sb, &osb);
	error = copyout((caddr_t)&osb, (caddr_t)SCARG(uap, ub), sizeof (osb));
	return (error);
}

/*
 * Return status information about a file descriptor.
 */
osf1_sys_fstat(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct osf1_sys_fstat_args /* {
		syscallarg(int) fd;
		syscallarg(struct osf1_stat *) sb;
	} */ *uap = v;
	register struct filedesc *fdp = p->p_fd;
	register struct file *fp;
	struct stat ub;
	struct osf1_stat oub;
	int error;

	if ((unsigned)SCARG(uap, fd) >= fdp->fd_nfiles ||
	    (fp = fdp->fd_ofiles[SCARG(uap, fd)]) == NULL)
		return (EBADF);
	switch (fp->f_type) {

	case DTYPE_VNODE:
		error = vn_stat((struct vnode *)fp->f_data, &ub, p);
		break;

	case DTYPE_SOCKET:
		error = soo_stat((struct socket *)fp->f_data, &ub);
		break;

	default:
		panic("ofstat");
		/*NOTREACHED*/
	}
	cvtstat2osf1(&ub, &oub);
	if (error == 0)
		error = copyout((caddr_t)&oub, (caddr_t)SCARG(uap, sb),
		    sizeof (oub));
	return (error);
}

#define	bsd2osf_dev(dev)	(major(dev) << 20 | minor(dev))
#define	osf2bsd_dev(dev)	makedev((dev >> 20) & 0xfff, dev & 0xfffff)

/*
 * Convert from a stat structure to an osf1 stat structure.
 */
cvtstat2osf1(st, ost)
	struct stat *st;
	struct osf1_stat *ost;
{

	ost->st_dev = bsd2osf_dev(st->st_dev);
	ost->st_ino = st->st_ino;
	ost->st_mode = st->st_mode;
	ost->st_nlink = st->st_nlink;
	ost->st_uid = st->st_uid == -2 ? (u_int16_t) -2 : st->st_uid;
	ost->st_gid = st->st_gid == -2 ? (u_int16_t) -2 : st->st_gid;
	ost->st_rdev = bsd2osf_dev(st->st_rdev);
	ost->st_size = st->st_size;
	ost->st_atime_sec = st->st_atime;
	ost->st_spare1 = 0;
	ost->st_mtime_sec = st->st_mtime;
	ost->st_spare2 = 0;
	ost->st_ctime_sec = st->st_ctime;
	ost->st_spare3 = 0;
	ost->st_blksize = st->st_blksize;
	ost->st_blocks = st->st_blocks;
	ost->st_flags = st->st_flags;
	ost->st_gen = st->st_gen;
}

int
osf1_sys_mknod(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct osf1_sys_mknod_args /* {
		syscallarg(char *) path;
		syscallarg(int) mode;
		syscallarg(int) dev;
	} */ *uap = v;
	struct sys_mknod_args a;

	SCARG(&a, path) = SCARG(uap, path);
	SCARG(&a, mode) = SCARG(uap, mode);
	SCARG(&a, dev) = osf2bsd_dev(SCARG(uap, dev));

	return sys_mknod(p, &a, retval);
}

#define OSF1_F_DUPFD	0
#define	OSF1_F_GETFD	1
#define	OSF1_F_SETFD	2
#define	OSF1_F_GETFL	3
#define	OSF1_F_SETFL	4

#define	OSF1_FAPPEND	0x00008		/* XXX OSF1_O_APPEND */
#define	OSF1_FNONBLOCK	0x00004		/* XXX OSF1_O_NONBLOCK */
#define	OSF1_FASYNC	0x00040
#define	OSF1_FSYNC	0x04000		/* XXX OSF1_O_SYNC */

int
osf1_sys_fcntl(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct osf1_sys_fcntl_args /* {
		syscallarg(int) fd;
		syscallarg(int) cmd;
		syscallarg(void *) arg;
	} */ *uap = v;
	struct sys_fcntl_args a;
	long tmp;
	int error;

	SCARG(&a, fd) = SCARG(uap, fd);

	switch (SCARG(uap, cmd)) {
	case OSF1_F_DUPFD:
		SCARG(&a, cmd) = F_DUPFD;
		SCARG(&a, arg) = SCARG(uap, arg);
		break;

	case OSF1_F_GETFD:
		SCARG(&a, cmd) = F_GETFD;
		SCARG(&a, arg) = SCARG(uap, arg);
		break;

	case OSF1_F_SETFD:
		SCARG(&a, cmd) = F_SETFD;
		SCARG(&a, arg) = SCARG(uap, arg);
		break;

	case OSF1_F_GETFL:
		SCARG(&a, cmd) = F_GETFL;
		SCARG(&a, arg) = SCARG(uap, arg);		/* ignored */
		break;

	case OSF1_F_SETFL:
		SCARG(&a, cmd) = F_SETFL;
		tmp = 0;
		if ((long)SCARG(uap, arg) & OSF1_FAPPEND)
			tmp |= FAPPEND;
		if ((long)SCARG(uap, arg) & OSF1_FNONBLOCK)
			tmp |= FNONBLOCK;
		if ((long)SCARG(uap, arg) & OSF1_FASYNC)
			tmp |= FASYNC;
		if ((long)SCARG(uap, arg) & OSF1_FSYNC)
			tmp |= FFSYNC;
		SCARG(&a, arg) = (void *)tmp;
		break;

	default:					/* XXX other cases */
		return (EINVAL);
	}

	error = sys_fcntl(p, &a, retval);

	if (error)
		return error;

	switch (SCARG(uap, cmd)) {
	case OSF1_F_GETFL:
		/* XXX */
		break;
	}

	return error;
}

int
osf1_sys_socket(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct osf1_sys_socket_args /* {
		syscallarg(int) domain;
		syscallarg(int) type;
		syscallarg(int) protocol;
	} */ *uap = v;
	struct sys_socket_args a;

	if (SCARG(uap, type) > AF_LINK)
		return (EINVAL);	/* XXX After AF_LINK, divergence. */

	SCARG(&a, domain) = SCARG(uap, domain);
	SCARG(&a, type) = SCARG(uap, type);
	SCARG(&a, protocol) = SCARG(uap, protocol);

	return sys_socket(p, &a, retval);
}

int
osf1_sys_sendto(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct osf1_sys_sendto_args /* {
		syscallarg(int) s;
		syscallarg(caddr_t) buf;
		syscallarg(size_t) len;
		syscallarg(int) flags;
		syscallarg(caddr_t) to;
		syscallarg(int) tolen;
	} */ *uap = v;
	struct sys_sendto_args a;

	if (SCARG(uap, flags) & ~0x7f)		/* unsupported flags */
		return (EINVAL);

	SCARG(&a, s) = SCARG(uap, s);
	SCARG(&a, buf) = SCARG(uap, buf);
	SCARG(&a, len) = SCARG(uap, len);
	SCARG(&a, flags) = SCARG(uap, flags);
	SCARG(&a, to) = SCARG(uap, to);
	SCARG(&a, tolen) = SCARG(uap, tolen);

	return sys_sendto(p, &a, retval);
}

#define	OSF1_RB_ASKNAME		0x001
#define	OSF1_RB_SINGLE		0x002
#define	OSF1_RB_NOSYNC		0x004
#define	OSF1_RB_HALT		0x008
#define	OSF1_RB_INITNAME	0x010
#define	OSF1_RB_DFLTROOT	0x020
#define	OSF1_RB_ALTBOOT		0x040
#define	OSF1_RB_UNIPROC		0x080
#define	OSF1_RB_ALLFLAGS	0x0ff		/* all of the above */

int
osf1_sys_reboot(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct osf1_sys_reboot_args /* {
		syscallarg(int) opt;
	} */ *uap = v;
	struct sys_reboot_args a;

	if (SCARG(uap, opt) & ~OSF1_RB_ALLFLAGS &&
	    SCARG(uap, opt) & (OSF1_RB_ALTBOOT|OSF1_RB_UNIPROC))
		return (EINVAL);

	SCARG(&a, opt) = 0;
	if (SCARG(uap, opt) & OSF1_RB_ASKNAME)
		SCARG(&a, opt) |= RB_ASKNAME;
	if (SCARG(uap, opt) & OSF1_RB_SINGLE)
		SCARG(&a, opt) |= RB_SINGLE;
	if (SCARG(uap, opt) & OSF1_RB_NOSYNC)
		SCARG(&a, opt) |= RB_NOSYNC;
	if (SCARG(uap, opt) & OSF1_RB_HALT)
		SCARG(&a, opt) |= RB_HALT;
	if (SCARG(uap, opt) & OSF1_RB_INITNAME)
		SCARG(&a, opt) |= RB_INITNAME;
	if (SCARG(uap, opt) & OSF1_RB_DFLTROOT)
		SCARG(&a, opt) |= RB_DFLTROOT;

	return sys_reboot(p, &a, retval);
}

int
osf1_sys_lseek(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct osf1_sys_lseek_args /* {  
		syscallarg(int) fd;  
		syscallarg(off_t) offset;
		syscallarg(int) whence;
	} */ *uap = v;
	struct sys_lseek_args a;

	SCARG(&a, fd) = SCARG(uap, fd);
	SCARG(&a, pad) = 0;
	SCARG(&a, offset) = SCARG(uap, offset);
	SCARG(&a, whence) = SCARG(uap, whence);

	return sys_lseek(p, &a, retval);
}

/*
 * OSF/1 defines _POSIX_SAVED_IDS, which means that our normal
 * setuid() won't work.
 *
 * Instead, by P1003.1b-1993, setuid() is supposed to work like:
 *	If the process has appropriate [super-user] priviledges, the
 *	    setuid() function sets the real user ID, effective user
 *	    ID, and the saved set-user-ID to uid.
 *	If the process does not have appropriate priviledges, but uid
 *	    is equal to the real user ID or the saved set-user-ID, the
 *	    setuid() function sets the effective user ID to uid; the
 *	    real user ID and saved set-user-ID remain unchanged by
 *	    this function call.
 */
int
osf1_sys_setuid(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct osf1_sys_setuid_args /* { 
		syscallargs(uid_t) uid;
	} */ *uap = v;
	register struct pcred *pc = p->p_cred;
	uid_t uid = SCARG(uap, uid);
	int error;

	if ((error = suser(pc->pc_ucred, &p->p_acflag)) != 0 &&
	    uid != pc->p_ruid && uid != pc->p_svuid)
		return (error);

	pc->pc_ucred = crcopy(pc->pc_ucred);
	pc->pc_ucred->cr_uid = uid;
	if (error == 0) {
	        (void)chgproccnt(pc->p_ruid, -1);
	        (void)chgproccnt(uid, 1);
		pc->p_ruid = uid;
		pc->p_svuid = uid;
	}
	p->p_flag |= P_SUGID;
	return (0);
}

/*
 * OSF/1 defines _POSIX_SAVED_IDS, which means that our normal
 * setgid() won't work.
 *
 * If you change "uid" to "gid" in the discussion, above, about
 * setuid(), you'll get a correct description of setgid().
 */
int
osf1_sys_setgid(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct osf1_sys_setgid_args /* {
		syscallargs(gid_t) gid;
	} */ *uap = v;
	register struct pcred *pc = p->p_cred;
	gid_t gid = SCARG(uap, gid);
	int error;

	if ((error = suser(pc->pc_ucred, &p->p_acflag)) != 0 &&
	    gid != pc->p_rgid && gid != pc->p_svgid)
		return (error);

	pc->pc_ucred = crcopy(pc->pc_ucred);
	pc->pc_ucred->cr_gid = gid;
	if (error == 0) {
		pc->p_rgid = gid;
		pc->p_svgid = gid;
	}
	p->p_flag |= P_SUGID;
	return (0);
}

/*
 * The structures end up being the same... but we can't be sure that
 * the other word of our iov_len is zero!
 */
struct osf1_iovec {
	char	*iov_base;
	int	iov_len;
};

int
osf1_sys_readv(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct osf1_sys_readv_args /* {
		syscallarg(int) fd;
		syscallarg(struct osf1_iovec *) iovp;
		syscallarg(u_int) iovcnt;
	} */ *uap = v;
	struct sys_readv_args /* {
		syscallarg(int) fd;
		syscallarg(struct iovec *) iovp;
		syscallarg(u_int) iovcnt;
	} */ a;
	struct emul *e = p->p_emul;
	struct osf1_iovec *oio;
	struct iovec *nio;
	int error, i;
	extern char sigcode[], esigcode[];

	if (SCARG(uap, iovcnt) > (STACKGAPLEN / sizeof (struct iovec)))
		return (EINVAL);

	oio = (struct osf1_iovec *)
	    malloc(SCARG(uap, iovcnt)*sizeof (struct osf1_iovec),
	    M_TEMP, M_WAITOK);
	nio = (struct iovec *)malloc(SCARG(uap, iovcnt)*sizeof (struct iovec),
	    M_TEMP, M_WAITOK);

	error = 0;
	if (error = copyin(SCARG(uap, iovp), oio,
	    SCARG(uap, iovcnt) * sizeof (struct osf1_iovec)))
		goto punt;
	for (i = 0; i < SCARG(uap, iovcnt); i++) {
		nio[i].iov_base = oio[i].iov_base;
		nio[i].iov_len = oio[i].iov_len;
	}

	SCARG(&a, fd) = SCARG(uap, fd);
	SCARG(&a, iovp) = (struct iovec *)STACKGAPBASE;
	SCARG(&a, iovcnt) = SCARG(uap, iovcnt);

	if (error = copyout(nio, (caddr_t)SCARG(&a, iovp),
	    SCARG(uap, iovcnt) * sizeof (struct iovec)))
		goto punt;
	error = sys_readv(p, &a, retval);

punt:
	free(oio, M_TEMP);
	free(nio, M_TEMP);
	return (error);
}

int
osf1_sys_writev(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct osf1_sys_writev_args /* {
		syscallarg(int) fd;
		syscallarg(struct osf1_iovec *) iovp;
		syscallarg(u_int) iovcnt;
	} */ *uap = v;
	struct sys_writev_args /* {
		syscallarg(int) fd;
		syscallarg(struct iovec *) iovp;
		syscallarg(u_int) iovcnt;
	} */ a;
	struct emul *e = p->p_emul;
	struct osf1_iovec *oio;
	struct iovec *nio;
	int error, i;
	extern char sigcode[], esigcode[];

	if (SCARG(uap, iovcnt) > (STACKGAPLEN / sizeof (struct iovec)))
		return (EINVAL);

	oio = (struct osf1_iovec *)
	    malloc(SCARG(uap, iovcnt)*sizeof (struct osf1_iovec),
	    M_TEMP, M_WAITOK);
	nio = (struct iovec *)malloc(SCARG(uap, iovcnt)*sizeof (struct iovec),
	    M_TEMP, M_WAITOK);

	error = 0;
	if (error = copyin(SCARG(uap, iovp), oio,
	    SCARG(uap, iovcnt) * sizeof (struct osf1_iovec)))
		goto punt;
	for (i = 0; i < SCARG(uap, iovcnt); i++) {
		nio[i].iov_base = oio[i].iov_base;
		nio[i].iov_len = oio[i].iov_len;
	}

	SCARG(&a, fd) = SCARG(uap, fd);
	SCARG(&a, iovp) = (struct iovec *)STACKGAPBASE;
	SCARG(&a, iovcnt) = SCARG(uap, iovcnt);

	if (error = copyout(nio, (caddr_t)SCARG(&a, iovp),
	    SCARG(uap, iovcnt) * sizeof (struct iovec)))
		goto punt;
	error = sys_writev(p, &a, retval);

punt:
	free(oio, M_TEMP);
	free(nio, M_TEMP);
	return (error);
}

/* More of the stupid off_t padding! */
int
osf1_sys_truncate(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct osf1_sys_truncate_args /* {
		syscallarg(char *) path;
		syscallarg(off_t) length;
	} */ *uap = v;
	struct sys_truncate_args a;

	SCARG(&a, path) = SCARG(uap, path);
	SCARG(&a, pad) = 0;
	SCARG(&a, length) = SCARG(uap, length);

	return sys_truncate(p, &a, retval);
}

int
osf1_sys_ftruncate(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct osf1_sys_ftruncate_args /* {
		syscallarg(int) fd;
		syscallarg(off_t) length;
	} */ *uap = v;
	struct sys_ftruncate_args a;

	SCARG(&a, fd) = SCARG(uap, fd);
	SCARG(&a, pad) = 0;
	SCARG(&a, length) = SCARG(uap, length);

	return sys_ftruncate(p, &a, retval);
}

int
osf1_sys_getsid(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct osf1_sys_getsid_args /* {  
		syscallarg(pid_t) pid;  
	} */ *uap = v;
	struct proc *t;

	if (SCARG(uap, pid) == 0)
		t = p;
	else if ((t = pfind(SCARG(uap, pid))) == NULL)
		return (ESRCH);

	*retval = t->p_session->s_leader->p_pid;
	return (0);
}

int
osf1_sys_getrusage(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{

	/* XXX */
	return EINVAL;
}

int
osf1_sys_madvise(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{

	/* XXX */
	return EINVAL;
}
