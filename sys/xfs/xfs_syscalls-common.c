/*
 * Copyright (c) 1995 - 2002 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <xfs/xfs_locl.h>

RCSID("$arla: xfs_syscalls-common.c,v 1.72 2003/01/19 20:53:49 lha Exp $");

/*
 * NNPFS system calls.
 */

#include <xfs/xfs_syscalls.h>
#include <xfs/xfs_message.h>
#include <xfs/xfs_fs.h>
#include <xfs/xfs_dev.h>
#include <xfs/xfs_node.h>
#include <xfs/xfs_vfsops.h>
#include <xfs/xfs_deb.h>

/* Misc syscalls */
#ifdef HAVE_SYS_IOCCOM_H
#include <sys/ioccom.h>
#elif defined(HAVE_SYS_IOCTL_H)
#include <sys/ioctl.h>
#endif
/*
 * XXX - horrible kludge.  If we are openbsd and not building an lkm,
 *     then use their headerfile.
 */
#if (defined(__OpenBSD__) || defined(__NetBSD__)) && !defined(_LKM)
#define NNPFS_NOT_LKM 1
#elif defined(__FreeBSD__) && !defined(KLD_MODULE)
#define NNPFS_NOT_LKM 1
#endif

#ifdef NNPFS_NOT_LKM
#include <xfs/xfs_pioctl.h>
#else
#include <kafs.h>
#endif

int (*old_setgroups_func)(syscall_d_thread_t *p, void *v, register_t *retval);

#if defined(__FreeBSD__) && __FreeBSD_version >= 500026
/*
 * XXX This is wrong
 */
static struct ucred *
xfs_crcopy(struct ucred *cr)
{
    struct ucred *ncr;

    if (crshared(cr)) {
	ncr = crdup(cr);
	crfree(cr);
	return ncr;
    }
    return cr;
}
#else
#define xfs_crcopy crcopy
#endif


/*
 * the syscall entry point
 */

#ifdef NNPFS_NOT_LKM
int
sys_xfspioctl(syscall_d_thread_t *proc, void *varg, register_t *return_value)
#else
int
xfspioctl(syscall_d_thread_t *proc, void *varg, register_t *return_value)
#endif
{
#ifdef NNPFS_NOT_LKM
    struct sys_xfspioctl_args *arg = (struct sys_xfspioctl_args *) varg;
#else
    struct sys_pioctl_args *arg = (struct sys_pioctl_args *) varg;
#endif
    int error = EINVAL;

    switch (SCARG(arg, operation)) {
    case AFSCALL_PIOCTL:
	error = xfs_pioctl_call(syscall_thread_to_thread(proc),
				  varg, return_value);
	break;
    case AFSCALL_SETPAG:
#ifdef HAVE_FREEBSD_THREAD
	error = xfs_setpag_call(&xfs_thread_to_cred(proc));
#else
	error = xfs_setpag_call(&xfs_proc_to_cred(syscall_thread_to_thread(proc)));
#endif
	break;
    default:
	NNPFSDEB(XDEBSYS, ("Unimplemeted xfspioctl: %d\n",
			 SCARG(arg, operation)));
	error = EINVAL;
	break;
    }

    return error;
}

/*
 * Def pag:
 *  33536 <= g0 <= 34560
 *  32512 <= g1 <= 48896
 */

#define NNPFS_PAG1_LLIM 33536
#define NNPFS_PAG1_ULIM 34560
#define NNPFS_PAG2_LLIM 32512
#define NNPFS_PAG2_ULIM 48896

static gid_t pag_part_one = NNPFS_PAG1_LLIM;
static gid_t pag_part_two = NNPFS_PAG2_LLIM;

/*
 * Is `cred' member of a PAG?
 */

static int
xfs_is_pag(struct ucred *cred)
{
    /* The first group is the gid of the user ? */

    if (cred->cr_ngroups >= 3 &&
	cred->cr_groups[1] >= NNPFS_PAG1_LLIM &&
	cred->cr_groups[1] <= NNPFS_PAG1_ULIM &&
	cred->cr_groups[2] >= NNPFS_PAG2_LLIM &&
	cred->cr_groups[2] <= NNPFS_PAG2_ULIM)
	return 1;
    else
	return 0;
}

/*
 * Return the pag used by `cred'
 */

xfs_pag_t
xfs_get_pag(struct ucred *cred)
{
    if (xfs_is_pag(cred)) {

	return (((cred->cr_groups[1] << 16) & 0xFFFF0000) |
		((cred->cr_groups[2] & 0x0000FFFF)));

    } else
	return cred->cr_uid;	       /* XXX */
}

/*
 * Set the pag in `ret_cred' and return a new cred.
 */

static int
store_pag (struct ucred **ret_cred, gid_t part1, gid_t part2)
{
    struct ucred *cred = *ret_cred;

    if (!xfs_is_pag (cred)) {
	int i;

	if (cred->cr_ngroups + 2 >= NGROUPS)
	    return E2BIG;

	cred = xfs_crcopy (cred);

	for (i = cred->cr_ngroups - 1; i > 0; i--) {
	    cred->cr_groups[i + 2] = cred->cr_groups[i];
	}
	cred->cr_ngroups += 2;
    } else {
	cred = xfs_crcopy (cred);
    }
    cred->cr_groups[1] = part1;
    cred->cr_groups[2] = part2;
    *ret_cred = cred;

    return 0;
}

/*
 * Acquire a new pag in `ret_cred'
 */

int
xfs_setpag_call(struct ucred **ret_cred)
{
    int ret;

    ret = store_pag (ret_cred, pag_part_one, pag_part_two++);
    if (ret)
	return ret;

    if (pag_part_two > NNPFS_PAG2_ULIM) {
	pag_part_one++;
	pag_part_two = NNPFS_PAG2_LLIM;
    }
    return 0;
}

#ifndef NNPFS_NOT_LKM
/*
 * remove a pag
 */

static int
xfs_unpag (struct ucred *cred)
{
    while (xfs_is_pag (cred)) {
	int i;

	for (i = 0; i < cred->cr_ngroups - 2; ++i)
	    cred->cr_groups[i] = cred->cr_groups[i+2];
	cred->cr_ngroups -= 2;
    }
    return 0;
}

/*
 * A wrapper around setgroups that preserves the pag.
 */

int
xfs_setgroups (syscall_d_thread_t *p,
	       void *varg,
	       register_t *retval)
{
    struct xfs_setgroups_args *uap = (struct xfs_setgroups_args *)varg;
#ifdef HAVE_FREEBSD_THREAD
    struct ucred **cred = &xfs_thread_to_cred(p);
#else
    struct ucred **cred = &xfs_proc_to_cred(syscall_thread_to_thread(p));
#endif

    if (xfs_is_pag (*cred)) {
	gid_t part1, part2;
	int ret;

	if (SCARG(uap,gidsetsize) + 2 > NGROUPS)
	    return EINVAL;

	part1 = (*cred)->cr_groups[1];
	part2 = (*cred)->cr_groups[2];
	ret = (*old_setgroups_func) (p, uap, retval);
	if (ret)
	    return ret;
	return store_pag (cred, part1, part2);
    } else {
	int ret;

	ret = (*old_setgroups_func) (p, uap, retval);
	/* don't support setting a PAG */
	if (xfs_is_pag (*cred)) {
	    xfs_unpag (*cred);
	    return EINVAL;
	}
	return ret;
    }
}
#endif /* !NNPFS_NOT_LKM */

/*
 * Return the vnode corresponding to `pathptr'
 */

static int
lookup_node (const char *pathptr,
	     int follow_links_p,
	     struct vnode **res,
	     d_thread_t *proc)
{
    int error;
    char path[MAXPATHLEN];
#ifdef __osf__
    struct nameidata *ndp = &u.u_nd;
#else
    struct nameidata nd, *ndp = &nd;
#endif
    struct vnode *vp;
    size_t count;

    NNPFSDEB(XDEBSYS, ("xfs_syscall: looking up: %lx\n",
		     (unsigned long)pathptr));

    error = copyinstr((char *) pathptr, path, MAXPATHLEN, &count);

    NNPFSDEB(XDEBSYS, ("xfs_syscall: looking up: %s, error: %d\n", path, error));

    if (error)
	return error;

    NDINIT(ndp, LOOKUP,
	   follow_links_p ? FOLLOW : 0,
	   UIO_SYSSPACE, path, proc);

    error = namei(ndp);
	
    if (error != 0) {
	NNPFSDEB(XDEBSYS, ("xfs_syscall: error during namei: %d\n", error));
	return EINVAL;
    }

    vp = ndp->ni_vp;

    *res = vp;
    return 0;
}

/*
 * implement xfs fhget in a way that should be compatible with the native
 * getfh
 */

static int
getfh_compat (d_thread_t *p,
	      struct ViceIoctl *vice_ioctl,
	      struct vnode *vp)
{
    /* This is to be same as getfh */
    fhandle_t fh;
    int error;
	
    bzero((caddr_t)&fh, sizeof(fh));
    fh.fh_fsid = vp->v_mount->mnt_stat.f_fsid;
#if __osf__
    VFS_VPTOFH(vp, &fh.fh_fid, error);
#else
    error = VFS_VPTOFH(vp, &fh.fh_fid);
#endif
    if (error)
	return error;

    if (vice_ioctl->out_size < sizeof(fh))
	return EINVAL;
	
    return copyout((caddr_t)&fh, vice_ioctl->out, sizeof (fh));
}

/*
 * implement xfs fhget by combining (dev, ino, generation)
 */

#ifndef __OpenBSD__
static int
trad_fhget (d_thread_t *p,
	    struct ViceIoctl *vice_ioctl,
	    struct vnode *vp)
{
    int error;
    struct mount *mnt;
    struct vattr vattr;
    size_t len;
    struct xfs_fhandle_t xfs_handle;
    struct xfs_fh_args fh_args;

#ifdef HAVE_FREEBSD_THREAD
    xfs_vop_getattr(vp, &vattr, xfs_thread_to_cred(p), p, error);
#else
    xfs_vop_getattr(vp, &vattr, xfs_proc_to_cred(p), p, error);
#endif
    if (error)
	return error;

    mnt = vp->v_mount;

    SCARG(&fh_args, fsid)   = mnt->mnt_stat.f_fsid;
    SCARG(&fh_args, fileid) = vattr.va_fileid;
    SCARG(&fh_args, gen)    = vattr.va_gen;
    
    xfs_handle.len = sizeof(fh_args);
    memcpy (xfs_handle.fhdata, &fh_args, sizeof(fh_args));
    len = sizeof(xfs_handle);

    if (vice_ioctl->out_size < len)
	return EINVAL;

    error = copyout (&xfs_handle, vice_ioctl->out, len);
    if (error) {
	NNPFSDEB(XDEBSYS, ("fhget_call: copyout failed: %d\n", error));
    }
    return error;
}
#endif  /* ! __OpenBSD__ */

/*
 * return file handle of `vp' in vice_ioctl->out
 * vp is vrele:d
 */

static int
fhget_call (d_thread_t *p,
	    struct ViceIoctl *vice_ioctl,
	    struct vnode *vp)
{
    int error;

    NNPFSDEB(XDEBSYS, ("fhget_call\n"));

    if (vp == NULL)
	return EBADF;

#if defined(__APPLE__) || defined(__osf__)
    error = EINVAL; /* XXX: Leaks vnodes if fhget/fhopen is used */
    goto out;
#endif

    error = xfs_suser (p);
    if (error)
	goto out;

#if (defined(HAVE_GETFH) && defined(HAVE_FHOPEN)) || defined(__osf__)
    error = getfh_compat (p, vice_ioctl, vp);
#else
    error = trad_fhget (p, vice_ioctl, vp);
#endif /* HAVE_GETFH && HAVE_FHOPEN */
out:
    vrele(vp);
    return error;
}

/*
 * open the file specified in `vice_ioctl->in'
 */

static int
fhopen_call (d_thread_t *p,
	     struct ViceIoctl *vice_ioctl,
	     struct vnode *vp,
	     int flags,
	     register_t *retval)
{

    NNPFSDEB(XDEBSYS, ("fhopen_call: flags = %d\n", flags));

    if (vp != NULL) {
	vrele (vp);
	return EINVAL;
    }

#if defined(__APPLE__) || defined(__osf__)
    return EINVAL; /* XXX: Leaks vnodes if fhget/fhopen is used */
#endif

    return xfs_fhopen (p,
		       (struct xfs_fhandle_t *)vice_ioctl->in,
		       flags,
		       retval);
}

/*
 * Send the pioctl to arlad
 */

static int
remote_pioctl (d_thread_t *p,
	       struct sys_pioctl_args *arg,
	       struct ViceIoctl *vice_ioctl,
	       struct vnode *vp)
{
    int error;
    struct xfs_message_pioctl msg;
    struct xfs_message_wakeup_data *msg2;

    if (vp != NULL) {
	struct xfs_node *xn;

	if (vp->v_tag != VT_AFS) {
	    NNPFSDEB(XDEBSYS, ("xfs_syscall: file is not in afs\n"));
	    vrele(vp);
	    return EINVAL;
	}

	xn = VNODE_TO_XNODE(vp);

	msg.handle = xn->handle;
	vrele(vp);
    }

    if (vice_ioctl->in_size < 0) {
	printf("xfs: remote pioctl: got a negative data size: opcode: %d",
	       SCARG(arg, a_opcode));
	return EINVAL;
    }

    if (vice_ioctl->in_size > NNPFS_MSG_MAX_DATASIZE) {
	printf("xfs_pioctl_call: got a humongous in packet: opcode: %d",
	       SCARG(arg, a_opcode));
	return EINVAL;
    }
    if (vice_ioctl->in_size != 0) {
	error = copyin(vice_ioctl->in, msg.msg, vice_ioctl->in_size);
	if (error)
	    return error;
    }

    msg.header.opcode = NNPFS_MSG_PIOCTL;
    msg.header.size = sizeof(msg);
    msg.opcode = SCARG(arg, a_opcode);

    msg.insize = vice_ioctl->in_size;
    msg.outsize = vice_ioctl->out_size;
#ifdef HAVE_FREEBSD_THREAD
    msg.cred.uid = xfs_thread_to_euid(p);
    msg.cred.pag = xfs_get_pag(xfs_thread_to_cred(p));
#else
    msg.cred.uid = xfs_proc_to_euid(p);
    msg.cred.pag = xfs_get_pag(xfs_proc_to_cred(p));
#endif

    error = xfs_message_rpc(0, &msg.header, sizeof(msg), p); /* XXX */
    msg2 = (struct xfs_message_wakeup_data *) &msg;

    if (error == 0)
	error = msg2->error;
    if (error == ENODEV)
	error = EINVAL;

    if (error == 0 && msg2->header.opcode == NNPFS_MSG_WAKEUP_DATA) {
	int len;

	len = msg2->len;
	if (len > vice_ioctl->out_size)
	    len = vice_ioctl->out_size;
	if (len > NNPFS_MSG_MAX_DATASIZE)
	    len = NNPFS_MSG_MAX_DATASIZE;
	if (len < 0)
	    len = 0;

	error = copyout(msg2->msg, vice_ioctl->out, len);
    }
    return error;
}

static int
xfs_debug (d_thread_t *p,
	   struct ViceIoctl *vice_ioctl)
{
    int32_t flags;
    int error;

    if (vice_ioctl->in_size != 0) {
	if (vice_ioctl->in_size < sizeof(int32_t))
	    return EINVAL;
	
	error = xfs_suser (p);
	if (error)
	    return error;

	error = copyin (vice_ioctl->in,
			&flags,
			sizeof(flags));
	if (error)
	    return error;
	
	xfsdeb = flags;
    }
    
    if (vice_ioctl->out_size != 0) {
	if (vice_ioctl->out_size < sizeof(int32_t))
	    return EINVAL;
	
	error = copyout (&xfsdeb,
			 vice_ioctl->out,
			 sizeof(int32_t));
	if (error)
	    return error;
    }

    return 0;
}


/*
 * Handle `pioctl'
 */

int
xfs_pioctl_call(d_thread_t *proc,
		struct sys_pioctl_args *arg,
		register_t *return_value)
{
    int error;
    struct ViceIoctl vice_ioctl;
    char *pathptr;
    struct vnode *vp = NULL;

    NNPFSDEB(XDEBSYS, ("xfs_syscall(%d, %lx, %d, %lx, %d)\n", 
		     SCARG(arg, operation),
		     (unsigned long)SCARG(arg, a_pathP),
		     SCARG(arg, a_opcode),
		     (unsigned long)SCARG(arg, a_paramsP),
		     SCARG(arg, a_followSymlinks)));

    /* Copy in the data structure for us */

    error = copyin(SCARG(arg, a_paramsP),
		   &vice_ioctl,
		   sizeof(vice_ioctl));

    if (error)
	return error;

    pathptr = SCARG(arg, a_pathP);

    if (pathptr != NULL) {
	error = lookup_node (pathptr, SCARG(arg, a_followSymlinks), &vp,
			     proc);
	if(error)
	    return error;
    }
	
    switch (SCARG(arg, a_opcode)) {
    case VIOC_FHGET :
	return fhget_call (proc, &vice_ioctl, vp);
    case VIOC_FHOPEN :
	return fhopen_call (proc, &vice_ioctl, vp,
			    SCARG(arg, a_followSymlinks), return_value);
    case VIOC_XFSDEBUG :
	if (vp != NULL)
	    vrele (vp);
	return xfs_debug (proc, &vice_ioctl);
    default :
	NNPFSDEB(XDEBSYS, ("a_opcode = %x\n", SCARG(arg, a_opcode)));
	return remote_pioctl (proc, arg, &vice_ioctl, vp);
    }
}
