/*	$NetBSD: svr4_stat.c,v 1.16 1995/12/19 18:27:02 christos Exp $	 */

/*
 * Copyright (c) 1994 Christos Zoulas
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/filedesc.h>
#include <sys/ioctl.h>
#include <sys/kernel.h>
#include <sys/mount.h>
#include <sys/malloc.h>

#include <sys/time.h>
#include <sys/ucred.h>
#include <vm/vm.h>
#include <sys/sysctl.h>

#include <sys/syscallargs.h>

#include <compat/svr4/svr4_types.h>
#include <compat/svr4/svr4_signal.h>
#include <compat/svr4/svr4_syscallargs.h>
#include <compat/svr4/svr4_util.h>
#include <compat/svr4/svr4_stat.h>
#include <compat/svr4/svr4_ustat.h>
#include <compat/svr4/svr4_fuser.h>
#include <compat/svr4/svr4_utsname.h>
#include <compat/svr4/svr4_systeminfo.h>
#include <compat/svr4/svr4_time.h>

#ifdef sparc
/* 
 * Solaris-2.4 on the sparc has the old stat call using the new
 * stat data structure...
 */
# define SVR4_NO_OSTAT
#endif

static void bsd_to_svr4_xstat __P((struct stat *, struct svr4_xstat *));


#ifndef SVR4_NO_OSTAT
static void bsd_to_svr4_stat __P((struct stat *, struct svr4_stat *));

static void
bsd_to_svr4_stat(st, st4)
	struct stat		*st;
	struct svr4_stat 	*st4;
{
	bzero(st4, sizeof(*st4));
	st4->st_dev = st->st_dev;
	st4->st_ino = st->st_ino;
	st4->st_mode = st->st_mode;
	st4->st_nlink = st->st_nlink;
	st4->st_uid = st->st_uid;
	st4->st_gid = st->st_gid;
	st4->st_rdev = st->st_rdev;
	st4->st_size = st->st_size;
	st4->st_atim = st->st_atimespec.ts_sec;
	st4->st_mtim = st->st_mtimespec.ts_sec;
	st4->st_ctim = st->st_ctimespec.ts_sec;
}
#endif



static void
bsd_to_svr4_xstat(st, st4)
	struct stat		*st;
	struct svr4_xstat	*st4;
{
	bzero(st4, sizeof(*st4));
	st4->st_dev = st->st_dev;
	st4->st_ino = st->st_ino;
	st4->st_mode = st->st_mode;
	st4->st_nlink = st->st_nlink;
	st4->st_uid = st->st_uid;
	st4->st_gid = st->st_gid;
	st4->st_rdev = st->st_rdev;
	st4->st_size = st->st_size;
	st4->st_atim = st->st_atimespec;
	st4->st_mtim = st->st_mtimespec;
	st4->st_ctim = st->st_ctimespec;
	st4->st_blksize = st->st_blksize;
	st4->st_blocks = st->st_blocks;
	strcpy(st4->st_fstype, "unknown");
}


int
svr4_sys_stat(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_stat_args *uap = v;
#ifdef SVR4_NO_OSTAT
	struct svr4_sys_xstat_args cup;

	SCARG(&cup, two) = 2;
	SCARG(&cup, path) = SCARG(uap, path);
	SCARG(&cup, ub) = (struct svr4_xstat *) SCARG(uap, ub);
	return svr4_sys_xstat(p, &cup, retval);
#else
	struct stat		st;
	struct svr4_stat	svr4_st;
	struct sys_stat_args	cup;
	int			error;

	caddr_t sg = stackgap_init(p->p_emul);
	SVR4_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));

	SCARG(&cup, path) = SCARG(uap, path);
	SCARG(&cup, ub) = stackgap_alloc(&sg, sizeof(struct stat));


	if ((error = sys_stat(p, &cup, retval)) != 0)
		return error;

	if ((error = copyin(SCARG(&cup, ub), &st, sizeof st)) != 0)
		return error;

	bsd_to_svr4_stat(&st, &svr4_st);

	if ((error = copyout(&svr4_st, SCARG(uap, ub), sizeof svr4_st)) != 0)
		return error;

	return 0;
#endif
}

int
svr4_sys_lstat(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_lstat_args *uap = v;
#ifdef SVR4_NO_OSTAT
	struct svr4_sys_lxstat_args cup;

	SCARG(&cup, two) = 2;
	SCARG(&cup, path) = SCARG(uap, path);
	SCARG(&cup, ub) = (struct svr4_xstat *) SCARG(uap, ub);
	return svr4_sys_lxstat(p, &cup, retval);
#else
	struct stat		st;
	struct svr4_stat	svr4_st;
	struct sys_lstat_args	cup;
	int			error;

	caddr_t sg = stackgap_init(p->p_emul);
	SVR4_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));

	SCARG(&cup, path) = SCARG(uap, path);
	SCARG(&cup, ub) = stackgap_alloc(&sg, sizeof(struct stat));

	if ((error = sys_lstat(p, &cup, retval)) != 0)
		return error;

	if ((error = copyin(SCARG(&cup, ub), &st, sizeof st)) != 0)
		return error;

	bsd_to_svr4_stat(&st, &svr4_st);

	if ((error = copyout(&svr4_st, SCARG(uap, ub), sizeof svr4_st)) != 0)
		return error;

	return 0;
#endif
}

int
svr4_sys_fstat(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_fstat_args *uap = v;
#ifdef SVR4_NO_OSTAT
	struct svr4_sys_fxstat_args cup;

	SCARG(&cup, two) = 2;
	SCARG(&cup, fd) = SCARG(uap, fd);
	SCARG(&cup, sb) = (struct svr4_xstat *) SCARG(uap, sb);
	return svr4_sys_fxstat(p, &cup, retval);
#else
	struct stat		st;
	struct svr4_stat	svr4_st;
	struct sys_fstat_args	cup;
	int			error;

	caddr_t sg = stackgap_init(p->p_emul);

	SCARG(&cup, fd) = SCARG(uap, fd);
	SCARG(&cup, sb) = stackgap_alloc(&sg, sizeof(struct stat));

	if ((error = sys_fstat(p, &cup, retval)) != 0)
		return error;

	if ((error = copyin(SCARG(&cup, sb), &st, sizeof st)) != 0)
		return error;

	bsd_to_svr4_stat(&st, &svr4_st);

	if ((error = copyout(&svr4_st, SCARG(uap, sb), sizeof svr4_st)) != 0)
		return error;

	return 0;
#endif
}


int
svr4_sys_xstat(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_xstat_args *uap = v;
	struct stat		st;
	struct svr4_xstat	svr4_st;
	struct sys_stat_args	cup;
	int			error;

	caddr_t sg = stackgap_init(p->p_emul);
	SVR4_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));

	SCARG(&cup, path) = SCARG(uap, path);
	SCARG(&cup, ub) = stackgap_alloc(&sg, sizeof(struct stat));

	if ((error = sys_stat(p, &cup, retval)) != 0)
		return error;

	if ((error = copyin(SCARG(&cup, ub), &st, sizeof st)) != 0)
		return error;

	bsd_to_svr4_xstat(&st, &svr4_st);

	if ((error = copyout(&svr4_st, SCARG(uap, ub), sizeof svr4_st)) != 0)
		return error;

	return 0;
}

int
svr4_sys_lxstat(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_lxstat_args *uap = v;
	struct stat		st;
	struct svr4_xstat	svr4_st;
	struct sys_lstat_args	cup;
	int			error;

	caddr_t sg = stackgap_init(p->p_emul);
	SVR4_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));

	SCARG(&cup, path) = SCARG(uap, path);
	SCARG(&cup, ub) = stackgap_alloc(&sg, sizeof(struct stat));

	if ((error = sys_lstat(p, &cup, retval)) != 0)
		return error;

	if ((error = copyin(SCARG(&cup, ub), &st, sizeof st)) != 0)
		return error;

	bsd_to_svr4_xstat(&st, &svr4_st);

	if ((error = copyout(&svr4_st, SCARG(uap, ub), sizeof svr4_st)) != 0)
		return error;

	return 0;
}

int
svr4_sys_fxstat(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_fxstat_args *uap = v;
	struct stat		st;
	struct svr4_xstat	svr4_st;
	struct sys_fstat_args	cup;
	int			error;

	caddr_t sg = stackgap_init(p->p_emul);

	SCARG(&cup, fd) = SCARG(uap, fd);
	SCARG(&cup, sb) = stackgap_alloc(&sg, sizeof(struct stat));

	if ((error = sys_fstat(p, &cup, retval)) != 0)
		return error;

	if ((error = copyin(SCARG(&cup, sb), &st, sizeof st)) != 0)
		return error;

	bsd_to_svr4_xstat(&st, &svr4_st);

	if ((error = copyout(&svr4_st, SCARG(uap, sb), sizeof svr4_st)) != 0)
		return error;

	return 0;
}

struct svr4_ustat_args {
	syscallarg(svr4_dev_t)		dev;
	syscallarg(struct svr4_ustat *) name;
};

int
svr4_ustat(p, uap, retval)
	register struct proc *p;
	struct svr4_ustat_args /* {
		syscallarg(svr4_dev_t)		dev;
		syscallarg(struct svr4_ustat *) name;
	} */ *uap;
	register_t *retval;
{
	struct svr4_ustat	us;
	int			error;

	bzero(&us, sizeof us);

	/*
         * XXX: should set f_tfree and f_tinode at least
         * How do we translate dev -> fstat? (and then to svr4_ustat)
         */
	if ((error = copyout(&us, SCARG(uap, name), sizeof us)) != 0)
		return (error);

	return 0;
}



int
svr4_sys_uname(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_uname_args *uap = v;
	struct svr4_utsname	sut;
	extern char ostype[], hostname[], osrelease[], version[], machine[];


	bzero(&sut, sizeof(sut));

	strncpy(sut.sysname, ostype, sizeof(sut.sysname));
	sut.sysname[sizeof(sut.sysname) - 1] = '\0';

	strncpy(sut.nodename, hostname, sizeof(sut.nodename));
	sut.nodename[sizeof(sut.nodename) - 1] = '\0';

	strncpy(sut.release, osrelease, sizeof(sut.release));
	sut.release[sizeof(sut.release) - 1] = '\0';

	strncpy(sut.version, version, sizeof(sut.version));
	sut.version[sizeof(sut.version) - 1] = '\0';

	strncpy(sut.machine, machine, sizeof(sut.machine));
	sut.machine[sizeof(sut.machine) - 1] = '\0';

	return copyout((caddr_t) &sut, (caddr_t) SCARG(uap, name),
		       sizeof(struct svr4_utsname));
}

int
svr4_sys_systeminfo(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_systeminfo_args *uap = v;
	char *str;
	int name;
	int error;
	long len;
	extern char ostype[], hostname[], osrelease[],
		    version[], machine[], domainname[];

	u_int rlen = SCARG(uap, len);

	switch (SCARG(uap, what)) {
	case SVR4_SI_SYSNAME:
		str = ostype;
		break;

	case SVR4_SI_HOSTNAME:
		str = hostname;
		break;

	case SVR4_SI_RELEASE:
		str = osrelease;
		break;

	case SVR4_SI_VERSION:
		str = version;
		break;

	case SVR4_SI_MACHINE:
		str = machine;
		break;

	case SVR4_SI_ARCHITECTURE:
		str = machine;
		break;

	case SVR4_SI_HW_SERIAL:
		str = "0";
		break;

	case SVR4_SI_HW_PROVIDER:
		str = ostype;
		break;

	case SVR4_SI_SRPC_DOMAIN:
		str = domainname;
		break;

	case SVR4_SI_SET_HOSTNAME:
		if ((error = suser(p->p_ucred, &p->p_acflag)) != 0)
			return error;
		name = KERN_HOSTNAME;
		return kern_sysctl(&name, 1, 0, 0, SCARG(uap, buf), rlen, p);

	case SVR4_SI_SET_SRPC_DOMAIN:
		if ((error = suser(p->p_ucred, &p->p_acflag)) != 0)
			return error;
		name = KERN_DOMAINNAME;
		return kern_sysctl(&name, 1, 0, 0, SCARG(uap, buf), rlen, p);

	default:
		DPRINTF(("Bad systeminfo command %d\n", SCARG(uap, what)));
		return ENOSYS;
	}

	len = strlen(str) + 1;
	if (len > rlen)
		len = rlen;

	return copyout(str, SCARG(uap, buf), len);
}


int
svr4_sys_utssys(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_utssys_args *uap = v;

	switch (SCARG(uap, sel)) {
	case 0:		/* uname(2)  */
		{
			struct svr4_sys_uname_args ua;
			SCARG(&ua, name) = SCARG(uap, a1);
			return svr4_sys_uname(p, &ua, retval);
		}

	case 2:		/* ustat(2)  */
		{
			struct svr4_ustat_args ua;
			SCARG(&ua, dev) = (svr4_dev_t) SCARG(uap, a2);
			SCARG(&ua, name) = SCARG(uap, a1);
			return svr4_ustat(p, &ua, retval);
		}

	case 3:		/* fusers(2) */
		return ENOSYS;

	default:
		return ENOSYS;
	}
	return ENOSYS;
}


int
svr4_sys_utime(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_utime_args *uap = v;
	struct svr4_utimbuf ub;
	struct timeval tbuf[2];
	struct sys_utimes_args ap;
	int error;
	caddr_t sg = stackgap_init(p->p_emul);

	SVR4_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));
	SCARG(&ap, path) = SCARG(uap, path);
	if (SCARG(uap, ubuf) == NULL) {
		if ((error = copyin(SCARG(uap, ubuf), &ub, sizeof(ub))) != 0)
			return error;
		tbuf[0].tv_sec = ub.actime;
		tbuf[0].tv_usec = 0;
		tbuf[1].tv_sec = ub.modtime;
		tbuf[1].tv_usec = 0;
		SCARG(&ap, tptr) = stackgap_alloc(&sg, sizeof(tbuf));
		if (error = copyout(tbuf, SCARG(&ap, tptr), sizeof(tbuf)) != 0)
			return error;
	}
	else
		SCARG(&ap, tptr) = NULL;
	return sys_utimes(p, &ap, retval);
}


int
svr4_sys_utimes(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_utimes_args *uap = v;
	caddr_t sg = stackgap_init(p->p_emul);
	SVR4_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));
	return sys_utimes(p, uap, retval);
}
