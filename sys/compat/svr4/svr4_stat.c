/*	$OpenBSD: svr4_stat.c,v 1.28 2009/01/29 22:08:45 guenther Exp $	 */
/*	$NetBSD: svr4_stat.c,v 1.21 1996/04/22 01:16:07 christos Exp $	 */

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
#include <sys/unistd.h>

#include <sys/time.h>
#include <sys/ucred.h>
#include <uvm/uvm_extern.h>
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
#include <compat/svr4/svr4_socket.h>

#ifdef __sparc__
/* 
 * Solaris-2.4 on the sparc has the old stat call using the new
 * stat data structure...
 */
# define SVR4_NO_OSTAT
#endif

static void bsd_to_svr4_xstat(struct stat *, struct svr4_xstat *);
static void bsd_to_svr4_stat64(struct stat *, struct svr4_stat64 *);
int svr4_ustat(struct proc *, void *, register_t *);
static int svr4_to_bsd_pathconf(int);

/*
 * SVR4 uses named pipes as named sockets, so we tell programs
 * that sockets are named pipes with mode 0
 */
#define BSD_TO_SVR4_MODE(mode) (S_ISSOCK(mode) ? S_IFIFO : (mode))


#ifndef SVR4_NO_OSTAT
static void bsd_to_svr4_stat(struct stat *, struct svr4_stat *);

static void
bsd_to_svr4_stat(st, st4)
	struct stat		*st;
	struct svr4_stat 	*st4;
{
	bzero(st4, sizeof(*st4));
	st4->st_dev = bsd_to_svr4_odev_t(st->st_dev);
	st4->st_ino = st->st_ino;
	st4->st_mode = BSD_TO_SVR4_MODE(st->st_mode);
	st4->st_nlink = st->st_nlink;
	st4->st_uid = st->st_uid;
	st4->st_gid = st->st_gid;
	st4->st_rdev = bsd_to_svr4_odev_t(st->st_rdev);
	st4->st_size = st->st_size;
	st4->st_atim = st->st_atime;
	st4->st_mtim = st->st_mtime;
	st4->st_ctim = st->st_ctime;
}
#endif


static void
bsd_to_svr4_xstat(st, st4)
	struct stat		*st;
	struct svr4_xstat	*st4;
{
	bzero(st4, sizeof(*st4));
	st4->st_dev = bsd_to_svr4_dev_t(st->st_dev);
	st4->st_ino = st->st_ino;
	st4->st_mode = BSD_TO_SVR4_MODE(st->st_mode);
	st4->st_nlink = st->st_nlink;
	st4->st_uid = st->st_uid;
	st4->st_gid = st->st_gid;
	st4->st_rdev = bsd_to_svr4_dev_t(st->st_rdev);
	st4->st_size = st->st_size;
	st4->st_atim = st->st_atim;
	st4->st_mtim = st->st_mtim;
	st4->st_ctim = st->st_ctim;
	st4->st_blksize = st->st_blksize;
	st4->st_blocks = st->st_blocks;
	strlcpy(st4->st_fstype, "unknown", sizeof st4->st_fstype);
}

static void
bsd_to_svr4_stat64(st, st4)
	struct stat		*st;
	struct svr4_stat64	*st4;
{
	bzero(st4, sizeof(*st4));
	st4->st_dev = bsd_to_svr4_dev_t(st->st_dev);
	st4->st_ino = st->st_ino;
	st4->st_mode = BSD_TO_SVR4_MODE(st->st_mode); 
	st4->st_nlink = st->st_nlink;
	st4->st_uid = st->st_uid;
	st4->st_gid = st->st_gid;
	st4->st_rdev = bsd_to_svr4_dev_t(st->st_rdev);
	st4->st_size = st->st_size;
	st4->st_atim = st->st_atim;
	st4->st_mtim = st->st_mtim;  
	st4->st_ctim = st->st_ctim;
	st4->st_blksize = st->st_blksize;
	st4->st_blocks = st->st_blocks;
	strlcpy(st4->st_fstype, "unknown", sizeof st4->st_fstype);
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
	SCARG(&cup, ub) = stackgap_alloc(&sg, sizeof(struct stat));
	SVR4_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));

	SCARG(&cup, path) = SCARG(uap, path);


	if ((error = sys_stat(p, &cup, retval)) != 0)
		return error;

	if ((error = copyin(SCARG(&cup, ub), &st, sizeof st)) != 0)
		return error;

	bsd_to_svr4_stat(&st, &svr4_st);

	if (S_ISSOCK(st.st_mode))
		(void) svr4_add_socket(p, SCARG(uap, path), &st);

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
	SCARG(&cup, ub) = stackgap_alloc(&sg, sizeof(struct stat));
	SVR4_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));

	SCARG(&cup, path) = SCARG(uap, path);

	if ((error = sys_lstat(p, &cup, retval)) != 0)
		return error;

	if ((error = copyin(SCARG(&cup, ub), &st, sizeof st)) != 0)
		return error;

	bsd_to_svr4_stat(&st, &svr4_st);

	if (S_ISSOCK(st.st_mode))
		(void) svr4_add_socket(p, SCARG(uap, path), &st);

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
	SCARG(&cup, ub) = stackgap_alloc(&sg, sizeof(struct stat));
	SVR4_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));

	SCARG(&cup, path) = SCARG(uap, path);

	if ((error = sys_stat(p, &cup, retval)) != 0)
		return error;

	if ((error = copyin(SCARG(&cup, ub), &st, sizeof st)) != 0)
		return error;

	bsd_to_svr4_xstat(&st, &svr4_st);

	if (S_ISSOCK(st.st_mode))
		(void) svr4_add_socket(p, SCARG(uap, path), &st);

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
	SCARG(&cup, ub) = stackgap_alloc(&sg, sizeof(struct stat));
	SVR4_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));

	SCARG(&cup, path) = SCARG(uap, path);

	if ((error = sys_lstat(p, &cup, retval)) != 0)
		return error;

	if ((error = copyin(SCARG(&cup, ub), &st, sizeof st)) != 0)
		return error;

	bsd_to_svr4_xstat(&st, &svr4_st);

	if (S_ISSOCK(st.st_mode))
		(void) svr4_add_socket(p, SCARG(uap, path), &st);

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


int
svr4_sys_stat64(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_stat64_args *uap = v;
	struct stat		st;
	struct svr4_stat64	svr4_st;
	struct sys_stat_args	cup;
	int			error;

	caddr_t sg = stackgap_init(p->p_emul);

	SCARG(&cup, path) = SCARG(uap, path);
	SCARG(&cup, ub) = stackgap_alloc(&sg, sizeof(struct stat));

	if ((error = sys_stat(p, &cup, retval)) != 0)
		return error;

	if ((error = copyin(SCARG(&cup, ub), &st, sizeof st)) != 0)
		return error;

	bsd_to_svr4_stat64(&st, &svr4_st);

	if (S_ISSOCK(st.st_mode))
		(void) svr4_add_socket(p, SCARG(uap, path), &st);

	if ((error = copyout(&svr4_st, SCARG(uap, sb), sizeof svr4_st)) != 0)
		return error;

	return 0;
}


int
svr4_sys_lstat64(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_lstat64_args *uap = v;
	struct stat		st;
	struct svr4_stat64	svr4_st;
	struct sys_lstat_args	cup;
	int			error;

	caddr_t sg = stackgap_init(p->p_emul);

	SCARG(&cup, path) = SCARG(uap, path);
	SCARG(&cup, ub) = stackgap_alloc(&sg, sizeof(struct stat));

	if ((error = sys_lstat(p, &cup, retval)) != 0)
		return error;

	if ((error = copyin(SCARG(&cup, ub), &st, sizeof st)) != 0)
		return error;

	bsd_to_svr4_stat64(&st, &svr4_st);

	if (S_ISSOCK(st.st_mode))
		(void) svr4_add_socket(p, SCARG(uap, path), &st);

	if ((error = copyout(&svr4_st, SCARG(uap, sb), sizeof svr4_st)) != 0)
		return error;

	return 0;
}


int
svr4_sys_fstat64(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_fstat64_args *uap = v;
	struct stat		st;
	struct svr4_stat64	svr4_st;
	struct sys_fstat_args	cup;
	int			error;

	caddr_t sg = stackgap_init(p->p_emul);

	SCARG(&cup, fd) = SCARG(uap, fd);
	SCARG(&cup, sb) = stackgap_alloc(&sg, sizeof(struct stat));

	if ((error = sys_fstat(p, &cup, retval)) != 0)
		return error;

	if ((error = copyin(SCARG(&cup, sb), &st, sizeof st)) != 0)
		return error;   

	bsd_to_svr4_stat64(&st, &svr4_st);

	if ((error = copyout(&svr4_st, SCARG(uap, sb), sizeof svr4_st)) != 0)
		return error;

	return 0;
}


struct svr4_ustat_args {
	syscallarg(svr4_dev_t)		dev;
	syscallarg(struct svr4_ustat *) name;
};

int
svr4_ustat(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_ustat_args /* {
		syscallarg(svr4_dev_t)		dev;
		syscallarg(struct svr4_ustat *) name;
	} */ *uap = v;
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
	struct svr4_utsname	*sut;
	extern char hostname[], machine[];
	const char *cp;
	char *dp, *ep;
	int error;

	sut = malloc(sizeof(*sut), M_TEMP, M_WAITOK); 
	bzero(sut, sizeof(*sut));
	strlcpy(sut->sysname, ostype, sizeof(sut->sysname));
	strlcpy(sut->nodename, hostname, sizeof(sut->nodename));
	strlcpy(sut->release, osrelease, sizeof(sut->release));

	dp = sut->version;
	ep = &sut->version[sizeof(sut->version) - 1];
	for (cp = version; *cp && *cp != '('; cp++)
		;
	for (cp++; *cp && *cp != ')' && dp < ep; cp++)
		*dp++ = *cp;
	for (; *cp && *cp != '#'; cp++)
		;
	for (; *cp && *cp != ':' && dp < ep; cp++)
		*dp++ = *cp;
	*dp = '\0';

	strlcpy(sut->machine, machine, sizeof(sut->machine));

	error = copyout(sut, SCARG(uap, name), sizeof(struct svr4_utsname));
	free(sut, M_TEMP);
	return (error);
}

int
svr4_sys_systeminfo(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_systeminfo_args *uap = v;
	const char *str;
	int name;
	int error;
	long len;
	extern char hostname[], machine[], domainname[];
#ifdef __sparc__
	extern char *cpu_class;
#endif

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

	case SVR4_SI_PLATFORM:
#ifdef __sparc__
		str = cpu_class;
#else
		str = machine;
#endif
		break;

	case SVR4_SI_KERB_REALM:
		str = "unsupported";
		break;

	case SVR4_SI_SET_HOSTNAME:
		if ((error = suser(p, 0)) != 0)
			return error;
		name = KERN_HOSTNAME;
		return kern_sysctl(&name, 1, 0, 0, SCARG(uap, buf), rlen, p);

	case SVR4_SI_SET_SRPC_DOMAIN:
		if ((error = suser(p, 0)) != 0)
			return error;
		name = KERN_DOMAINNAME;
		return kern_sysctl(&name, 1, 0, 0, SCARG(uap, buf), rlen, p);

	case SVR4_SI_SET_KERB_REALM:
		return 0;

	default:
		DPRINTF(("Bad systeminfo command %d\n", SCARG(uap, what)));
		return ENOSYS;
	}

	/* on success, sysinfo() returns byte count including \0 */
	/* result is not diminished if user buffer was too small */
	len = strlen(str) + 1;
	*retval = len;

	/* nothing to copy if user buffer is empty */
	if (rlen == 0)
		return 0;

	if (len > rlen) {
		char nul = 0;

		/* if str overruns buffer, put NUL in last place */
		len = rlen - 1;
		if (copyout(&nul, SCARG(uap, buf), sizeof(char)) != 0)
			return EFAULT;
	}

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

	SCARG(&ap, tptr) = stackgap_alloc(&sg, sizeof(tbuf));
	SVR4_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));
	SCARG(&ap, path) = SCARG(uap, path);
	if (SCARG(uap, ubuf) != NULL) {
		if ((error = copyin(SCARG(uap, ubuf), &ub, sizeof(ub))) != 0)
			return error;
		tbuf[0].tv_sec = ub.actime;
		tbuf[0].tv_usec = 0;
		tbuf[1].tv_sec = ub.modtime;
		tbuf[1].tv_usec = 0;
		error = copyout(tbuf, (struct timeval *)SCARG(&ap, tptr), sizeof(tbuf));
		if (error)
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


static int
svr4_to_bsd_pathconf(name)
	int name;
{
	switch (name) {
	case SVR4_PC_LINK_MAX:
	    	return _PC_LINK_MAX;

	case SVR4_PC_MAX_CANON:
		return _PC_MAX_CANON;

	case SVR4_PC_MAX_INPUT:
		return _PC_MAX_INPUT;

	case SVR4_PC_NAME_MAX:
		return _PC_NAME_MAX;

	case SVR4_PC_PATH_MAX:
		return _PC_PATH_MAX;

	case SVR4_PC_PIPE_BUF:
		return _PC_PIPE_BUF;

	case SVR4_PC_NO_TRUNC:
		return _PC_NO_TRUNC;

	case SVR4_PC_VDISABLE:
		return _PC_VDISABLE;

	case SVR4_PC_CHOWN_RESTRICTED:
		return _PC_CHOWN_RESTRICTED;

	case SVR4_PC_ASYNC_IO:
	case SVR4_PC_PRIO_IO:
	case SVR4_PC_SYNC_IO:
		/* Not supported */
		return 0;

	default:
		/* Invalid */
		return -1;
	}
}


int
svr4_sys_pathconf(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_pathconf_args *uap = v;
	caddr_t sg = stackgap_init(p->p_emul);

	SVR4_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));

	SCARG(uap, name) = svr4_to_bsd_pathconf(SCARG(uap, name));

	switch (SCARG(uap, name)) {
	case -1:
		*retval = -1;
		return EINVAL;
	case 0:
		*retval = 0;
		return 0;
	default:
		return sys_pathconf(p, uap, retval);
	}
}

int
svr4_sys_fpathconf(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_fpathconf_args *uap = v;

	SCARG(uap, name) = svr4_to_bsd_pathconf(SCARG(uap, name));

	switch (SCARG(uap, name)) {
	case -1:
		*retval = -1;
		return EINVAL;
	case 0:
		*retval = 0;
		return 0;
	default:
		return sys_fpathconf(p, uap, retval);
	}
}
