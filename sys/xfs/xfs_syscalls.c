/*	$OpenBSD: xfs_syscalls.c,v 1.1 1998/08/30 16:47:21 art Exp $	*/
/*
 * Copyright (c) 1995, 1996, 1997, 1998 Kungliga Tekniska Högskolan
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

#include <sys/types.h>
#include <sys/param.h>
#include <sys/uio.h>
#include <sys/namei.h>
#include <sys/ucred.h>
#include <sys/mount.h>
#include <sys/signal.h>
#include <sys/syscall.h>
#include <sys/systm.h>
#include <sys/proc.h>

#include <xfs/xfs_common.h>

RCSID("$KTH: xfs_syscalls.c,v 1.20 1998/07/19 21:18:30 art Exp $");

/*
 * XFS system calls.
 */

#include <sys/xfs_message.h>
#include <xfs/xfs_syscalls.h>
#include <xfs/xfs_dev.h>
#include <xfs/xfs_node.h>
#include <xfs/xfs_deb.h>

/* Misc syscalls */
#include <sys/pioctl.h>
#include <sys/syscallargs.h>

#ifdef ACTUALLY_LKM_NOT_KERNEL
/* XXX really defined in kern/kern_lkm.c */
extern int sys_lkmnosys(struct proc *p, void *v, register_t *retval);

#ifndef SYS_MAXSYSCALL		       /* Workaround for OpenBSD */
#define SYS_MAXSYSCALL 255
#endif
#endif /* ACTUALLY_LKM_NOT_KERNEL */


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


pag_t
xfs_get_pag(struct ucred *cred)
{
    if (xfs_is_pag(cred)) {

	return (((cred->cr_groups[1] << 16) & 0xFFFF0000) |
		((cred->cr_groups[2] & 0x0000FFFF)));

    } else
	return cred->cr_uid;	       /* XXX */
}

static int
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

#ifdef ACTUALLY_LKM_NOT_KERNEL
#if defined(__NetBSD__) || defined(__OpenBSD__)

#define syscallarg(x)   union { x datum; register_t pad; }

struct sys_pioctl_args {
    syscallarg(int) operation;
    syscallarg(char *) a_pathP;
    syscallarg(int) a_opcode;
    syscallarg(struct ViceIoctl *) a_paramsP;
    syscallarg(int) a_followSymlinks;
};

#elif defined(__FreeBSD__)

struct sys_pioctl_args {
    int operation;
    char *a_pathP;
    int a_opcode;
    struct ViceIoctl *a_paramsP;
    int a_followSymlinks;
};

#ifndef SCARG
#define SCARG(a, b) (a->b)
#endif

#endif
#endif

static int
xfs_pioctl_call(struct proc *p, void *v, int *i)
{
    int error;
    struct ViceIoctl vice_ioctl;
    struct xfs_message_pioctl msg;
    struct xfs_message_wakeup_data *msg2;
    char *pathptr;

    struct sys_pioctl_args *arg = (struct sys_pioctl_args *) v;

    /* Copy in the data structure for us */

    error = copyin(SCARG(arg, a_paramsP),
		   &vice_ioctl,
		   sizeof(vice_ioctl));

    if (error)
	return error;

    if (vice_ioctl.in_size > 2048) {
	printf("xfs_pioctl_call: got a humongous in packet: opcode: %d",
	       SCARG(arg, a_opcode));
	return EINVAL;
    }
    if (vice_ioctl.in_size != 0) {
	error = copyin(vice_ioctl.in,
		       &msg.msg,
		       vice_ioctl.in_size);

	if (error)
	    return error;
    }

    pathptr = SCARG(arg, a_pathP);

    if (pathptr != NULL) {
	char path[MAXPATHLEN];
	struct xfs_node *xn;
	struct nameidata nd;
	struct vnode *vp;
	size_t done;

	XFSDEB(XDEBSYS, ("xfs_syscall: looking up: %p\n", pathptr));

	error = copyinstr(pathptr, path, MAXPATHLEN, &done);

	XFSDEB(XDEBSYS, ("xfs_syscall: looking up: %s len: %d error: %d\n", 
			 path, done, error));

	if (error)
	    return error;

	NDINIT(&nd, LOOKUP,
	       SCARG(arg, a_followSymlinks) ? FOLLOW : 0,
	       UIO_SYSSPACE, path, p);

	error = namei(&nd);
	
	if (error != 0) {
	    XFSDEB(XDEBSYS, ("xfs_syscall: error during namei: %d\n", error));
	    return EINVAL;
	}

	vp = nd.ni_vp;

	if (vp->v_tag != VT_AFS) {
	    XFSDEB(XDEBSYS, ("xfs_syscall: %s not in afs\n", path));
	    vrele(vp);
	    return EINVAL;
	}

	xn = VNODE_TO_XNODE(vp);

	msg.handle = xn->handle;
	vrele(vp);
    }

    msg.header.opcode = XFS_MSG_PIOCTL;
    msg.opcode = SCARG(arg, a_opcode);

    msg.insize = vice_ioctl.in_size;
    msg.outsize = vice_ioctl.out_size;
    msg.cred.uid = p->p_cred->p_ruid;
    msg.cred.pag = xfs_get_pag(p->p_ucred);

    error = xfs_message_rpc(0, &msg.header, sizeof(msg)); /* XXX */
    msg2 = (struct xfs_message_wakeup_data *) &msg;

    if (error == 0)
        error = msg2->error;
    else
        error = EINVAL; /* return EINVAL to not confuse applications */

    if (error == 0 && msg2->header.opcode == XFS_MSG_WAKEUP_DATA)
        error = copyout(msg2->msg, vice_ioctl.out,
			min(msg2->len, vice_ioctl.out_size));
    return error;
}


#ifdef ACTUALLY_LKM_NOT_KERNEL
static int
xfs_syscall(struct proc *p, void *v, int *i)
#else
int
sys_pioctl(struct proc *p, void *v, int *i)
#endif
{
    struct sys_pioctl_args *arg = (struct sys_pioctl_args *) v;
    int error = EINVAL;

    switch (SCARG(arg, operation)) {
    case AFSCALL_PIOCTL:
	error = xfs_pioctl_call(p, v, i);
	break;
    case AFSCALL_SETPAG:
	error = xfs_setpag_call(&p->p_cred->pc_ucred);
	break;
    default:
	uprintf("Unimplemeted call: %d\n", SCARG(arg, operation));
	error = EINVAL;
	break;
    }

    return error;
}

#ifdef ACTUALLY_LKM_NOT_KERNEL
#if defined(__NetBSD__) || defined(__OpenBSD__)

static int syscall_offset;
static struct sysent syscall_oldent;

static struct sysent xfs_syscallent = {
    4,				       /* number of args */
    sizeof(struct sys_pioctl_args),    /* size of args */
    xfs_syscall			       /* function pointer */
};

static int
find_first_free_syscall(int *ret)
{
    int i;

    /*
     * Search the table looking for a slot...
     */
    for (i = 0; i < SYS_MAXSYSCALL; i++)
	if (sysent[i].sy_call == sys_lkmnosys) {
	    *ret = i;
	    return 0;
	}
    return ENFILE;
}

int
xfs_install_syscalls(void)
{
    int error;

#ifdef AFS_SYSCALL
    syscall_offset = AFS_SYSCALL;
#else
    error = find_first_free_syscall(&syscall_offset);
    if (error)
	return error;
#endif

    syscall_oldent = sysent[syscall_offset];

    /* replace with new */

    sysent[syscall_offset] = xfs_syscallent;

    printf("syscall %d\n", syscall_offset);
    return 0;
}

int
xfs_uninstall_syscalls(void)
{
    /* replace current slot contents with old contents */
    if (syscall_offset)
	sysent[syscall_offset] = syscall_oldent;

    return 0;
}

int
xfs_stat_syscalls(void)
{
    return 0;
}

#elif defined(__FreeBSD__)

static int syscall_offset;
static struct sysent syscall_oldent;

static struct sysent xfs_syscallent = {
    4,
    xfs_syscall
};

static int
find_first_free_syscall(int *ret)
{
    int i;

    /*
     * Search the table looking for a slot...
     */
    for (i = 0; i < aout_sysvec.sv_size; i++)
	if (aout_sysvec.sv_table[i].sy_call == (sy_call_t *) lkmnosys) {
	    *ret = i;
	    return 0;
	}
    return ENFILE;
}

int
xfs_install_syscalls(void)
{
    int i;
    int error;

#ifdef AFS_SYSCALL
    i = AFS_SYSCALL;
#else
    error = find_first_free_syscall(&i);
    if (error)
	return error;
#endif

    syscall_oldent = aout_sysvec.sv_table[i];

    aout_sysvec.sv_table[i] = xfs_syscallent;

    syscall_offset = i;
    printf("syscall %d\n", i);
    return 0;
}

int
xfs_uninstall_syscalls(void)
{
    if (syscall_offset) {
	aout_sysvec.sv_table[syscall_offset].sy_call = (sy_call_t *) lkmnosys;
    }
    return 0;
}

int
xfs_stat_syscalls(void)
{
    return 0;
}

#endif
#endif /* ACTUALLY_LKM_NOT_KERNEL */
