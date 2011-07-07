/*	$OpenBSD: linux_file.c,v 1.25 2011/07/07 01:19:39 tedu Exp $	*/
/*	$NetBSD: linux_file.c,v 1.15 1996/05/20 01:59:09 fvdl Exp $	*/

/*
 * Copyright (c) 1995 Frank van der Linden
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
 *      This product includes software developed for the NetBSD Project
 *      by Frank van der Linden
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
#include <sys/signalvar.h>
#include <sys/uio.h>
#include <sys/malloc.h>
#include <sys/vnode.h>
#include <sys/tty.h>
#include <sys/conf.h>

#include <sys/syscallargs.h>

#include <compat/linux/linux_types.h>
#include <compat/linux/linux_signal.h>
#include <compat/linux/linux_syscallargs.h>
#include <compat/linux/linux_fcntl.h>
#include <compat/linux/linux_util.h>

#include <machine/linux_machdep.h>

static int linux_to_bsd_ioflags(int);
static int bsd_to_linux_ioflags(int);
static void bsd_to_linux_flock(struct flock *, struct linux_flock *);
static void linux_to_bsd_flock(struct linux_flock *, struct flock *);
static void bsd_to_linux_stat(struct stat *, struct linux_stat *);
static int linux_stat1(struct proc *, void *, register_t *, int);


/*
 * Some file-related calls are handled here. The usual flag conversion
 * an structure conversion is done, and alternate emul path searching.
 */

/*
 * The next two functions convert between the Linux and OpenBSD values
 * of the flags used in open(2) and fcntl(2).
 */
static int
linux_to_bsd_ioflags(lflags)
	int lflags;
{
	int res = 0;

	res |= cvtto_bsd_mask(lflags, LINUX_O_WRONLY, O_WRONLY);
	res |= cvtto_bsd_mask(lflags, LINUX_O_RDONLY, O_RDONLY);
	res |= cvtto_bsd_mask(lflags, LINUX_O_RDWR, O_RDWR);
	res |= cvtto_bsd_mask(lflags, LINUX_O_CREAT, O_CREAT);
	res |= cvtto_bsd_mask(lflags, LINUX_O_EXCL, O_EXCL);
	res |= cvtto_bsd_mask(lflags, LINUX_O_NOCTTY, O_NOCTTY);
	res |= cvtto_bsd_mask(lflags, LINUX_O_TRUNC, O_TRUNC);
	res |= cvtto_bsd_mask(lflags, LINUX_O_NDELAY, O_NDELAY);
	res |= cvtto_bsd_mask(lflags, LINUX_O_SYNC, O_SYNC);
	res |= cvtto_bsd_mask(lflags, LINUX_FASYNC, O_ASYNC);
	res |= cvtto_bsd_mask(lflags, LINUX_O_APPEND, O_APPEND);

	return res;
}

static int
bsd_to_linux_ioflags(bflags)
	int bflags;
{
	int res = 0;

	res |= cvtto_linux_mask(bflags, O_WRONLY, LINUX_O_WRONLY);
	res |= cvtto_linux_mask(bflags, O_RDONLY, LINUX_O_RDONLY);
	res |= cvtto_linux_mask(bflags, O_RDWR, LINUX_O_RDWR);
	res |= cvtto_linux_mask(bflags, O_CREAT, LINUX_O_CREAT);
	res |= cvtto_linux_mask(bflags, O_EXCL, LINUX_O_EXCL);
	res |= cvtto_linux_mask(bflags, O_NOCTTY, LINUX_O_NOCTTY);
	res |= cvtto_linux_mask(bflags, O_TRUNC, LINUX_O_TRUNC);
	res |= cvtto_linux_mask(bflags, O_NDELAY, LINUX_O_NDELAY);
	res |= cvtto_linux_mask(bflags, O_SYNC, LINUX_O_SYNC);
	res |= cvtto_linux_mask(bflags, O_ASYNC, LINUX_FASYNC);
	res |= cvtto_linux_mask(bflags, O_APPEND, LINUX_O_APPEND);

	return res;
}

/*
 * creat(2) is an obsolete function, but it's present as a Linux
 * system call, so let's deal with it.
 *
 * Just call open(2) with the TRUNC, CREAT and WRONLY flags.
 */
int
linux_sys_creat(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_creat_args /* {
		syscallarg(char *) path;
		syscallarg(int) mode;
	} */ *uap = v;
	struct sys_open_args oa;
	caddr_t sg;

	sg = stackgap_init(p->p_emul);
	LINUX_CHECK_ALT_CREAT(p, &sg, SCARG(uap, path));

	SCARG(&oa, path) = SCARG(uap, path);
	SCARG(&oa, flags) = O_CREAT | O_TRUNC | O_WRONLY;
	SCARG(&oa, mode) = SCARG(uap, mode);

	return sys_open(p, &oa, retval);
}

/*
 * open(2). Take care of the different flag values, and let the
 * OpenBSD syscall do the real work. See if this operation
 * gives the current process a controlling terminal.
 * (XXX is this necessary?)
 */
int
linux_sys_open(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_open_args /* {
		syscallarg(char *) path;
		syscallarg(int) flags;
		syscallarg(int) mode;
	} */ *uap = v;
	int error, fl;
	struct sys_open_args boa;
	caddr_t sg;

	sg = stackgap_init(p->p_emul);

	fl = linux_to_bsd_ioflags(SCARG(uap, flags));

	if (fl & O_CREAT)
		LINUX_CHECK_ALT_CREAT(p, &sg, SCARG(uap, path));
	else
		LINUX_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));

	SCARG(&boa, path) = SCARG(uap, path);
	SCARG(&boa, flags) = fl;
	SCARG(&boa, mode) = SCARG(uap, mode);

	if ((error = sys_open(p, &boa, retval)))
		return error;

	/*
	 * this bit from sunos_misc.c (and svr4_fcntl.c).
	 * If we are a session leader, and we don't have a controlling
	 * terminal yet, and the O_NOCTTY flag is not set, try to make
	 * this the controlling terminal.
	 */ 
        if (!(fl & O_NOCTTY) && SESS_LEADER(p->p_p) &&
	    !(p->p_p->ps_flags & PS_CONTROLT)) {
                struct filedesc *fdp = p->p_fd;
                struct file     *fp;

		if ((fp = fd_getfile(fdp, *retval)) == NULL)
			return (EBADF);
		FREF(fp);
                if (fp->f_type == DTYPE_VNODE)
                        (fp->f_ops->fo_ioctl) (fp, TIOCSCTTY, (caddr_t) 0, p);
		FRELE(fp);
        }
	return 0;
}

int
linux_sys_lseek(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_lseek_args /* {
		syscallarg(int) fd;
		syscallarg(long) offset;
		syscallarg(int) whence;
	} */ *uap = v;
	struct sys_lseek_args bla;

	SCARG(&bla, fd) = SCARG(uap, fd);
	SCARG(&bla, offset) = SCARG(uap, offset);
	SCARG(&bla, whence) = SCARG(uap, whence);

	return sys_lseek(p, &bla, retval);
}

/*
 * This appears to be part of a Linux attempt to switch to 64 bits file sizes.
 */
int
linux_sys_llseek(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_llseek_args /* {
		syscallarg(int) fd;
		syscallarg(uint32_t) ohigh;
		syscallarg(uint32_t) olow;
		syscallarg(caddr_t) res;
		syscallarg(int) whence;
	} */ *uap = v;
	struct sys_lseek_args bla;
	int error;
	off_t off;

	off = SCARG(uap, olow) | (((off_t) SCARG(uap, ohigh)) << 32);

	SCARG(&bla, fd) = SCARG(uap, fd);
	SCARG(&bla, offset) = off;
	SCARG(&bla, whence) = SCARG(uap, whence);

	if ((error = sys_lseek(p, &bla, retval)))
		return error;

	if ((error = copyout(retval, SCARG(uap, res), sizeof (off_t))))
		return error;

	retval[0] = 0;
	return 0;
}

/*
 * The next two functions take care of converting the flock
 * structure back and forth between Linux and OpenBSD format.
 * The only difference in the structures is the order of
 * the fields, and the 'whence' value.
 */
static void
bsd_to_linux_flock(bfp, lfp)
	struct flock *bfp;
	struct linux_flock *lfp;
{

	lfp->l_start = bfp->l_start;
	lfp->l_len = bfp->l_len;
	lfp->l_pid = bfp->l_pid;
	lfp->l_whence = bfp->l_whence;
	switch (bfp->l_type) {
	case F_RDLCK:
		lfp->l_type = LINUX_F_RDLCK;
		break;
	case F_UNLCK:
		lfp->l_type = LINUX_F_UNLCK;
		break;
	case F_WRLCK:
		lfp->l_type = LINUX_F_WRLCK;
		break;
	}
}

static void
linux_to_bsd_flock(lfp, bfp)
	struct linux_flock *lfp;
	struct flock *bfp;
{

	bfp->l_start = lfp->l_start;
	bfp->l_len = lfp->l_len;
	bfp->l_pid = lfp->l_pid;
	bfp->l_whence = lfp->l_whence;
	switch (lfp->l_type) {
	case LINUX_F_RDLCK:
		bfp->l_type = F_RDLCK;
		break;
	case LINUX_F_UNLCK:
		bfp->l_type = F_UNLCK;
		break;
	case LINUX_F_WRLCK:
		bfp->l_type = F_WRLCK;
		break;
	}
}

/*
 * Most actions in the fcntl() call are straightforward; simply
 * pass control to the OpenBSD system call. A few commands need
 * conversions after the actual system call has done its work,
 * because the flag values and lock structure are different.
 */
int
linux_sys_fcntl(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_fcntl_args /* {
		syscallarg(int) fd;
		syscallarg(int) cmd;
		syscallarg(void *) arg;
	} */ *uap = v;
	int fd, cmd, error, val;
	caddr_t arg, sg;
	struct linux_flock lfl;
	struct flock *bfp, bfl;
	struct sys_fcntl_args fca;
	struct filedesc *fdp;
	struct file *fp;
	struct vnode *vp;
	struct vattr va;
	long pgid;
	struct pgrp *pgrp;
	struct tty *tp, *(*d_tty)(dev_t);

	fd = SCARG(uap, fd);
	cmd = SCARG(uap, cmd);
	arg = (caddr_t) SCARG(uap, arg);

	switch (cmd) {
	case LINUX_F_DUPFD:
		cmd = F_DUPFD;
		break;
	case LINUX_F_GETFD:
		cmd = F_GETFD;
		break;
	case LINUX_F_SETFD:
		cmd = F_SETFD;
		break;
	case LINUX_F_GETFL:
		SCARG(&fca, fd) = fd;
		SCARG(&fca, cmd) = F_GETFL;
		SCARG(&fca, arg) = arg;
		if ((error = sys_fcntl(p, &fca, retval)))
			return error;
		retval[0] = bsd_to_linux_ioflags(retval[0]);
		return 0;
	case LINUX_F_SETFL:
		val = linux_to_bsd_ioflags((int)SCARG(uap, arg));
		SCARG(&fca, fd) = fd;
		SCARG(&fca, cmd) = F_SETFL;
		SCARG(&fca, arg) = (caddr_t) val;
		return sys_fcntl(p, &fca, retval);
	case LINUX_F_GETLK:
		sg = stackgap_init(p->p_emul);
		if ((error = copyin(arg, &lfl, sizeof lfl)))
			return error;
		linux_to_bsd_flock(&lfl, &bfl);
		bfp = (struct flock *) stackgap_alloc(&sg, sizeof *bfp);
		SCARG(&fca, fd) = fd;
		SCARG(&fca, cmd) = F_GETLK;
		SCARG(&fca, arg) = bfp;
		if ((error = copyout(&bfl, bfp, sizeof bfl)))
			return error;
		if ((error = sys_fcntl(p, &fca, retval)))
			return error;
		if ((error = copyin(bfp, &bfl, sizeof bfl)))
			return error;
		bsd_to_linux_flock(&bfl, &lfl);
		error = copyout(&lfl, arg, sizeof lfl);
		return error;
		break;
	case LINUX_F_SETLK:
	case LINUX_F_SETLKW:
		cmd = (cmd == LINUX_F_SETLK ? F_SETLK : F_SETLKW);
		if ((error = copyin(arg, &lfl, sizeof lfl)))
			return error;
		linux_to_bsd_flock(&lfl, &bfl);
		sg = stackgap_init(p->p_emul);
		bfp = (struct flock *) stackgap_alloc(&sg, sizeof *bfp);
		if ((error = copyout(&bfl, bfp, sizeof bfl)))
			return error;
		SCARG(&fca, fd) = fd;
		SCARG(&fca, cmd) = cmd;
		SCARG(&fca, arg) = bfp;
		return sys_fcntl(p, &fca, retval);
		break;
	case LINUX_F_SETOWN:
	case LINUX_F_GETOWN:	
		/*
		 * We need to route around the normal fcntl() for these calls,
		 * since it uses TIOC{G,S}PGRP, which is too restrictive for
		 * Linux F_{G,S}ETOWN semantics. For sockets, this problem
		 * does not exist.
		 */
		fdp = p->p_fd;
		if ((fp = fd_getfile(fdp, fd)) == NULL)
			return (EBADF);
		if (fp->f_type == DTYPE_SOCKET) {
			cmd = cmd == LINUX_F_SETOWN ? F_SETOWN : F_GETOWN;
			break;
		}
		vp = (struct vnode *)fp->f_data;
		if (vp->v_type != VCHR)
			return EINVAL;
		FREF(fp);
		error = VOP_GETATTR(vp, &va, p->p_ucred, p);
		FRELE(fp);
		if (error)
			return error;
		d_tty = cdevsw[major(va.va_rdev)].d_tty;
		if (!d_tty || (!(tp = (*d_tty)(va.va_rdev))))
			return EINVAL;
		if (cmd == LINUX_F_GETOWN) {
			retval[0] = tp->t_pgrp ? tp->t_pgrp->pg_id : NO_PID;
			return 0;
		}
		if ((long)arg <= 0) {
			pgid = -(long)arg;
		} else {
			struct process *pr = prfind((long)arg);
			if (pr == 0)
				return (ESRCH);
			pgid = (long)pr->ps_pgrp->pg_id;
		}
		pgrp = pgfind(pgid);
		if (pgrp == NULL || pgrp->pg_session != p->p_p->ps_session)
			return EPERM;
		tp->t_pgrp = pgrp;
		return 0;
	default:
		return EOPNOTSUPP;
	}

	SCARG(&fca, fd) = fd;
	SCARG(&fca, cmd) = cmd;
	SCARG(&fca, arg) = arg;

	return sys_fcntl(p, &fca, retval);
}

/*
 * Convert a OpenBSD stat structure to a Linux stat structure.
 * Only the order of the fields and the padding in the structure
 * is different. linux_fakedev is a machine-dependent function
 * which optionally converts device driver major/minor numbers
 * (XXX horrible, but what can you do against code that compares
 * things against constant major device numbers? sigh)
 */
static void
bsd_to_linux_stat(bsp, lsp)
	struct stat *bsp;
	struct linux_stat *lsp;
{

	lsp->lst_dev     = bsp->st_dev;
	lsp->lst_ino     = bsp->st_ino;
	lsp->lst_mode    = bsp->st_mode;
	lsp->lst_nlink   = bsp->st_nlink;
	lsp->lst_uid     = bsp->st_uid;
	lsp->lst_gid     = bsp->st_gid;
	lsp->lst_rdev    = linux_fakedev(bsp->st_rdev);
	lsp->lst_size    = bsp->st_size;
	lsp->lst_blksize = bsp->st_blksize;
	lsp->lst_blocks  = bsp->st_blocks;
	lsp->lst_atime   = bsp->st_atime;
	lsp->lst_mtime   = bsp->st_mtime;
	lsp->lst_ctime   = bsp->st_ctime;
}

/*
 * The stat functions below are plain sailing. stat and lstat are handled
 * by one function to avoid code duplication.
 */
int
linux_sys_fstat(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_fstat_args /* {
		syscallarg(int) fd;
		syscallarg(linux_stat *) sp;
	} */ *uap = v;
	struct sys_fstat_args fsa;
	struct linux_stat tmplst;
	struct stat *st,tmpst;
	caddr_t sg;
	int error;

	sg = stackgap_init(p->p_emul);

	st = stackgap_alloc(&sg, sizeof (struct stat));

	SCARG(&fsa, fd) = SCARG(uap, fd);
	SCARG(&fsa, sb) = st;

	if ((error = sys_fstat(p, &fsa, retval)))
		return error;

	if ((error = copyin(st, &tmpst, sizeof tmpst)))
		return error;

	bsd_to_linux_stat(&tmpst, &tmplst);

	if ((error = copyout(&tmplst, SCARG(uap, sp), sizeof tmplst)))
		return error;

	return 0;
}

static int
linux_stat1(p, v, retval, dolstat)
	struct proc *p;
	void *v;
	register_t *retval;
	int dolstat;
{
	struct sys_stat_args sa;
	struct linux_stat tmplst;
	struct stat *st, tmpst;
	caddr_t sg;
	int error;
	struct linux_sys_stat_args *uap = v;

	sg = stackgap_init(p->p_emul);

	st = stackgap_alloc(&sg, sizeof (struct stat));
	LINUX_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));

	SCARG(&sa, ub) = st;
	SCARG(&sa, path) = SCARG(uap, path);

	if ((error = (dolstat ? sys_lstat(p, &sa, retval) :
				sys_stat(p, &sa, retval))))
		return error;

	if ((error = copyin(st, &tmpst, sizeof tmpst)))
		return error;

	bsd_to_linux_stat(&tmpst, &tmplst);

	if ((error = copyout(&tmplst, SCARG(uap, sp), sizeof tmplst)))
		return error;

	return 0;
}

int
linux_sys_stat(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_stat_args /* {
		syscallarg(char *) path;
		syscallarg(struct linux_stat *) sp;
	} */ *uap = v;

	return linux_stat1(p, uap, retval, 0);
}

int
linux_sys_lstat(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_lstat_args /* {
		syscallarg(char *) path;
		syscallarg(struct linux_stat *) sp;
	} */ *uap = v;

	return linux_stat1(p, uap, retval, 1);
}

/*
 * The following syscalls are mostly here because of the alternate path check.
 */
int
linux_sys_access(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_access_args /* {
		syscallarg(char *) path;
		syscallarg(int) flags;
	} */ *uap = v;
	caddr_t sg = stackgap_init(p->p_emul);

	LINUX_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));

	return sys_access(p, uap, retval);
}

int
linux_sys_unlink(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;

{
	struct linux_sys_unlink_args /* {
		syscallarg(char *) path;
	} */ *uap = v;
	caddr_t sg = stackgap_init(p->p_emul);

	LINUX_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));

	return sys_unlink(p, uap, retval);
}

int
linux_sys_chdir(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_chdir_args /* {
		syscallarg(char *) path;
	} */ *uap = v;
	caddr_t sg = stackgap_init(p->p_emul);

	LINUX_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));

	return sys_chdir(p, uap, retval);
}

int
linux_sys_mknod(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_mknod_args /* {
		syscallarg(char *) path;
		syscallarg(int) mode;
		syscallarg(int) dev;
	} */ *uap = v;
	caddr_t sg = stackgap_init(p->p_emul);
	struct sys_mkfifo_args bma;

	LINUX_CHECK_ALT_CREAT(p, &sg, SCARG(uap, path));

	/*
	 * BSD handles FIFOs separately
	 */
	if (S_ISFIFO(SCARG(uap, mode))) {
		SCARG(&bma, path) = SCARG(uap, path);
		SCARG(&bma, mode) = SCARG(uap, mode);
		return sys_mkfifo(p, uap, retval);
	} else
		return sys_mknod(p, uap, retval);
}

int
linux_sys_chmod(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_chmod_args /* {
		syscallarg(char *) path;
		syscallarg(int) mode;
	} */ *uap = v;
	caddr_t sg = stackgap_init(p->p_emul);

	LINUX_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));

	return sys_chmod(p, uap, retval);
}

int
linux_sys_chown16(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_chown16_args /* {
		syscallarg(char *) path;
		syscallarg(int) uid;
		syscallarg(int) gid;
	} */ *uap = v;
	struct sys_chown_args bca;
	caddr_t sg = stackgap_init(p->p_emul);

	LINUX_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));

	SCARG(&bca, path) = SCARG(uap, path);
	SCARG(&bca, uid) = ((linux_uid_t)SCARG(uap, uid) == (linux_uid_t)-1) ?
		(uid_t)-1 : SCARG(uap, uid);
	SCARG(&bca, gid) = ((linux_gid_t)SCARG(uap, gid) == (linux_gid_t)-1) ?
		(gid_t)-1 : SCARG(uap, gid);
	
	return sys_chown(p, &bca, retval);
}

int
linux_sys_fchown16(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_fchown16_args /* {
		syscallarg(int) fd;
		syscallarg(int) uid;
		syscallarg(int) gid;
	} */ *uap = v;
	struct sys_fchown_args bfa;

	SCARG(&bfa, fd) = SCARG(uap, fd);
	SCARG(&bfa, uid) = ((linux_uid_t)SCARG(uap, uid) == (linux_uid_t)-1) ?
		(uid_t)-1 : SCARG(uap, uid);
	SCARG(&bfa, gid) = ((linux_gid_t)SCARG(uap, gid) == (linux_gid_t)-1) ?
		(gid_t)-1 : SCARG(uap, gid);
	
	return sys_fchown(p, &bfa, retval);
}

int
linux_sys_lchown16(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_lchown16_args /* {
		syscallarg(char *) path;
		syscallarg(int) uid;
		syscallarg(int) gid;
	} */ *uap = v;
	struct sys_lchown_args bla;

	SCARG(&bla, path) = SCARG(uap, path);
	SCARG(&bla, uid) = ((linux_uid_t)SCARG(uap, uid) == (linux_uid_t)-1) ?
		(uid_t)-1 : SCARG(uap, uid);
	SCARG(&bla, gid) = ((linux_gid_t)SCARG(uap, gid) == (linux_gid_t)-1) ?
		(gid_t)-1 : SCARG(uap, gid);

	return sys_lchown(p, &bla, retval);
}

int
linux_sys_rename(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_rename_args /* {
		syscallarg(char *) from;
		syscallarg(char *) to;
	} */ *uap = v;
	caddr_t sg = stackgap_init(p->p_emul);

	LINUX_CHECK_ALT_EXIST(p, &sg, SCARG(uap, from));
	LINUX_CHECK_ALT_CREAT(p, &sg, SCARG(uap, to));

	return sys_rename(p, uap, retval);
}

int
linux_sys_mkdir(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_mkdir_args /* {
		syscallarg(char *) path;
		syscallarg(int) mode;
	} */ *uap = v;
	caddr_t sg = stackgap_init(p->p_emul);

	LINUX_CHECK_ALT_CREAT(p, &sg, SCARG(uap, path));

	return sys_mkdir(p, uap, retval);
}

int
linux_sys_rmdir(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_rmdir_args /* {
		syscallarg(char *) path;
	} */ *uap = v;
	caddr_t sg = stackgap_init(p->p_emul);

	LINUX_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));

	return sys_rmdir(p, uap, retval);
}

int
linux_sys_symlink(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_symlink_args /* {
		syscallarg(char *) path;
		syscallarg(char *) to;
	} */ *uap = v;
	caddr_t sg = stackgap_init(p->p_emul);

	LINUX_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));
	LINUX_CHECK_ALT_CREAT(p, &sg, SCARG(uap, to));

	return sys_symlink(p, uap, retval);
}

int
linux_sys_readlink(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_readlink_args /* {
		syscallarg(char *) name;
		syscallarg(char *) buf;
		syscallarg(int) count;
	} */ *uap = v;
	caddr_t sg = stackgap_init(p->p_emul);

	LINUX_CHECK_ALT_EXIST(p, &sg, SCARG(uap, name));

	return sys_readlink(p, uap, retval);
}

int
linux_sys_ftruncate(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_ftruncate_args /* {
		syscallarg(int)  fd;
		syscallarg(long) length;
	} */ *uap = v;
	struct sys_ftruncate_args sta;

	SCARG(&sta, fd) = SCARG(uap, fd);
	SCARG(&sta, length) = SCARG(uap, length);

	return sys_ftruncate(p, uap, retval);
}

int
linux_sys_truncate(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_truncate_args /* {
		syscallarg(char *) path;
		syscallarg(long) length;
	} */ *uap = v;
	caddr_t sg = stackgap_init(p->p_emul);
	struct sys_truncate_args sta;

	LINUX_CHECK_ALT_EXIST(p, &sg, SCARG(uap, path));

	SCARG(&sta, path) = SCARG(uap, path);
	SCARG(&sta, length) = SCARG(uap, length);

	return sys_truncate(p, &sta, retval);
}

/*
 * This is just fsync() for now (just as it is in the Linux kernel)
 */
int
linux_sys_fdatasync(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_fdatasync_args /* {
		syscallarg(int) fd;
	} */ *uap = v;
	return sys_fsync(p, uap, retval);
}

/*
 * pread(2).
 */
int
linux_sys_pread(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_pread_args /* {
		syscallarg(int) fd;
		syscallarg(void *) buf;
		syscallarg(size_t) nbyte;
		syscallarg(linux_off_t) offset;
	} */ *uap = v;
	struct sys_pread_args pra;
	
	SCARG(&pra, fd) = SCARG(uap, fd);
	SCARG(&pra, buf) = SCARG(uap, buf);
	SCARG(&pra, nbyte) = SCARG(uap, nbyte);
	SCARG(&pra, offset) = SCARG(uap, offset);
	
	return sys_pread(p, &pra, retval);
}

/*
 * pwrite(2).
 */
int
linux_sys_pwrite(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct linux_sys_pwrite_args /* {
		syscallarg(int) fd;
		syscallarg(char *) buf;
		syscallarg(size_t) nbyte;
		syscallarg(linux_off_t) offset;
	} */ *uap = v;
	struct sys_pwrite_args pra;

	SCARG(&pra, fd) = SCARG(uap, fd);
	SCARG(&pra, buf) = SCARG(uap, buf);
	SCARG(&pra, nbyte) = SCARG(uap, nbyte);
	SCARG(&pra, offset) = SCARG(uap, offset);

	return sys_pwrite(p, &pra, retval);
}
