/*	$NetBSD: ibcs2_misc.c,v 1.13 1996/01/06 03:23:49 scottb Exp $	*/

/*
 * Copyright (c) 1994, 1995 Scott Bartram
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
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
 * from: Header: sun_misc.c,v 1.16 93/04/07 02:46:27 torek Exp 
 *
 *	@(#)sun_misc.c	8.1 (Berkeley) 6/18/93
 */

/*
 * IBCS2 compatibility module.
 *
 * IBCS2 system calls that are implemented differently in BSD are
 * handled here.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/dir.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/ioctl.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/resource.h>
#include <sys/resourcevar.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/vnode.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <sys/utsname.h>
#include <sys/unistd.h>

#include <netinet/in.h>
#include <sys/syscallargs.h>

#include <miscfs/specfs/specdev.h>

#include <vm/vm.h>
#include <sys/sysctl.h>		/* must be included after vm.h */

#include <i386/include/reg.h>

#include <compat/ibcs2/ibcs2_types.h>
#include <compat/ibcs2/ibcs2_dirent.h>
#include <compat/ibcs2/ibcs2_fcntl.h>
#include <compat/ibcs2/ibcs2_time.h>
#include <compat/ibcs2/ibcs2_signal.h>
#include <compat/ibcs2/ibcs2_timeb.h>
#include <compat/ibcs2/ibcs2_unistd.h>
#include <compat/ibcs2/ibcs2_utsname.h>
#include <compat/ibcs2/ibcs2_util.h>
#include <compat/ibcs2/ibcs2_utime.h>
#include <compat/ibcs2/ibcs2_syscallargs.h>
#include <compat/ibcs2/ibcs2_sysi86.h>


int
ibcs2_sys_ulimit(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ibcs2_sys_ulimit_args /* {
		syscallarg(int) cmd;
		syscallarg(int) newlimit;
	} */ *uap = v;
	int error;
	struct rlimit rl;
	struct sys_setrlimit_args sra;
#define IBCS2_GETFSIZE		1
#define IBCS2_SETFSIZE		2
#define IBCS2_GETPSIZE		3
#define IBCS2_GETDTABLESIZE	4
	
	switch (SCARG(uap, cmd)) {
	case IBCS2_GETFSIZE:
		*retval = p->p_rlimit[RLIMIT_FSIZE].rlim_cur;
		return 0;
	case IBCS2_SETFSIZE:	/* XXX - fix this */
#ifdef notyet
		rl.rlim_cur = SCARG(uap, newlimit);
		sra.which = RLIMIT_FSIZE;
		sra.rlp = &rl;
		error = setrlimit(p, &sra, retval);
		if (!error)
			*retval = p->p_rlimit[RLIMIT_FSIZE].rlim_cur;
		else
			DPRINTF(("failed "));
		return error;
#else
		*retval = SCARG(uap, newlimit);
		return 0;
#endif
	case IBCS2_GETPSIZE:
		*retval = p->p_rlimit[RLIMIT_RSS].rlim_cur; /* XXX */
		return 0;
	case IBCS2_GETDTABLESIZE:
		uap->cmd = IBCS2_SC_OPEN_MAX;
		return ibcs2_sys_sysconf(p, uap, retval);
	default:
		return ENOSYS;
	}
}

int
ibcs2_sys_waitsys(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ibcs2_sys_waitsys_args /* {
		syscallarg(int) a1;
		syscallarg(int) a2;
		syscallarg(int) a3;
	} */ *uap = v;
	int error, status;
	struct sys_wait4_args w4;
#define WAITPID_EFLAGS	0x8c4	/* OF, SF, ZF, PF */
	
	SCARG(&w4, rusage) = NULL;
	if ((p->p_md.md_regs->tf_eflags & WAITPID_EFLAGS) == WAITPID_EFLAGS) {
		/* waitpid */
		SCARG(&w4, pid) = SCARG(uap, a1);
		SCARG(&w4, status) = (int *)SCARG(uap, a2);
		SCARG(&w4, options) = SCARG(uap, a3);
	} else {
		/* wait */
		SCARG(&w4, pid) = WAIT_ANY;
		SCARG(&w4, status) = (int *)SCARG(uap, a1);
		SCARG(&w4, options) = 0;
	}
	if ((error = sys_wait4(p, &w4, retval)) != 0)
		return error;
	if (SCARG(&w4, status))		/* this is real iBCS brain-damage */
		return copyin((caddr_t)SCARG(&w4, status), (caddr_t)&retval[1],
			      sizeof(SCARG(&w4, status)));
	return 0;
}

int
ibcs2_sys_execv(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ibcs2_sys_execv_args /* {
		syscallarg(char *) path;
		syscallarg(char **) argp;
	} */ *uap = v;
	struct sys_execve_args ea;
	caddr_t sg = stackgap_init(p->p_emul);

        IBCS2_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));
	SCARG(&ea, path) = SCARG(uap, path);
	SCARG(&ea, argp) = SCARG(uap, argp);
	SCARG(&ea, envp) = NULL;
	return sys_execve(p, &ea, retval);
}

int
ibcs2_sys_execve(p, v, retval) 
        struct proc *p;
	void *v;
        register_t *retval;
{
	struct sys_execve_args /* {
		syscallarg(char *) path;
		syscallarg(char **) argp;
		syscallarg(char **) envp;
	} */ *uap = v;
        caddr_t sg = stackgap_init(p->p_emul);

        IBCS2_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));
        return sys_execve(p, uap, retval);
}

int
ibcs2_sys_umount(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ibcs2_sys_umount_args /* {
		syscallarg(char *) name;
	} */ *uap = v;
	struct sys_unmount_args um;

	SCARG(&um, path) = SCARG(uap, name);
	SCARG(&um, flags) = 0;
	return sys_unmount(p, &um, retval);
}

int
ibcs2_sys_mount(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ibcs2_sys_mount_args /* {
		syscallarg(char *) special;
		syscallarg(char *) dir;
		syscallarg(int) flags;
		syscallarg(int) fstype;
		syscallarg(char *) data;
		syscallarg(int) len;
	} */ *uap = v;
#ifdef notyet
	int oflags = SCARG(uap, flags), nflags, error;
	char fsname[MFSNAMELEN];

	if (oflags & (IBCS2_MS_NOSUB | IBCS2_MS_SYS5))
		return (EINVAL);
	if ((oflags & IBCS2_MS_NEWTYPE) == 0)
		return (EINVAL);
	nflags = 0;
	if (oflags & IBCS2_MS_RDONLY)
		nflags |= MNT_RDONLY;
	if (oflags & IBCS2_MS_NOSUID)
		nflags |= MNT_NOSUID;
	if (oflags & IBCS2_MS_REMOUNT)
		nflags |= MNT_UPDATE;
	SCARG(uap, flags) = nflags;

	if (error = copyinstr((caddr_t)SCARG(uap, type), fsname, sizeof fsname,
			      (u_int *)0))
		return (error);

	if (strncmp(fsname, "4.2", sizeof fsname) == 0) {
		SCARG(uap, type) = (caddr_t)STACK_ALLOC();
		if (error = copyout("ffs", SCARG(uap, type), sizeof("ffs")))
			return (error);
	} else if (strncmp(fsname, "nfs", sizeof fsname) == 0) {
		struct ibcs2_nfs_args sna;
		struct sockaddr_in sain;
		struct nfs_args na;
		struct sockaddr sa;

		if (error = copyin(SCARG(uap, data), &sna, sizeof sna))
			return (error);
		if (error = copyin(sna.addr, &sain, sizeof sain))
			return (error);
		bcopy(&sain, &sa, sizeof sa);
		sa.sa_len = sizeof(sain);
		SCARG(uap, data) = (caddr_t)STACK_ALLOC();
		na.addr = (struct sockaddr *)((int)SCARG(uap, data) + sizeof na);
		na.sotype = SOCK_DGRAM;
		na.proto = IPPROTO_UDP;
		na.fh = (nfsv2fh_t *)sna.fh;
		na.flags = sna.flags;
		na.wsize = sna.wsize;
		na.rsize = sna.rsize;
		na.timeo = sna.timeo;
		na.retrans = sna.retrans;
		na.hostname = sna.hostname;

		if (error = copyout(&sa, na.addr, sizeof sa))
			return (error);
		if (error = copyout(&na, SCARG(uap, data), sizeof na))
			return (error);
	}
	return (sys_mount(p, uap, retval));
#else
	return EINVAL;
#endif
}

/*
 * Read iBCS2-style directory entries.  We suck them into kernel space so
 * that they can be massaged before being copied out to user code.  Like
 * SunOS, we squish out `empty' entries.
 *
 * This is quite ugly, but what do you expect from compatibility code?
 */

int
ibcs2_sys_getdents(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	register struct ibcs2_sys_getdents_args /* {
		syscallarg(int) fd;
		syscallarg(char *) buf;
		syscallarg(int) nbytes;
	} */ *uap = v;
	register struct dirent *bdp;
	struct vnode *vp;
	caddr_t inp, buf;	/* BSD-format */
	int len, reclen;	/* BSD-format */
	caddr_t outp;		/* iBCS2-format */
	int resid, ibcs2_reclen;/* iBCS2-format */
	struct file *fp;
	struct uio auio;
	struct iovec aiov;
	struct ibcs2_dirent idb;
	off_t off;			/* true file offset */
	int buflen, error, eofflag;
	u_long *cookiebuf, *cookie;
	int ncookies;

	if ((error = getvnode(p->p_fd, SCARG(uap, fd), &fp)) != 0)
		return (error);

	if ((fp->f_flag & FREAD) == 0)
		return (EBADF);

	vp = (struct vnode *)fp->f_data;

	if (vp->v_type != VDIR)	/* XXX  vnode readdir op should do this */
		return (EINVAL);

	buflen = min(MAXBSIZE, SCARG(uap, nbytes));
	buf = malloc(buflen, M_TEMP, M_WAITOK);
	ncookies = buflen / 16;
	cookiebuf = malloc(ncookies * sizeof(*cookiebuf), M_TEMP, M_WAITOK);
	VOP_LOCK(vp);
	off = fp->f_offset;
again:
	aiov.iov_base = buf;
	aiov.iov_len = buflen;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_rw = UIO_READ;
	auio.uio_segflg = UIO_SYSSPACE;
	auio.uio_procp = p;
	auio.uio_resid = buflen;
	auio.uio_offset = off;
	/*
	 * First we read into the malloc'ed buffer, then
	 * we massage it into user space, one record at a time.
	 */
	error = VOP_READDIR(vp, &auio, fp->f_cred, &eofflag, cookiebuf,
	    ncookies);
	if (error)
		goto out;

	inp = buf;
	outp = SCARG(uap, buf);
	resid = SCARG(uap, nbytes);
	if ((len = buflen - auio.uio_resid) == 0)
		goto eof;

	for (cookie = cookiebuf; len > 0; len -= reclen) {
		bdp = (struct dirent *)inp;
		reclen = bdp->d_reclen;
		if (reclen & 3)
			panic("ibcs2_getdents");
		off = *cookie++;	/* each entry points to the next */
		if (bdp->d_fileno == 0) {
			inp += reclen;	/* it is a hole; squish it out */
			continue;
		}
		ibcs2_reclen = IBCS2_RECLEN(&idb, bdp->d_namlen);
		if (reclen > len || resid < ibcs2_reclen) {
			/* entry too big for buffer, so just stop */
			outp++;
			break;
		}
		/*
		 * Massage in place to make a iBCS2-shaped dirent (otherwise
		 * we have to worry about touching user memory outside of
		 * the copyout() call).
		 */
		idb.d_ino = (ibcs2_ino_t)bdp->d_fileno;
		idb.d_off = (ibcs2_off_t)off;
		idb.d_reclen = (u_short)ibcs2_reclen;
		strcpy(idb.d_name, bdp->d_name);
		if ((error = copyout((caddr_t)&idb, outp, ibcs2_reclen)))
			goto out;
		/* advance past this real entry */
		inp += reclen;
		/* advance output past iBCS2-shaped entry */
		outp += ibcs2_reclen;
		resid -= ibcs2_reclen;
	}

	/* if we squished out the whole block, try again */
	if (outp == SCARG(uap, buf))
		goto again;
	fp->f_offset = off;	/* update the vnode offset */

eof:
	*retval = SCARG(uap, nbytes) - resid;
out:
	VOP_UNLOCK(vp);
	free(cookiebuf, M_TEMP);
	free(buf, M_TEMP);
	return (error);
}

int
ibcs2_sys_read(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ibcs2_sys_read_args /* {
		syscallarg(int) fd;
		syscallarg(char *) buf;
		syscallarg(u_int) nbytes;
	} */ *uap = v;
	register struct dirent *bdp;
	register struct vnode *vp;
	register caddr_t inp, buf;	/* BSD-format */
	register int len, reclen;	/* BSD-format */
	register caddr_t outp;		/* iBCS2-format */
	register int resid;		/* iBCS2-format */
	struct file *fp;
	struct uio auio;
	struct iovec aiov;
	struct ibcs2_direct {
		ibcs2_ino_t ino;
		char name[14];
	} idb;
	off_t off;			/* true file offset */
	int buflen, error, eofflag, size;
	u_long *cookiebuf, *cookie;
	int ncookies;

	if (error = getvnode(p->p_fd, SCARG(uap, fd), &fp)) {
		if (error == EINVAL)
			return sys_read(p, uap, retval);
		else
			return error;
	}
	if ((fp->f_flag & FREAD) == 0)
		return (EBADF);
	vp = (struct vnode *)fp->f_data;
	if (vp->v_type != VDIR)
		return sys_read(p, uap, retval);
	DPRINTF(("ibcs2_read: read directory\n"));
	buflen = max(MAXBSIZE, SCARG(uap, nbytes));
	buf = malloc(buflen, M_TEMP, M_WAITOK);
	ncookies = buflen / 16;
	cookiebuf = malloc(ncookies * sizeof(*cookiebuf), M_TEMP, M_WAITOK);
	VOP_LOCK(vp);
	off = fp->f_offset;
again:
	aiov.iov_base = buf;
	aiov.iov_len = buflen;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_rw = UIO_READ;
	auio.uio_segflg = UIO_SYSSPACE;
	auio.uio_procp = p;
	auio.uio_resid = buflen;
	auio.uio_offset = off;
	/*
	 * First we read into the malloc'ed buffer, then
	 * we massage it into user space, one record at a time.
	 */
	if (error = VOP_READDIR(vp, &auio, fp->f_cred, &eofflag, cookiebuf,
				ncookies)) {
		DPRINTF(("VOP_READDIR failed: %d\n", error));
		goto out;
	}
	inp = buf + off;
	buflen -= off;
	outp = SCARG(uap, buf);
	resid = SCARG(uap, nbytes);
	if ((len = buflen - auio.uio_resid) == 0)
		goto eof;
	for (cookie = cookiebuf; len > 0 && resid > 0; len -= reclen) {
		bdp = (struct dirent *)inp;
		reclen = bdp->d_reclen;
		if (reclen & 3)
			panic("ibcs2_read");
		if (bdp->d_fileno == 0) {
			inp += reclen;	/* it is a hole; squish it out */
			off = *cookie++;
			continue;
		}
		if (reclen > len || resid < sizeof(struct ibcs2_direct)) {
			/* entry too big for buffer, so just stop */
			outp++;
			break;
		}
		/*
		 * Massage in place to make a iBCS2-shaped dirent (otherwise
		 * we have to worry about touching user memory outside of
		 * the copyout() call).
		 *
		 * TODO: if length(filename) > 14, then break filename into
		 * multiple entries and set inode = 0xffff except last
		 */
		idb.ino = (bdp->d_fileno > 0xfffe) ? 0xfffe : bdp->d_fileno;
		(void)copystr(bdp->d_name, idb.name, 14, &size);
		bzero(idb.name + size, 14 - size);
		if (error = copyout(&idb, outp, sizeof(struct ibcs2_direct)))
			goto out;
		/* advance past this real entry */
		off = *cookie++;	/* each entry points to the next */
		inp += reclen;
		/* advance output past iBCS2-shaped entry */
		outp += sizeof(struct ibcs2_direct);
		resid -= sizeof(struct ibcs2_direct);
	}
	/* if we squished out the whole block, try again */
	if (outp == SCARG(uap, buf))
		goto again;
	fp->f_offset = off;		/* update the vnode offset */
eof:
	*retval = SCARG(uap, nbytes) - resid;
out:
	VOP_UNLOCK(vp);
	free(buf, M_TEMP);
	return (error);
}

int
ibcs2_sys_mknod(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ibcs2_sys_mknod_args /* {
		syscallarg(char *) path;
		syscallarg(int) mode;
		syscallarg(int) dev;
	} */ *uap = v;
        caddr_t sg = stackgap_init(p->p_emul);

        IBCS2_CHECK_ALT_CREAT(p, &sg, SCARG(uap, path));
	if (S_ISFIFO(SCARG(uap, mode))) {
                struct sys_mkfifo_args ap;
                SCARG(&ap, path) = SCARG(uap, path);
                SCARG(&ap, mode) = SCARG(uap, mode);
		return sys_mkfifo(p, uap, retval);
	} else {
                struct sys_mknod_args ap;
                SCARG(&ap, path) = SCARG(uap, path);
                SCARG(&ap, mode) = SCARG(uap, mode);
                SCARG(&ap, dev) = SCARG(uap, dev);
                return sys_mknod(p, &ap, retval);
	}
}

int
ibcs2_sys_getgroups(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ibcs2_sys_getgroups_args /* {
		syscallarg(int) gidsetsize;
		syscallarg(ibcs2_gid_t *) gidset;
	} */ *uap = v;
	int error, i;
	ibcs2_gid_t igid, *iset;
	struct sys_getgroups_args sa;
	gid_t *gp;
	caddr_t sg = stackgap_init(p->p_emul);

	SCARG(&sa, gidsetsize) = SCARG(uap, gidsetsize);
	if (SCARG(uap, gidsetsize)) {
		SCARG(&sa, gidset) = stackgap_alloc(&sg, NGROUPS_MAX *
						    sizeof(gid_t *));
		iset = stackgap_alloc(&sg, SCARG(uap, gidsetsize) *
				      sizeof(ibcs2_gid_t));
	}
	if (error = sys_getgroups(p, &sa, retval))
		return error;
	for (i = 0, gp = SCARG(&sa, gidset); i < retval[0]; i++)
		iset[i] = (ibcs2_gid_t)*gp++;
	if (retval[0] && (error = copyout((caddr_t)iset,
					  (caddr_t)SCARG(uap, gidset),
					  sizeof(ibcs2_gid_t) * retval[0])))
		return error;
        return 0;
}

int
ibcs2_sys_setgroups(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ibcs2_sys_setgroups_args /* {
		syscallarg(int) gidsetsize;
		syscallarg(ibcs2_gid_t *) gidset;
	} */ *uap = v;
	int error, i;
	ibcs2_gid_t igid, *iset;
	struct sys_setgroups_args sa;
	gid_t *gp;
	caddr_t sg = stackgap_init(p->p_emul);

	SCARG(&sa, gidsetsize) = SCARG(uap, gidsetsize);
	SCARG(&sa, gidset) = stackgap_alloc(&sg, SCARG(&sa, gidsetsize) *
					    sizeof(gid_t *));
	iset = stackgap_alloc(&sg, SCARG(&sa, gidsetsize) *
			      sizeof(ibcs2_gid_t *));
	if (SCARG(&sa, gidsetsize)) {
		if (error = copyin((caddr_t)SCARG(uap, gidset), (caddr_t)iset, 
				   sizeof(ibcs2_gid_t *) *
				   SCARG(uap, gidsetsize)))
			return error;
	}
	for (i = 0, gp = SCARG(&sa, gidset); i < SCARG(&sa, gidsetsize); i++)
		*gp++ = (gid_t)iset[i];
	return sys_setgroups(p, &sa, retval);
}

int
ibcs2_sys_setuid(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ibcs2_sys_setuid_args /* {
		syscallarg(int) uid;
	} */ *uap = v;
	struct sys_setuid_args sa;

	SCARG(&sa, uid) = (uid_t)SCARG(uap, uid);
	return sys_setuid(p, &sa, retval);
}

int
ibcs2_sys_setgid(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ibcs2_sys_setgid_args /* {
		syscallarg(int) gid;
	} */ *uap = v;
	struct sys_setgid_args sa;

	SCARG(&sa, gid) = (gid_t)SCARG(uap, gid);
	return sys_setgid(p, &sa, retval);
}

int
xenix_sys_ftime(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct xenix_sys_ftime_args /* {
		syscallarg(struct xenix_timeb *) tp;
	} */ *uap = v;
	struct timeval tv;
	extern struct timezone tz;
	struct xenix_timeb itb;

	microtime(&tv);
	itb.time = tv.tv_sec;
	itb.millitm = (tv.tv_usec / 1000);
	itb.timezone = tz.tz_minuteswest;
	itb.dstflag = tz.tz_dsttime;
	return copyout((caddr_t)&itb, (caddr_t)SCARG(uap, tp), xenix_timeb_len);
}

int
ibcs2_sys_time(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ibcs2_sys_time_args /* {
		syscallarg(ibcs2_time_t *) tp;
	} */ *uap = v;
	struct timeval tv;

	microtime(&tv);
	*retval = tv.tv_sec;
	if (SCARG(uap, tp))
		return copyout((caddr_t)&tv.tv_sec, (caddr_t)SCARG(uap, tp),
			       sizeof(ibcs2_time_t));
	else
		return 0;
}

int
ibcs2_sys_pathconf(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ibcs2_sys_pathconf_args /* {
		syscallarg(char *) path;
		syscallarg(int) name;
	} */ *uap = v;
	SCARG(uap, name)++;	/* iBCS2 _PC_* defines are offset by one */
        return sys_pathconf(p, uap, retval);
}

int
ibcs2_sys_fpathconf(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ibcs2_sys_fpathconf_args /* {
		syscallarg(int) fd;
		syscallarg(int) name;
	} */ *uap = v;
	SCARG(uap, name)++;	/* iBCS2 _PC_* defines are offset by one */
        return sys_fpathconf(p, uap, retval);
}

int
ibcs2_sys_sysconf(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ibcs2_sys_sysconf_args /* {
		syscallarg(int) name;
	} */ *uap = v;
	int mib[2], value, len, error;
	struct sys___sysctl_args sa;
	struct sys_getrlimit_args ga;

	switch(SCARG(uap, name)) {
	case IBCS2_SC_ARG_MAX:
		mib[1] = KERN_ARGMAX;
		break;

	case IBCS2_SC_CHILD_MAX:
	    {
		caddr_t sg = stackgap_init(p->p_emul);

		SCARG(&ga, which) = RLIMIT_NPROC;
		SCARG(&ga, rlp) = stackgap_alloc(&sg, sizeof(struct rlimit *));
		if (error = sys_getrlimit(p, &ga, retval))
			return error;
		*retval = SCARG(&ga, rlp)->rlim_cur;
		return 0;
	    }

	case IBCS2_SC_CLK_TCK:
		*retval = hz;
		return 0;

	case IBCS2_SC_NGROUPS_MAX:
		mib[1] = KERN_NGROUPS;
		break;

	case IBCS2_SC_OPEN_MAX:
	    {
		caddr_t sg = stackgap_init(p->p_emul);

		SCARG(&ga, which) = RLIMIT_NOFILE;
		SCARG(&ga, rlp) = stackgap_alloc(&sg, sizeof(struct rlimit *));
		if (error = sys_getrlimit(p, &ga, retval))
			return error;
		*retval = SCARG(&ga, rlp)->rlim_cur;
		return 0;
	    }
		
	case IBCS2_SC_JOB_CONTROL:
		mib[1] = KERN_JOB_CONTROL;
		break;
		
	case IBCS2_SC_SAVED_IDS:
		mib[1] = KERN_SAVED_IDS;
		break;
		
	case IBCS2_SC_VERSION:
		mib[1] = KERN_POSIX1;
		break;
		
	case IBCS2_SC_PASS_MAX:
		*retval = 128;		/* XXX - should we create PASS_MAX ? */
		return 0;

	case IBCS2_SC_XOPEN_VERSION:
		*retval = 2;		/* XXX: What should that be? */
		return 0;
		
	default:
		return EINVAL;
	}

	mib[0] = CTL_KERN;
	len = sizeof(value);
	SCARG(&sa, name) = mib;
	SCARG(&sa, namelen) = 2;
	SCARG(&sa, old) = &value;
	SCARG(&sa, oldlenp) = &len;
	SCARG(&sa, new) = NULL;
	SCARG(&sa, newlen) = 0;
	if (error = sys___sysctl(p, &sa, retval))
		return error;
	*retval = value;
	return 0;
}

int
ibcs2_sys_alarm(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ibcs2_sys_alarm_args /* {
		syscallarg(unsigned) sec;
	} */ *uap = v;
	int error;
        struct itimerval *itp, *oitp;
	struct sys_setitimer_args sa;
	caddr_t sg = stackgap_init(p->p_emul);

        itp = stackgap_alloc(&sg, sizeof(*itp));
	oitp = stackgap_alloc(&sg, sizeof(*oitp));
        timerclear(&itp->it_interval);
        itp->it_value.tv_sec = SCARG(uap, sec);
        itp->it_value.tv_usec = 0;

	SCARG(&sa, which) = ITIMER_REAL;
	SCARG(&sa, itv) = itp;
	SCARG(&sa, oitv) = oitp;
        error = sys_setitimer(p, &sa, retval);
	if (error)
		return error;
        if (oitp->it_value.tv_usec)
                oitp->it_value.tv_sec++;
        *retval = oitp->it_value.tv_sec;
        return 0;
}

int
ibcs2_sys_getmsg(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ibcs2_sys_getmsg_args /* {
		syscallarg(int) fd;
		syscallarg(struct ibcs2_stropts *) ctl;
		syscallarg(struct ibcs2_stropts *) dat;
		syscallarg(int *) flags;
	} */ *uap = v;

	return 0;
}

int
ibcs2_sys_putmsg(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ibcs2_sys_putmsg_args /* {
		syscallarg(int) fd;
		syscallarg(struct ibcs2_stropts *) ctl;
		syscallarg(struct ibcs2_stropts *) dat;
		syscallarg(int) flags;
	} */ *uap = v;

	return 0;
}

int
ibcs2_sys_times(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ibcs2_sys_times_args /* {
		syscallarg(struct tms *) tp;
	} */ *uap = v;
	int error;
	struct sys_getrusage_args ga;
	struct tms tms;
        struct timeval t;
	caddr_t sg = stackgap_init(p->p_emul);
        struct rusage *ru = stackgap_alloc(&sg, sizeof(*ru));
#define CONVTCK(r)      (r.tv_sec * hz + r.tv_usec / (1000000 / hz))

	SCARG(&ga, who) = RUSAGE_SELF;
	SCARG(&ga, rusage) = ru;
	error = sys_getrusage(p, &ga, retval);
	if (error)
                return error;
        tms.tms_utime = CONVTCK(ru->ru_utime);
        tms.tms_stime = CONVTCK(ru->ru_stime);

	SCARG(&ga, who) = RUSAGE_CHILDREN;
        error = sys_getrusage(p, &ga, retval);
	if (error)
		return error;
        tms.tms_cutime = CONVTCK(ru->ru_utime);
        tms.tms_cstime = CONVTCK(ru->ru_stime);

	microtime(&t);
        *retval = CONVTCK(t);
	
	return copyout((caddr_t)&tms, (caddr_t)SCARG(uap, tp),
		       sizeof(struct tms));
}

int
ibcs2_sys_stime(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ibcs2_sys_stime_args /* {
		syscallarg(long *) timep;
	} */ *uap = v;
	int error;
	struct sys_settimeofday_args sa;
	caddr_t sg = stackgap_init(p->p_emul);

	SCARG(&sa, tv) = stackgap_alloc(&sg, sizeof(*SCARG(&sa, tv)));
	SCARG(&sa, tzp) = NULL;
	if (error = copyin((caddr_t)SCARG(uap, timep),
			   &(SCARG(&sa, tv)->tv_sec), sizeof(long)))
		return error;
	SCARG(&sa, tv)->tv_usec = 0;
	if (error = sys_settimeofday(p, &sa, retval))
		return EPERM;
	return 0;
}

int
ibcs2_sys_utime(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ibcs2_sys_utime_args /* {
		syscallarg(char *) path;
		syscallarg(struct ibcs2_utimbuf *) buf;
	} */ *uap = v;
	int error;
	struct sys_utimes_args sa;
	struct timeval *tp;
	caddr_t sg = stackgap_init(p->p_emul);

        IBCS2_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));
	SCARG(&sa, path) = SCARG(uap, path);
	if (SCARG(uap, buf)) {
		struct ibcs2_utimbuf ubuf;

		if (error = copyin((caddr_t)SCARG(uap, buf), (caddr_t)&ubuf,
				   sizeof(ubuf)))
			return error;
		SCARG(&sa, tptr) = stackgap_alloc(&sg,
						  2 * sizeof(struct timeval *));
		tp = (struct timeval *)SCARG(&sa, tptr);
		tp->tv_sec = ubuf.actime;
		tp->tv_usec = 0;
		tp++;
		tp->tv_sec = ubuf.modtime;
		tp->tv_usec = 0;
	} else
		SCARG(&sa, tptr) = NULL;
	return sys_utimes(p, &sa, retval);
}

int
ibcs2_sys_nice(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ibcs2_sys_nice_args /* {
		syscallarg(int) incr;
	} */ *uap = v;
	int error, cur_nice = p->p_nice;
	struct sys_setpriority_args sa;

	SCARG(&sa, which) = PRIO_PROCESS;
	SCARG(&sa, who) = 0;
	SCARG(&sa, prio) = p->p_nice + SCARG(uap, incr);
	if (error = sys_setpriority(p, &sa, retval))
		return EPERM;
	*retval = p->p_nice;
	return 0;
}

/*
 * iBCS2 getpgrp, setpgrp, setsid, and setpgid
 */

int
ibcs2_sys_pgrpsys(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ibcs2_sys_pgrpsys_args /* {
		syscallarg(int) type;
		syscallarg(caddr_t) dummy;
		syscallarg(int) pid;
		syscallarg(int) pgid;
	} */ *uap = v;
	switch (SCARG(uap, type)) {
	case 0:			/* getpgrp */
		*retval = p->p_pgrp->pg_id;
		return 0;

	case 1:			/* setpgrp */
	    {
		struct sys_setpgid_args sa;

		SCARG(&sa, pid) = 0;
		SCARG(&sa, pgid) = 0;
		sys_setpgid(p, &sa, retval);
		*retval = p->p_pgrp->pg_id;
		return 0;
	    }

	case 2:			/* setpgid */
	    {
		struct sys_setpgid_args sa;

		SCARG(&sa, pid) = SCARG(uap, pid);
		SCARG(&sa, pgid) = SCARG(uap, pgid);
		return sys_setpgid(p, &sa, retval);
	    }

	case 3:			/* setsid */
		return sys_setsid(p, NULL, retval);

	default:
		return EINVAL;
	}
}

/*
 * XXX - need to check for nested calls
 */

int
ibcs2_sys_plock(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ibcs2_sys_plock_args /* {
		syscallarg(int) cmd;
	} */ *uap = v;
	int error;
#define IBCS2_UNLOCK	0
#define IBCS2_PROCLOCK	1
#define IBCS2_TEXTLOCK	2
#define IBCS2_DATALOCK	4

	
        if (error = suser(p->p_ucred, &p->p_acflag))
                return EPERM;
	switch(SCARG(uap, cmd)) {
	case IBCS2_UNLOCK:
	case IBCS2_PROCLOCK:
	case IBCS2_TEXTLOCK:
	case IBCS2_DATALOCK:
		return 0;	/* XXX - TODO */
	}
	return EINVAL;
}

int
ibcs2_sys_uadmin(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ibcs2_sys_uadmin_args /* {
		syscallarg(int) cmd;
		syscallarg(int) func;
		syscallarg(caddr_t) data;
	} */ *uap = v;
	int error;

#define SCO_A_REBOOT        1
#define SCO_A_SHUTDOWN      2
#define SCO_A_REMOUNT       4
#define SCO_A_CLOCK         8
#define SCO_A_SETCONFIG     128
#define SCO_A_GETDEV        130

#define SCO_AD_HALT         0
#define SCO_AD_BOOT         1
#define SCO_AD_IBOOT        2
#define SCO_AD_PWRDOWN      3
#define SCO_AD_PWRNAP       4

#define SCO_AD_PANICBOOT    1

#define SCO_AD_GETBMAJ      0
#define SCO_AD_GETCMAJ      1

	/* XXX: is this the right place for this call? */
	if (error = suser(p->p_ucred, &p->p_acflag))
		return (error);

	switch(SCARG(uap, cmd)) {
	case SCO_A_REBOOT:
	case SCO_A_SHUTDOWN:
		switch(SCARG(uap, func)) {
		case SCO_AD_HALT:
		case SCO_AD_PWRDOWN:
		case SCO_AD_PWRNAP:
			boot(RB_HALT);
		case SCO_AD_BOOT:
		case SCO_AD_IBOOT:
			boot(RB_AUTOBOOT);
		}
		return EINVAL;
	case SCO_A_REMOUNT:
	case SCO_A_CLOCK:
	case SCO_A_SETCONFIG:
		return 0;
	case SCO_A_GETDEV:
		return EINVAL;	/* XXX - TODO */
	}
	return EINVAL;
}

int
ibcs2_sys_sysfs(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ibcs2_sys_sysfs_args /* {
		syscallarg(int) cmd;
		syscallarg(caddr_t) d1;
		syscallarg(char *) buf;
	} */ *uap = v;

#define IBCS2_GETFSIND        1
#define IBCS2_GETFSTYP        2
#define IBCS2_GETNFSTYP       3

	switch(SCARG(uap, cmd)) {
	case IBCS2_GETFSIND:
	case IBCS2_GETFSTYP:
	case IBCS2_GETNFSTYP:
	}
	return EINVAL;		/* XXX - TODO */
}

int
ibcs2_sys_poll(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ibcs2_sys_poll_args /* {
		syscallarg(struct ibcs2_pollfd *) fds;
		syscallarg(long) nfds;
		syscallarg(int) timeout;
	} */ *uap = v;

	return EINVAL;		/* XXX - TODO */
}

int
xenix_sys_rdchk(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct xenix_sys_rdchk_args /* {
		syscallarg(int) fd;
	} */ *uap = v;
	int error;
	struct sys_ioctl_args sa;
	caddr_t sg = stackgap_init(p->p_emul);

	SCARG(&sa, fd) = SCARG(uap, fd);
	SCARG(&sa, com) = FIONREAD;
	SCARG(&sa, data) = stackgap_alloc(&sg, sizeof(int));
	if (error = sys_ioctl(p, &sa, retval))
		return error;
	*retval = (*((int*)SCARG(&sa, data))) ? 1 : 0;
	return 0;
}

int
xenix_sys_chsize(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct xenix_sys_chsize_args /* {
		syscallarg(int) fd;
		syscallarg(long) size;
	} */ *uap = v;
	struct sys_ftruncate_args sa;

	SCARG(&sa, fd) = SCARG(uap, fd);
	SCARG(&sa, pad) = 0;
	SCARG(&sa, length) = SCARG(uap, size);
	return sys_ftruncate(p, &sa, retval);
}

int
xenix_sys_nap(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct xenix_sys_nap_args /* {
		syscallarg(int) millisec;
	} */ *uap = v;

	return ENOSYS;
}

int
ibcs2_sys_unlink(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ibcs2_sys_unlink_args /* {
		syscallarg(char *) path;
	} */ *uap = v;
        caddr_t sg = stackgap_init(p->p_emul);

	IBCS2_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));
	return sys_unlink(p, uap, retval);
}

int
ibcs2_sys_chdir(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ibcs2_sys_chdir_args /* {
		syscallarg(char *) path;
	} */ *uap = v;
        caddr_t sg = stackgap_init(p->p_emul);

	IBCS2_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));
	return sys_chdir(p, uap, retval);
}

int
ibcs2_sys_chmod(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ibcs2_sys_chmod_args /* {
		syscallarg(char *) path;
		syscallarg(int) mode;
	} */ *uap = v;
        caddr_t sg = stackgap_init(p->p_emul);

	IBCS2_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));
	return sys_chmod(p, uap, retval);
}

int
ibcs2_sys_chown(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ibcs2_sys_chown_args /* {
		syscallarg(char *) path;
		syscallarg(int) uid;
		syscallarg(int) gid;
	} */ *uap = v;
        caddr_t sg = stackgap_init(p->p_emul);

	IBCS2_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));
	return sys_chown(p, uap, retval);
}

int
ibcs2_sys_rmdir(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ibcs2_sys_rmdir_args /* {
		syscallarg(char *) path;
	} */ *uap = v;
        caddr_t sg = stackgap_init(p->p_emul);

	IBCS2_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));
	return sys_rmdir(p, uap, retval);
}

int
ibcs2_sys_mkdir(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ibcs2_sys_mkdir_args /* {
		syscallarg(char *) path;
		syscallarg(int) mode;
	} */ *uap = v;
        caddr_t sg = stackgap_init(p->p_emul);

	IBCS2_CHECK_ALT_CREAT(p, &sg, SCARG(uap, path));
	return sys_mkdir(p, uap, retval);
}

int
ibcs2_sys_symlink(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ibcs2_sys_symlink_args /* {
		syscallarg(char *) path;
		syscallarg(char *) link;
	} */ *uap = v;
        caddr_t sg = stackgap_init(p->p_emul);

	IBCS2_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));
	IBCS2_CHECK_ALT_CREAT(p, &sg, SCARG(uap, link));
	return sys_symlink(p, uap, retval);
}

int
ibcs2_sys_rename(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ibcs2_sys_rename_args /* {
		syscallarg(char *) from;
		syscallarg(char *) to;
	} */ *uap = v;
        caddr_t sg = stackgap_init(p->p_emul);

	IBCS2_CHECK_ALT_EXIST(p, &sg, SCARG(uap, from));
	IBCS2_CHECK_ALT_CREAT(p, &sg, SCARG(uap, to));
	return sys_rename(p, uap, retval);
}

int
ibcs2_sys_readlink(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ibcs2_sys_readlink_args /* {
		syscallarg(char *) path;
		syscallarg(char *) buf;
		syscallarg(int) count;
	} */ *uap = v;
        caddr_t sg = stackgap_init(p->p_emul);

	IBCS2_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));
	return sys_readlink(p, uap, retval);
}

int
ibcs2_sysi86(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct ibcs2_sysi86_args /* {
		syscallarg(int) cmd;
		syscallarg(int) arg;
	} */ *uap = v;
	int val, error;

	switch (SCARG(uap, cmd)) {
	case IBCS2_SI86FPHW:
		val = IBCS2_FP_NO;
#ifdef MATH_EMULATE
		val = IBCS2_FP_SW;
#else
		val = IBCS2_FP_387;		/* a real coprocessor */
#endif
		if ((error = copyout((caddr_t)&val, (caddr_t)SCARG(uap, arg),
				     sizeof(val))))
			return error;
		break;

	case IBCS2_SI86STIME:		/* XXX - not used much, if at all */
	case IBCS2_SI86SETNAME:
		return EINVAL;

	case IBCS2_SI86PHYSMEM:
                *retval = ctob(physmem);
		break;

	default:
		return EINVAL;
	}
	return 0;
}
