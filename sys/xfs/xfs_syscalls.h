/* $OpenBSD: xfs_syscalls.h,v 1.4 2000/03/03 00:54:58 todd Exp $ */
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


#ifndef  __xfs_syscalls
#define  __xfs_syscalls

#include <xfs/xfs_common.h>
#include <xfs/xfs_message.h>
#ifdef HAVE_SYS_SYSCALLARGS_H
#include <sys/syscallargs.h>
#endif

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

struct sys_pioctl_args {
    syscallarg(int) operation;
    syscallarg(char *) a_pathP;
    syscallarg(int) a_opcode;
    syscallarg(struct ViceIoctl *) a_paramsP;
    syscallarg(int) a_followSymlinks;
};

struct xfs_fh_args {
    syscallarg(fsid_t) fsid;
    syscallarg(long)   fileid;
    syscallarg(long)   gen;
};

int xfs_install_syscalls(void);
int xfs_uninstall_syscalls(void);
int xfs_stat_syscalls(void);
pag_t xfs_get_pag(struct ucred *);

int xfs_setpag_call(struct ucred **ret_cred);
int xfs_pioctl_call(struct proc *proc,
		    struct sys_pioctl_args *args,
		    register_t *return_value);

int sys_xfspioctl(struct proc *proc, void *varg, register_t *retval);

#endif				       /* __xfs_syscalls */
