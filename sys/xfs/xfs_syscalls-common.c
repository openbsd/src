/*	$OpenBSD: xfs_syscalls-common.c,v 1.2 2000/03/03 00:54:58 todd Exp $	*/

/*
 * Copyright (c) 1995, 1996, 1997, 1998, 1999 Kungliga Tekniska Högskolan
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the Kungliga Tekniska
 *      Högskolan and its contributors.
 *
 * 4. Neither the name of the Institute nor the names of its contributors
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

RCSID("$OpenBSD: xfs_syscalls-common.c,v 1.2 2000/03/03 00:54:58 todd Exp $");

/*
 * XFS system calls.
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
#include <xfs/xfs_pioctl.h>

/*
 * the syscall entry point
 */

int
sys_xfspioctl(struct proc *proc, void *varg, register_t *return_value)
{
    struct sys_pioctl_args *arg = (struct sys_pioctl_args *) varg;
    int error = EINVAL;

    switch (SCARG(arg, operation)) {
    case AFSCALL_PIOCTL:
	error = xfs_pioctl_call(proc, varg, return_value);
	break;
    case AFSCALL_SETPAG:
	error = xfs_setpag_call(&xfs_proc_to_cred(proc));
	break;
    default:
	XFSDEB(XDEBSYS, ("Unimplemeted xfspioctl: %d\n",
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

#define XFS_PAG1_LLIM 33536
#define XFS_PAG1_ULIM 34560
#define XFS_PAG2_LLIM 32512
#define XFS_PAG2_ULIM 48896

static gid_t pag_part_one = XFS_PAG1_LLIM;
static gid_t pag_part_two = XFS_PAG2_LLIM;

/*
 * Is `cred' member of a PAG?
 */

static int
xfs_is_pag(struct ucred *cred)
{
    /* The first group is the gid of the user ? */

    if (cred->cr_ngroups >= 3 &&
	cred->cr_groups[1] >= XFS_PAG1_LLIM &&
	cred->cr_groups[1] <= XFS_PAG1_ULIM &&
	cred->cr_groups[2] >= XFS_PAG2_LLIM &&
	cred->cr_groups[2] <= XFS_PAG2_ULIM)
	return 1;
    else
	return 0;
}

/*
 * Return the pag used by `cred'
 */

pag_t
xfs_get_pag(struct ucred *cred)
{
    if (xfs_is_pag(cred)) {

	return (((cred->cr_groups[1] << 16) & 0xFFFF0000) |
		((cred->cr_groups[2] & 0x0000FFFF)));

    } else
	return cred->cr_uid;	       /* XXX */
}

/*
 * Acquire a new pag in `ret_cred'
 */

int
xfs_setpag_call(struct ucred **ret_cred)
{
    struct ucred *cred = *ret_cred;
    int i;

    if (!xfs_is_pag(cred)) {

	/* Check if it fits */
	if (cred->cr_ngroups + 2 >= NGROUPS)
	    return E2BIG;	       /* XXX Hmmm, better error ? */

	cred = crcopy (cred);

	/* Copy the groups */
	for (i = cred->cr_ngroups - 1; i > 0; i--) {
	    cred->cr_groups[i + 2] = cred->cr_groups[i];
	}
	cred->cr_ngroups += 2;

    } else
	cred = crcopy(cred);

    cred->cr_groups[1] = pag_part_one;
    cred->cr_groups[2] = pag_part_two++;

    if (pag_part_two > XFS_PAG2_ULIM) {
	pag_part_one++;
	pag_part_two = XFS_PAG2_LLIM;
    }
    *ret_cred = cred;
    return 0;
}

/*
 * Return the vnode corresponding to `pathptr'
 */

static int
lookup_node (const char *pathptr,
	     int follow_links_p,
	     struct vnode **res,
	     struct proc *proc)
{
    int error;
    char path[MAXPATHLEN];
#ifdef __osf__
    struct nameidata *ndp = &u.u_nd;
#else
    struct nameidata nd, *ndp = &nd;
#endif
    struct vnode *vp;
    size_t done;

    XFSDEB(XDEBSYS, ("xfs_syscall: looking up: %p\n", pathptr));

    error = copyinstr(pathptr, path, MAXPATHLEN, &done);

    XFSDEB(XDEBSYS, ("xfs_syscall: looking up: %s len: %lu error: %d\n", 
		     path, (unsigned long)done, error));

    if (error)
	return error;

    NDINIT(ndp, LOOKUP,
	   follow_links_p ? FOLLOW : 0,
	   UIO_SYSSPACE, path, proc);

    error = namei(ndp);
	
    if (error != 0) {
	XFSDEB(XDEBSYS, ("xfs_syscall: error during namei: %d\n", error));
	return EINVAL;
    }

    vp = ndp->ni_vp;

    *res = vp;
    return 0;
}

/*
 * return file handle of `vp' in vice_ioctl->out
 */

static int
fhget_call (struct proc *p,
	    struct ViceIoctl *vice_ioctl,
	    struct vnode *vp)
{
    int error;
    struct mount *mnt;
    struct vattr vattr;
    size_t len;
    struct xfs_fh_args fh_args;

    XFSDEB(XDEBSYS, ("fhget_call\n"));

    if (vp == NULL)
	return EBADF;

    error = suser (xfs_proc_to_cred(p), NULL);
    if (error)
	return error;

#ifdef __osf__
    VOP_GETATTR(vp, &vattr, p->p_rcred, error);
#else
    error = VOP_GETATTR(vp, &vattr, p->p_ucred, p);
#endif
    if (error)
	goto out;

    mnt = vp->v_mount;

    SCARG(&fh_args, fsid)   = mnt->mnt_stat.f_fsid;
    SCARG(&fh_args, fileid) = vattr.va_fileid;
    SCARG(&fh_args, gen)    = vattr.va_gen;
    
    len = sizeof(fh_args);

    if (vice_ioctl->out_size < len) {
	error = EINVAL;
	goto out;
    }

    error = copyout (&fh_args, vice_ioctl->out, len);
    if (error) {
	XFSDEB(XDEBSYS, ("fhget_call: copyout failed: %d\n", error));
    }

out:
    vrele (vp);
    return error;
}

/*
 * open the file specified in `vice_ioctl->in'
 */

static int
fhopen_call (struct proc *p,
	     struct ViceIoctl *vice_ioctl,
	     struct vnode *vp,
	     int flags,
	     register_t *retval)
{
    int error;
    struct xfs_fh_args fh_args;

    XFSDEB(XDEBSYS, ("fhopen_call: flags = %d\n", flags));

    if (vp != NULL) {
	vrele (vp);
	return EINVAL;
    }

    if (vice_ioctl->in_size < sizeof(fh_args))
	return EINVAL;

    error = copyin (vice_ioctl->in,
		    &fh_args,
		    sizeof(fh_args));
    if (error)
	return error;

    return xfs_fhopen (p,
		       SCARG(&fh_args, fsid),
		       SCARG(&fh_args, fileid),
		       SCARG(&fh_args, gen),
		       flags,
		       retval);
}

/*
 * Send the pioctl to arlad
 */

static int
remote_pioctl (struct proc *p,
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
	    XFSDEB(XDEBSYS, ("xfs_syscall: file is not in afs\n"));
	    vrele(vp);
	    return EINVAL;
	}

	xn = VNODE_TO_XNODE(vp);

	msg.handle = xn->handle;
	vrele(vp);
    }

    if (vice_ioctl->in_size > 2048) {
	printf("xfs_pioctl_call: got a humongous in packet: opcode: %d",
	       SCARG(arg, a_opcode));
	return EINVAL;
    }
    if (vice_ioctl->in_size != 0) {
	error = copyin(vice_ioctl->in,
		       &msg.msg,
		       vice_ioctl->in_size);

	if (error)
	    return error;
    }

    msg.header.opcode = XFS_MSG_PIOCTL;
    msg.opcode = SCARG(arg, a_opcode);

    msg.insize = vice_ioctl->in_size;
    msg.outsize = vice_ioctl->out_size;
#ifdef __osf__
    msg.cred.uid = p->p_ruid;
    msg.cred.pag = xfs_get_pag(p->p_rcred);
#else
    msg.cred.uid = p->p_cred->p_ruid;
    msg.cred.pag = xfs_get_pag(p->p_ucred);
#endif

    error = xfs_message_rpc(0, &msg.header, sizeof(msg)); /* XXX */
    msg2 = (struct xfs_message_wakeup_data *) &msg;

    if (error == 0)
	error = msg2->error;
    else
	error = EINVAL; /* return EINVAL to not confuse applications */

    if (error == 0 && msg2->header.opcode == XFS_MSG_WAKEUP_DATA)
	error = copyout(msg2->msg, vice_ioctl->out, 
			min(msg2->len, vice_ioctl->out_size));
    return error;
}

static int
xfs_debug (struct proc *p,
	   struct ViceIoctl *vice_ioctl)
{
    int32_t flags;
    int error;

    if (vice_ioctl->in_size != 0) {
	if (vice_ioctl->in_size < sizeof(int32_t))
	    return EINVAL;
	
	error = suser (xfs_proc_to_cred(p), NULL);
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
xfs_pioctl_call(struct proc *proc,
		struct sys_pioctl_args *arg,
		register_t *return_value)
{
    int error;
    struct ViceIoctl vice_ioctl;
    char *pathptr;
    struct vnode *vp = NULL;

    XFSDEB(XDEBSYS, ("xfs_syscall(%d, %p, %d, %p, %d)\n", 
		     SCARG(arg, operation),
		     SCARG(arg, a_pathP),
		     SCARG(arg, a_opcode),
		     SCARG(arg, a_paramsP),
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
    case VIOC_XFSDEBUG:
	return xfs_debug (proc, &vice_ioctl);
    default :
	XFSDEB(XDEBSYS, ("a_opcode = %x\n", SCARG(arg, a_opcode)));
	return remote_pioctl (proc, arg, &vice_ioctl, vp);
    }
}
