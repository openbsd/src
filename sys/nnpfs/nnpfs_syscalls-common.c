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

#include <nnpfs/nnpfs_locl.h>

RCSID("$arla: nnpfs_syscalls-common.c,v 1.72 2003/01/19 20:53:49 lha Exp $");

/*
 * NNPFS system calls.
 */

#include <nnpfs/nnpfs_syscalls.h>
#include <nnpfs/nnpfs_message.h>
#include <nnpfs/nnpfs_fs.h>
#include <nnpfs/nnpfs_dev.h>
#include <nnpfs/nnpfs_node.h>
#include <nnpfs/nnpfs_vfsops.h>
#include <nnpfs/nnpfs_deb.h>

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
#include <nnpfs/nnpfs_pioctl.h>
#else
#include <kafs.h>
#endif

int (*old_setgroups_func)(syscall_d_thread_t *p, void *v, register_t *retval);

#if defined(__FreeBSD__) && __FreeBSD_version >= 500026
/*
 * XXX This is wrong
 */
static struct ucred *
nnpfs_crcopy(struct ucred *cr)
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
#define nnpfs_crcopy crcopy
#endif


/*
 * the syscall entry point
 */

#ifdef NNPFS_NOT_LKM
int
sys_nnpfspioctl(syscall_d_thread_t *proc, void *varg, register_t *return_value)
#else
int
nnpfspioctl(syscall_d_thread_t *proc, void *varg, register_t *return_value)
#endif
{
#ifdef NNPFS_NOT_LKM
    struct sys_nnpfspioctl_args *arg = (struct sys_nnpfspioctl_args *) varg;
#else
    struct sys_pioctl_args *arg = (struct sys_pioctl_args *) varg;
#endif
    int error = EINVAL;

    switch (SCARG(arg, operation)) {
    case AFSCALL_PIOCTL:
	error = nnpfs_pioctl_call(syscall_thread_to_thread(proc),
				  varg, return_value);
	break;
    case AFSCALL_SETPAG:
#ifdef HAVE_FREEBSD_THREAD
	error = nnpfs_setpag_call(&nnpfs_thread_to_cred(proc));
#else
	error = nnpfs_setpag_call(&nnpfs_proc_to_cred(syscall_thread_to_thread(proc)));
#endif
	break;
    default:
	NNPFSDEB(XDEBSYS, ("Unimplemeted nnpfspioctl: %d\n",
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
nnpfs_is_pag(struct ucred *cred)
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

nnpfs_pag_t
nnpfs_get_pag(struct ucred *cred)
{
    if (nnpfs_is_pag(cred)) {

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

    if (!nnpfs_is_pag (cred)) {
	int i;

	if (cred->cr_ngroups + 2 >= NGROUPS)
	    return E2BIG;

	cred = nnpfs_crcopy (cred);

	for (i = cred->cr_ngroups - 1; i > 0; i--) {
	    cred->cr_groups[i + 2] = cred->cr_groups[i];
	}
	cred->cr_ngroups += 2;
    } else {
	cred = nnpfs_crcopy (cred);
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
nnpfs_setpag_call(struct ucred **ret_cred)
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
nnpfs_unpag (struct ucred *cred)
{
    while (nnpfs_is_pag (cred)) {
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
nnpfs_setgroups (syscall_d_thread_t *p,
	       void *varg,
	       register_t *retval)
{
    struct nnpfs_setgroups_args *uap = (struct nnpfs_setgroups_args *)varg;
#ifdef HAVE_FREEBSD_THREAD
    struct ucred **cred = &nnpfs_thread_to_cred(p);
#else
    struct ucred **cred = &nnpfs_proc_to_cred(syscall_thread_to_thread(p));
#endif

    if (nnpfs_is_pag (*cred)) {
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
	if (nnpfs_is_pag (*cred)) {
	    nnpfs_unpag (*cred);
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

    NNPFSDEB(XDEBSYS, ("nnpfs_syscall: looking up: %lx\n",
		     (unsigned long)pathptr));

    error = copyinstr((char *) pathptr, path, MAXPATHLEN, &count);

    NNPFSDEB(XDEBSYS, ("nnpfs_syscall: looking up: %s, error: %d\n", path, error));

    if (error)
	return error;

    NDINIT(ndp, LOOKUP,
	   follow_links_p ? FOLLOW : 0,
	   UIO_SYSSPACE, path, proc);

    error = namei(ndp);
	
    if (error != 0) {
	NNPFSDEB(XDEBSYS, ("nnpfs_syscall: error during namei: %d\n", error));
	return EINVAL;
    }

    vp = ndp->ni_vp;

    *res = vp;
    return 0;
}

/*
 * implement nnpfs fhget in a way that should be compatible with the native
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
 * implement nnpfs fhget by combining (dev, ino, generation)
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
    struct nnpfs_fhandle_t nnpfs_handle;
    struct nnpfs_fh_args fh_args;

#ifdef HAVE_FREEBSD_THREAD
    nnpfs_vop_getattr(vp, &vattr, nnpfs_thread_to_cred(p), p, error);
#else
    nnpfs_vop_getattr(vp, &vattr, nnpfs_proc_to_cred(p), p, error);
#endif
    if (error)
	return error;

    mnt = vp->v_mount;

    SCARG(&fh_args, fsid)   = mnt->mnt_stat.f_fsid;
    SCARG(&fh_args, fileid) = vattr.va_fileid;
    SCARG(&fh_args, gen)    = vattr.va_gen;
    
    nnpfs_handle.len = sizeof(fh_args);
    memcpy (nnpfs_handle.fhdata, &fh_args, sizeof(fh_args));
    len = sizeof(nnpfs_handle);

    if (vice_ioctl->out_size < len)
	return EINVAL;

    error = copyout (&nnpfs_handle, vice_ioctl->out, len);
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

    error = nnpfs_suser (p);
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

    return nnpfs_fhopen (p,
		       (struct nnpfs_fhandle_t *)vice_ioctl->in,
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
    int error = 0;
    struct nnpfs_message_pioctl *msg = NULL;
    struct nnpfs_message_wakeup_data *msg2;

    msg = malloc(sizeof(*msg), M_TEMP, M_WAITOK | M_CANFAIL | M_ZERO);
    if (msg == NULL) {
        error = ENOMEM;
	goto done;
    }

    if (vp != NULL) {
	struct nnpfs_node *xn;

	if (vp->v_tag != VT_NNPFS) {
	    NNPFSDEB(XDEBSYS, ("nnpfs_syscall: file is not in afs\n"));
	    vrele(vp);
	    error = EINVAL;
	    goto done;
	}

	xn = VNODE_TO_XNODE(vp);

	msg->handle = xn->handle;
	vrele(vp);
    }

    if (vice_ioctl->in_size < 0) {
	printf("nnpfs: remote pioctl: got a negative data size: opcode: %d",
	       SCARG(arg, a_opcode));
	error = EINVAL;
	goto done;
    }

    if (vice_ioctl->in_size > NNPFS_MSG_MAX_DATASIZE) {
	printf("nnpfs_pioctl_call: got a humongous in packet: opcode: %d",
	       SCARG(arg, a_opcode));
	error = EINVAL;
	goto done;
    }
    if (vice_ioctl->in_size != 0) {
	error = copyin(vice_ioctl->in, msg->msg, vice_ioctl->in_size);
	if (error)
	  goto done;
    }

    msg->header.opcode = NNPFS_MSG_PIOCTL;
    msg->header.size = sizeof(*msg);
    msg->opcode = SCARG(arg, a_opcode);

    msg->insize = vice_ioctl->in_size;
    msg->outsize = vice_ioctl->out_size;
#ifdef HAVE_FREEBSD_THREAD
    msg->cred.uid = nnpfs_thread_to_euid(p);
    msg->cred.pag = nnpfs_get_pag(nnpfs_thread_to_cred(p));
#else
    msg->cred.uid = nnpfs_proc_to_euid(p);
    msg->cred.pag = nnpfs_get_pag(nnpfs_proc_to_cred(p));
#endif

    error = nnpfs_message_rpc(0, &(msg->header), sizeof(*msg), p); /* XXX */
    msg2 = (struct nnpfs_message_wakeup_data *) msg;

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
 done:
    if (msg != NULL)
        free(msg, M_TEMP);
    return error;
}

static int
nnpfs_debug (d_thread_t *p,
	   struct ViceIoctl *vice_ioctl)
{
    int32_t flags;
    int error;

    if (vice_ioctl->in_size != 0) {
	if (vice_ioctl->in_size < sizeof(int32_t))
	    return EINVAL;
	
	error = nnpfs_suser (p);
	if (error)
	    return error;

	error = copyin (vice_ioctl->in,
			&flags,
			sizeof(flags));
	if (error)
	    return error;
	
	nnpfsdeb = flags;
    }
    
    if (vice_ioctl->out_size != 0) {
	if (vice_ioctl->out_size < sizeof(int32_t))
	    return EINVAL;
	
	error = copyout (&nnpfsdeb,
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
nnpfs_pioctl_call(d_thread_t *proc,
		struct sys_pioctl_args *arg,
		register_t *return_value)
{
    int error;
    struct ViceIoctl vice_ioctl;
    char *pathptr;
    struct vnode *vp = NULL;

    NNPFSDEB(XDEBSYS, ("nnpfs_syscall(%d, %lx, %d, %lx, %d)\n", 
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
    case VIOC_NNPFSDEBUG :
	if (vp != NULL)
	    vrele (vp);
	return nnpfs_debug (proc, &vice_ioctl);
    default :
	NNPFSDEB(XDEBSYS, ("a_opcode = %x\n", SCARG(arg, a_opcode)));
	return remote_pioctl (proc, arg, &vice_ioctl, vp);
    }
}
