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

/* $arla: xfs_syscalls.h,v 1.29 2003/01/19 20:53:54 lha Exp $ */

#ifndef  __xfs_syscalls
#define  __xfs_syscalls

#include <xfs/xfs_common.h>
#include <xfs/xfs_message.h>

#include <afssysdefs.h>

struct sys_pioctl_args {
    syscallarg(int) operation;
    syscallarg(char *) a_pathP;
    syscallarg(int) a_opcode;
    syscallarg(struct ViceIoctl *) a_paramsP;
    syscallarg(int) a_followSymlinks;
};

#define NNPFS_FHMAXDATA 40

struct xfs_fhandle_t {
    u_short	len;
    u_short	pad;
    char	fhdata[NNPFS_FHMAXDATA];
};

struct xfs_fh_args {
    syscallarg(fsid_t) fsid;
    syscallarg(long)   fileid;
    syscallarg(long)   gen;
};

int xfs_install_syscalls(void);
int xfs_uninstall_syscalls(void);
int xfs_stat_syscalls(void);
xfs_pag_t xfs_get_pag(struct ucred *);

int xfs_setpag_call(struct ucred **ret_cred);
int xfs_pioctl_call(d_thread_t *proc,
		    struct sys_pioctl_args *args,
		    register_t *return_value);

int xfspioctl(syscall_d_thread_t *proc, void *varg, register_t *retval);

int xfs_setgroups(syscall_d_thread_t *p, void *varg, register_t *retval);

extern int (*old_setgroups_func)(syscall_d_thread_t *, void *, register_t *);
extern int xfs_syscall_num; /* The old syscall number */


#ifndef HAVE_KERNEL_SYS_LKMNOSYS
#define sys_lkmnosys nosys
#endif

#ifndef SYS_MAXSYSCALL
#define SYS_MAXSYSCALL nsysent
#endif

#endif				       /* __xfs_syscalls */
