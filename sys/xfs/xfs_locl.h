/*
 * Copyright (c) 1995 - 2000 Kungliga Tekniska Högskolan
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

/* $Id: xfs_locl.h,v 1.4 2001/06/27 04:58:49 art Exp $ */

#ifdef HAVE_CONFIG_H
#include <config.h>
#else
#include <xfs/xfs_config.h>
#endif

#ifndef RCSID
#define RCSID(x)
#endif

#ifdef __osf__

#ifdef __GNUC__
#define asm __foo_asm
#endif
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/uio.h>
#include <machine/cpu.h>
#include <sys/conf.h>
#include <sys/sysconfig.h>
#include <sys/file.h>
#include <sys/malloc.h>
#include <sys/poll.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/vfs_proto.h>
#include <io/common/devdriver.h>
#include <vm/vm_page.h>
#include <vm/vm_vppage.h>
#include <vm/vm_ubc.h>

typedef short int16_t;
typedef unsigned short u_int16_t;
typedef int int32_t;
typedef unsigned int u_int32_t;

#define VT_AFS VT_ADDON
#define MOUNT_XFS MOUNT_PC

typedef struct nameidata xfs_componentname;

/* XXX this is gross, but makes the code considerably more readable */
#if 0
#define componentname	nameidata
#endif

#define cn_nameptr	ni_ptr
#define cn_namelen	ni_namelen
#define cn_hash		ni_hash
#define cn_cred		ni_cred
#define cn_nameiop	ni_nameiop
#define cn_flags	ni_flags

#define mnt_stat m_stat
#define mnt_flag m_flag

#define NDINIT(ndp, op, flags, segflg, namep, p)	\
	(ndp)->ni_nameiop = (op) | (flags);		\
	(ndp)->ni_segflg = segflg;			\
	(ndp)->ni_dirp = namep;

#define LOCKLEAF 0

#define FFLAGS(mode) ((mode) - FOPEN)

/* 4.4BSD vput does VOP_UNLOCK + vrele, but it seems as if we only
   should do a vrele here */
#define vput(VP) vrele(VP)

#define xfs_uio_to_proc(uiop) (u.u_procp)
#define xfs_cnp_to_proc(cnp) (u.u_procp)
#define xfs_proc_to_cred(p) ((p)->p_rcred)
#define xfs_proc_to_ruid(p) ((p)->p_ruid)

#define xfs_curproc() (u.u_procp)

#define xfs_vop_read VOP_READ
#define xfs_vop_write VOP_WRITE
#define xfs_vop_getattr(t, attr, cred, proc, error) VOP_GETATTR((t), (attr), (cred), (error))
#define xfs_vop_access(dvp, mode, cred, proc, error) VOP_ACCESS((dvp), (mode), (cred), (error))

struct vop_generic_args;

#else /* !__osf__ */

typedef struct componentname xfs_componentname;

#include <sys/types.h>
#include <sys/param.h>
#if 0
#include <sys/ioctl.h>
#endif
#include <sys/proc.h>
#include <sys/filedesc.h>
#include <sys/kernel.h>
#ifdef HAVE_SYS_MODULE_H
#include <sys/module.h>
#endif
#include <sys/systm.h>
#include <sys/fcntl.h>
#ifdef HAVE_SYS_SYSPROTO_H
#include <sys/sysproto.h>
#endif
#include <sys/conf.h>
#include <sys/mount.h>
#include <sys/exec.h>
#ifdef HAVE_SYS_SYSENT_H
#include <sys/sysent.h>
#endif
#ifdef HAVE_SYS_LKM_H
#include <sys/lkm.h>
#endif
#include <sys/errno.h>
#include <sys/file.h>
#include <sys/namei.h>
#include <sys/dirent.h>
#include <sys/ucred.h>
#include <sys/select.h>
#include <sys/uio.h>
#ifdef HAVE_SYS_POLL_H
#include <sys/poll.h>
#endif
#ifdef HAVE_SYS_SIGNALVAR_H
#include <sys/signalvar.h>
#endif
#include <sys/syscall.h>
#include <sys/queue.h>
#include <sys/malloc.h>
#ifdef HAVE_SYS_SYSCALLARGS_H
#include <sys/syscallargs.h>
#endif
#ifdef HAVE_SYS_ATTR_H
#include <sys/attr.h>
#endif

#ifdef HAVE_MISCFS_GENFS_GENFS_H
#include <miscfs/genfs/genfs.h>
#endif
#ifdef HAVE_VM_VM_H
#include <vm/vm.h>
#endif
#ifdef HAVE_VM_VM_EXTERN_H
#include <vm/vm_extern.h>
#endif
#ifdef HAVE_VM_VM_ZONE_H
#include <vm/vm_zone.h>
#endif
#ifdef HAVE_VM_VM_OBJECT_H
#include <vm/vm_object.h>
#endif
#ifdef HAVE_UVM_UVM_EXTERN_H
#include <uvm/uvm_extern.h>
#endif

#if defined(__APPLE__)
#include <machine/machine_routines.h>
#include <mach/machine/vm_types.h>
void cache_purge(struct vnode *);
int cache_lookup(struct vnode *, struct vnode **, struct componentname *);
void cache_enter(struct vnode *, struct vnode *, struct componentname *);
void cache_purgevfs(struct mount *);
#endif

#define xfs_uio_to_proc(uiop) ((uiop)->uio_procp)
#define xfs_cnp_to_proc(cnp) ((cnp)->cn_proc)
#define xfs_proc_to_cred(p) ((p)->p_ucred)
#define xfs_proc_to_ruid(p) ((p)->p_cred->p_ruid)

#ifdef __APPLE__
#define xfs_curproc() (current_proc())
#else
#define xfs_curproc() (curproc)
#endif

#define xfs_vop_read(t, uio, ioflag, cred, error) (error) = VOP_READ((t), (uio), (ioflag), (cred))
#define xfs_vop_write(t, uio, ioflag, cred, error) (error) = VOP_WRITE((t), (uio), (ioflag), (cred))
#define xfs_vop_getattr(t, attr, cred, proc, error) (error) = VOP_GETATTR((t), (attr), (cred), (proc))
#define xfs_vop_access(dvp, mode, cred, proc, error) (error) = VOP_ACCESS((dvp), (mode), (cred), (proc))

#endif /* !__osf__ */

/*
 * XXX
 */

#ifndef SCARG
#define SCARG(a, b) ((a)->b.datum)
#define syscallarg(x)   union { x datum; register_t pad; }
#endif

#ifndef syscallarg
#define syscallarg(x)   x
#endif

#ifndef HAVE_REGISTER_T
typedef int register_t;
#endif

#if defined(HAVE_DEF_STRUCT_SETGROUPS_ARGS)
#define xfs_setgroups_args setgroups_args
#elif defined(HAVE_DEF_STRUCT_SYS_SETGROUPS_ARGS)
#define xfs_setgroups_args sys_setgroups_args
#elif __osf__
struct xfs_setgroups_args {
    syscallarg(int) gidsetsize;
    syscallarg(gid_t) *gidset;
};
#elif defined(__APPLE__)
struct xfs_setgroups_args{
        syscallarg(u_int)   gidsetsize;
        syscallarg(gid_t)   *gidset;
};
#else
#error what is you setgroups named ?
#endif


#ifdef HAVE_KERNEL_VFS_GETVFS
#define xfs_vfs_getvfs vfs_getvfs
#else
#define xfs_vfs_getvfs getvfs
#endif

#ifdef HAVE_FOUR_ARGUMENT_VFS_OBJECT_CREATE
#define xfs_vfs_object_create(vp,proc,ucred) vfs_object_create(vp,proc,ucred,TRUE)
#else
#define xfs_vfs_object_create(vp,proc,ucred) vfs_object_create(vp,proc,ucred)
#endif

#ifdef __OpenBSD__
#define xfs_set_vp_size(vp, sz) uvm_vnp_setsize(vp, sz)
#elif HAVE_KERNEL_VNODE_PAGER_SETSIZE
#define xfs_set_vp_size(vp, sz) vnode_pager_setsize(vp, sz)
#else
#define xfs_set_vp_size(vp, sz)
#endif

#include <xfs/xfs_syscalls.h>

/* 
 *  The VOP table
 *
 *    What VOPs do we have today ? 
 */

#include "xfs/xfs_vopdefs.h"
