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

/* $arla: nnpfs_locl.h,v 1.72 2003/02/15 16:40:00 lha Exp $ */

#ifdef HAVE_CONFIG_H
#include <config.h>
#else
#include <nnpfs/nnpfs_config.h>
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
typedef unsigned short uint16_t;
typedef int int32_t;
typedef unsigned int uint32_t;

#define VT_AFS VT_ADDON
#define MOUNT_NNPFS MOUNT_PC

typedef struct nameidata nnpfs_componentname;

/* XXX this is gross, but makes the code considerably more readable */
#if 0
#define componentname	nameidata
#endif

#define cn_nameptr	ni_ptr
#define cn_namelen	ni_namelen
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

#define nnpfs_uio_to_proc(uiop) (u.u_procp)
#define nnpfs_cnp_to_proc(cnp) (u.u_procp)
#define nnpfs_proc_to_cred(p) ((p)->p_rcred)
#define nnpfs_proc_to_euid(p) ((p)->p_rcred->cr_uid)

#define nnpfs_curproc() (u.u_procp)

#define nnpfs_vop_read VOP_READ
#define nnpfs_vop_write VOP_WRITE
#define nnpfs_vop_getattr(t, attr, cred, proc, error) VOP_GETATTR((t), (attr), (cred), (error))
#define nnpfs_vop_access(dvp, mode, cred, proc, error) VOP_ACCESS((dvp), (mode), (cred), (error))

struct vop_generic_args;

typedef u_long va_size_t;

#else /* !__osf__ */

typedef struct componentname nnpfs_componentname;

#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
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
#ifdef HAVE_SYS_LOCK_H
#include <sys/lock.h>
#endif
#ifdef HAVE_SYS_MUTEX_H
#include <sys/mutex.h>
#endif
#include <sys/vnode.h>
#include <sys/errno.h>
#include <sys/file.h>
#include <sys/namei.h>
#include <sys/dirent.h>
#include <sys/ucred.h>
#include <sys/selinfo.h>
#include <sys/uio.h>
#ifdef HAVE_SYS_POLL_H
#include <sys/poll.h>
#endif
#ifdef HAVE_SYS_SIGNALVAR_H
#include <sys/signalvar.h>
#endif
#ifdef HAVE_SYS_INTTYPES_H
#include <sys/inttypes.h>
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
#ifdef HAVE_MISCFS_SYNCFS_SYNCFS_H
#include <miscfs/syncfs/syncfs.h>
#endif
#ifndef HAVE_KERNEL_UVM_ONLY
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
#endif
#ifdef HAVE_UVM_UVM_EXTERN_H
#include <uvm/uvm_extern.h>
#endif
#ifdef HAVE_VM_UMA_H
#include <vm/uma.h>
#endif

#if defined(__APPLE__)
#include <machine/machine_routines.h>
#include <mach/machine/vm_types.h>
#include <sys/ubc.h>
void cache_purge(struct vnode *);
int cache_lookup(struct vnode *, struct vnode **, struct componentname *);
void cache_enter(struct vnode *, struct vnode *, struct componentname *);
void cache_purgevfs(struct mount *);
#endif

#define nnpfs_vop_read(t, uio, ioflag, cred, error) (error) = VOP_READ((t), (uio), (ioflag), (cred))
#define nnpfs_vop_write(t, uio, ioflag, cred, error) (error) = VOP_WRITE((t), (uio), (ioflag), (cred))
#define nnpfs_vop_getattr(t, attr, cred, proc, error) (error) = VOP_GETATTR((t), (attr), (cred), (proc))
#define nnpfs_vop_access(dvp, mode, cred, proc, error) (error) = VOP_ACCESS((dvp), (mode), (cred), (proc))

typedef u_quad_t va_size_t;

#endif /* !__osf__ */

#ifdef __FreeBSD_version
#if __FreeBSD_version < 400000
# error This version is unsupported
#elif __FreeBSD_version < 440001 || (__FreeBSD_version >= 500000 && __FreeBSD_version < 500023)
typedef struct proc d_thread_t;
#elif __FreeBSD_version == 500023
#   define HAVE_FREEBSD_THREAD
typedef struct thread d_thread_t;
#elif __FreeBSD_version >= 500024
#   define HAVE_FREEBSD_THREAD
#endif
typedef d_thread_t syscall_d_thread_t;
#define syscall_thread_to_thread(x) (x)
#else /* !__FreeBSD_version */
#if defined(__NetBSD__) && __NetBSD_Version__ >= 106130000
typedef struct lwp syscall_d_thread_t;
#define syscall_thread_to_thread(x) ((x)->l_proc)
#else
typedef struct proc syscall_d_thread_t;
#define syscall_thread_to_thread(x) (x)
#endif
typedef struct proc d_thread_t;
#endif /* !__FreeBSD_version */

#ifdef VV_ROOT
#define NNPFS_MAKE_VROOT(v) ((v)->v_vflag |= VV_ROOT) /* FreeBSD 5 */
#else
#define NNPFS_MAKE_VROOT(v) ((v)->v_flag |= VROOT)
#endif

#if defined(__NetBSD__) && __NetBSD_Version__ >= 105280000
#include <miscfs/genfs/genfs.h>
#include <miscfs/genfs/genfs_node.h>

struct genfs_ops nnpfs_genfsops;
#endif


#if defined(HAVE_FREEBSD_THREAD)
#define nnpfs_uio_to_thread(uiop) ((uiop)->uio_td)
#define nnpfs_cnp_to_thread(cnp) ((cnp)->cn_thread)
#define nnpfs_thread_to_cred(td) ((td)->td_proc->p_ucred)
#define nnpfs_thread_to_euid(td) ((td)->td_proc->p_ucred->cr_uid)
#else
#define nnpfs_uio_to_proc(uiop) ((uiop)->uio_procp)
#define nnpfs_cnp_to_proc(cnp) ((cnp)->cn_proc)
#define nnpfs_proc_to_cred(p) ((p)->p_ucred)
#define nnpfs_proc_to_euid(p) ((p)->p_ucred->cr_uid)
#endif

#if defined(__FreeBSD_version) && __FreeBSD_version >= 500043
extern const char *VT_AFS;
#endif

#if defined(__FreeBSD__)
typedef void * nnpfs_malloc_type;
#elif defined(__NetBSD__) && __NetBSD_Version__ >= 106140000 /* 1.6N */
typedef struct malloc_type * nnpfs_malloc_type;
#else
typedef int nnpfs_malloc_type;
#endif

#ifdef __APPLE__
#define nnpfs_curproc() (current_proc())
#else
#if defined(HAVE_FREEBSD_THREAD)
#define nnpfs_curthread() (curthread)
#else
#define nnpfs_curproc() (curproc)
#endif
#endif

#ifdef __osf__
#define nnpfs_pushdirty(vp, cred, p)
#else
void	nnpfs_pushdirty(struct vnode *, struct ucred *, d_thread_t *);
#endif


#if defined(HAVE_UINTPTR_T) /* c99 enviroment */
#define nnpfs_uintptr_t		uintptr_t
#else
#if defined(_LP64) || defined(alpha) || defined(__alpha__) || defined(__sparc64__) || defined(__sparcv9__)
#define nnpfs_uintptr_t		unsigned long long
#else /* !LP64 */
#define nnpfs_uintptr_t		unsigned long
#endif /* LP64 */
#endif

/*
 * XXX
 */

#ifndef SCARG
#if defined(__FreeBSD_version) && __FreeBSD_version >  500042
#define SCARG(a, b) ((a)->b)
#define syscallarg(x)   x
#else
#define SCARG(a, b) ((a)->b.datum)
#define syscallarg(x)   union { x datum; register_t pad; }
#endif /* __FreeBSD_version */
#endif /* SCARG */

#ifndef syscallarg
#define syscallarg(x)   x
#endif

#ifndef HAVE_REGISTER_T
typedef int register_t;
#endif

/* malloc(9) waits by default, freebsd post 5.0 choose to remove the flag */
#ifndef M_WAITOK
#define M_WAITOK 0
#endif

#if defined(HAVE_DEF_STRUCT_SETGROUPS_ARGS)
#define nnpfs_setgroups_args setgroups_args
#elif defined(HAVE_DEF_STRUCT_SYS_SETGROUPS_ARGS)
#define nnpfs_setgroups_args sys_setgroups_args
#elif __osf__
struct nnpfs_setgroups_args {
    syscallarg(int) gidsetsize;
    syscallarg(gid_t) *gidset;
};
#elif defined(__APPLE__)
struct nnpfs_setgroups_args{
        syscallarg(u_int)   gidsetsize;
        syscallarg(gid_t)   *gidset;
};
#else
#error what is you setgroups named ?
#endif


#ifdef HAVE_KERNEL_VFS_GETVFS
#define nnpfs_vfs_getvfs vfs_getvfs
#else
#define nnpfs_vfs_getvfs getvfs
#endif

#ifdef HAVE_FOUR_ARGUMENT_VFS_OBJECT_CREATE
#define nnpfs_vfs_object_create(vp,proc,ucred) vfs_object_create(vp,proc,ucred,TRUE)
#else
#define nnpfs_vfs_object_create(vp,proc,ucred) vfs_object_create(vp,proc,ucred)
#endif

#if  defined(UVM) || (defined(__NetBSD__) && __NetBSD_Version__ >= 105280000) || defined(__OpenBSD__)
#define nnpfs_set_vp_size(vp, sz) uvm_vnp_setsize(vp, sz)
#elif HAVE_KERNEL_VNODE_PAGER_SETSIZE
#define nnpfs_set_vp_size(vp, sz) vnode_pager_setsize(vp, sz)
#elif defined(__APPLE__)
#define nnpfs_set_vp_size(vp, sz) ubc_setsize(vp, sz)
#else
#define nnpfs_set_vp_size(vp, sz)
#endif

/* namei flag */
#ifdef LOCKLEAF
#define NNPFS_LOCKLEAF LOCKLEAF
#else
#define NNPFS_LOCKLEAF 0
#endif

#ifdef NEED_VGONEL_PROTO
void    vgonel (struct vnode *vp, d_thread_t *p);
#endif

#ifdef NEED_ISSIGNAL_PROTO
int	issignal (d_thread_t *);
#endif

#ifdef NEED_STRNCMP_PROTO
int	strncmp (const char *, const char *, size_t);
#endif

#ifdef NEED_VN_WRITECHK_PROTO
int	vn_writechk (struct vnode *);
#endif

#ifdef NEED_UBC_PUSHDIRTY_PROTO
int     ubc_pushdirty (struct vnode *);
#endif

#include <nnpfs/nnpfs_syscalls.h>

/* 
 *  The VOP table
 *
 *    What VOPs do we have today ? 
 */

#define NNPFS_VOP_DEF(n)	\
	struct vop_##n##_args; \
	int nnpfs_##n(struct vop_##n##_args *);

#include "nnpfs/nnpfs_vopdefs.h"
